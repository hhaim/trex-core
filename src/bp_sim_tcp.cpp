/*
 Hanoh Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2015 Cisco Systems, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "bp_sim.h"
#include <common/utl_gcc_diag.h>
#include <common/basic_utils.h>
#include "h_timer.h"
#include <cmath>
#include "utl_mbuf.h"

#include "44bsd/tcp.h"
#include "44bsd/tcp_var.h"
#include "44bsd/tcp.h"
#include "44bsd/tcp_fsm.h"
#include "44bsd/tcp_seq.h"
#include "44bsd/tcp_timer.h"
#include "44bsd/tcp_socket.h"
#include "44bsd/tcpip.h"
#include "44bsd/tcp_dpdk.h"
#include "44bsd/flow_table.h"

#include "mbuf.h"
#include <stdlib.h>
#include <common/c_common.h>

#undef DEBUG_TX_PACKET
                                          
class CTcpDpdkCb : public CTcpCtxCb {
public:
   int on_tx(CTcpPerThreadCtx *ctx,
             struct tcpcb * tp,
             rte_mbuf_t *m);

   int on_flow_close(CTcpPerThreadCtx *ctx,
                     CTcpFlow * flow);

public:
    uint8_t                 m_dir;
    CFlowGenListPerThread * m_p;
};


int CTcpDpdkCb::on_flow_close(CTcpPerThreadCtx *ctx,
                              CTcpFlow * flow){
    uint32_t   c_idx;
    uint16_t   c_pool_idx;
    uint16_t   c_template_id;
    bool       enable;


    if (ctx->m_ft.is_client_side() == false) {
        /* nothing to do, flow ports was allocated by client */
        return(0);
    }
    m_p->m_stats.m_total_close_flows +=1;

    flow->get_tuple_generator(c_idx,c_pool_idx,c_template_id,enable);
    assert(enable==true); /* all flows should have tuple generator */

    CFlowGeneratorRecPerThread * cur = m_p->m_cap_gen[c_template_id];
    CTupleGeneratorSmart * lpgen=cur->tuple_gen.get_gen();
    if ( lpgen->IsFreePortRequired(c_pool_idx) ){
        lpgen->FreePort(c_pool_idx,c_idx,flow->m_tcp.src_port);
    }
    return(0);
}


int CTcpDpdkCb::on_tx(CTcpPerThreadCtx *ctx,
                      struct tcpcb * tp,
                      rte_mbuf_t *m){
    CNodeTcp node_tcp;
    node_tcp.dir  = m_dir;
    node_tcp.mbuf = m;

#ifdef DEBUG_TX_PACKET
     fprintf(stdout,"TX---> dir %d \n",m_dir);
     utl_rte_pktmbuf_dump_k12(stdout,m);
#endif
    
    m_p->m_node_gen.m_v_if->send_node((CGenNode *) &node_tcp);
    return(0);
}


void CFlowGenListPerThread::tcp_handle_rx_flush(CGenNode * node,
                                                bool on_terminate){

    m_node_gen.m_p_queue.pop();
    if ( on_terminate == false ){
        node->m_time += TCP_RX_FLUSH_SEC;
        m_node_gen.m_p_queue.push(node);
    }else{
        free_node(node);
    }

    CVirtualIF * v_if=m_node_gen.m_v_if;
    rte_mbuf_t * rx_pkts[64];
    int dir;
    uint16_t cnt;

    CTcpPerThreadCtx  * mctx_dir[2]={
        m_c_tcp,
        m_s_tcp
    };
    
    for (dir=0; dir<CS_NUM; dir++) {
        CTcpPerThreadCtx  * ctx=mctx_dir[dir];
        while (true) {
            cnt=v_if->rx_burst(dir,rx_pkts,64);
            if (cnt==0) {
                break;
            }
            int i;
            for (i=0; i<(int)cnt;i++) {
                rte_mbuf_t * m=rx_pkts[i];

#ifdef DEBUG_TX_PACKET
                fprintf(stdout,"RX---> dir %d \n",dir);
                utl_rte_pktmbuf_dump_k12(stdout,m);
#endif
                ctx->m_ft.rx_handle_packet(ctx,m);
            }
        }
    }
}

static CTcpAppApiImpl     m_tcp_bh_api_impl_c;


void CFlowGenListPerThread::tcp_generate_flows_roundrobin(bool &done){

    done=false;
    /* TBD need to scan the vector of template .. for now we have one template */

    CFlowGeneratorRecPerThread * cur;
    uint8_t template_id=0; /* TBD take template zero */

    cur=m_cap_gen[template_id]; /* first template TBD need to fix this */


    CTupleBase  tuple;
    cur->tuple_gen.GenerateTuple(tuple);
    /* it is not set by generator, need to take it from the pcap file */
    tuple.setServerPort(80) ;

    /* TBD this include an information on the client/vlan/destination */ 
    /*ClientCfgBase * lpc=tuple.getClientCfg(); */

    /* TBD set the tuple */
    CTcpFlow * c_flow = m_c_tcp->m_ft.alloc_flow(m_c_tcp,
                                                 tuple.getClient(),
                                                 tuple.getServer(),
                                                 tuple.getClientPort(),
                                                 tuple.getServerPort() ,
                                                 false);
    if (c_flow == (CTcpFlow *)0) {
        return;
    }

    /* save tuple generator information into the flow */
    c_flow->set_tuple_generator(tuple.getClientId(), 
                                cur->m_info->m_client_pool_idx,
                                template_id,
                                true);

    /* update default mac addrees, dir is zero client side  */
    m_node_gen.m_v_if->update_mac_addr_from_global_cfg(CLIENT_SIDE,c_flow->m_tcp.template_pkt);

    CFlowKeyTuple   c_tuple;
    c_tuple.set_ip(tuple.getClient());
    c_tuple.set_port(tuple.getClientPort());
    c_tuple.set_proto(6);
    c_tuple.set_ipv4(true);

    if (!m_c_tcp->m_ft.insert_new_flow(c_flow,c_tuple)){
        /* need to free the tuple */
        m_c_tcp->m_ft.handle_close(m_c_tcp,c_flow,false);
        return;
    }

    CTcpApp * app_c;

    app_c = &c_flow->m_app;

    app_c->set_program(m_prog_c);
    app_c->set_bh_api(&m_tcp_bh_api_impl_c);
    app_c->set_flow_ctx(m_c_tcp,c_flow);
    c_flow->set_app(app_c);

    /* start connect */
    app_c->start(true);
    tcp_connect(m_c_tcp,&c_flow->m_tcp);

    m_stats.m_total_open_flows += 1;
}

void CFlowGenListPerThread::tcp_handle_tx_fif(CGenNode * node,
                                              bool on_terminate){
    bool done;
    m_node_gen.m_p_queue.pop();
    if ( on_terminate == false ) {
        m_cur_time_sec = node->m_time ;

        tcp_generate_flows_roundrobin(done);

        if (!done) {
            node->m_time += m_tcp_fif_d_time;
            m_node_gen.m_p_queue.push(node);
        }else{
            free_node(node);
        }
    }else{
        free_node(node);
    }



}

void CFlowGenListPerThread::tcp_handle_tw(CGenNode * node,
                                          bool on_terminate){

    m_node_gen.m_p_queue.pop();
    if ( on_terminate == false ){
        node->m_time += tcp_get_tw_tick_in_sec();
        m_node_gen.m_p_queue.push(node);
    }else{
        free_node(node);
    }

    CTcpPerThreadCtx  * mctx_dir[2]={
        m_c_tcp,
        m_s_tcp
    };

    int dir;
    for (dir=0; dir<CS_NUM; dir++) {
        CTcpPerThreadCtx  * ctx=mctx_dir[dir];
        ctx->timer_w_on_tick();
    }
}


double CFlowGenListPerThread::tcp_get_tw_tick_in_sec(){
    return((double)TCP_TIMER_W_TICK/((double)TCP_TIMER_W_DIV* 1000.0));
}


bool CFlowGenListPerThread::Create_tcp(){

    m_c_tcp = new CTcpPerThreadCtx();
    m_s_tcp = new CTcpPerThreadCtx();

    CTcpDpdkCb * c_tcp_io = new CTcpDpdkCb();
    CTcpDpdkCb * s_tcp_io = new CTcpDpdkCb();

    c_tcp_io->m_dir =0;
    c_tcp_io->m_p   = this;
    s_tcp_io->m_dir =1;
    s_tcp_io->m_p   = this;

    m_c_tcp_io =c_tcp_io;
    m_s_tcp_io =s_tcp_io;

    m_c_tcp->Create(10000,true);
    m_c_tcp->set_cb(m_c_tcp_io);
    
    m_s_tcp->Create(10000,false);
    m_s_tcp->set_cb(m_s_tcp_io);

    uint32_t tx_num_bytes=500*1024;

    m_buf = new CMbufBuffer();
    m_prog_c = new CTcpAppProgram();
    m_prog_s = new CTcpAppProgram();
    utl_mbuf_buffer_create_and_fill(m_buf,2048,tx_num_bytes);

    CTcpAppCmd cmd;

    /* PER FLOW  */

    /* client program */
    cmd.m_cmd =tcTX_BUFFER;
    cmd.u.m_tx_cmd.m_buf =m_buf;

    m_prog_c->add_cmd(cmd);


    /* server program */

    cmd.m_cmd =tcRX_BUFFER;
    cmd.u.m_rx_cmd.m_flags =CTcpAppCmdRxBuffer::rxcmd_WAIT;
    cmd.u.m_rx_cmd.m_rx_bytes_wm = tx_num_bytes;

    m_prog_s->add_cmd(cmd);

    m_s_tcp->m_ft.set_tcp_api(&m_tcp_bh_api_impl_c);
    m_s_tcp->m_ft.set_tcp_program(m_prog_s);


    return(true);
}

void CFlowGenListPerThread::Delete_tcp(){

    delete m_prog_c;
    delete m_prog_s;

    m_buf->Delete();
    delete m_buf;

    m_c_tcp->Delete();
    m_s_tcp->Delete();

    delete m_c_tcp;
    delete m_s_tcp;

    CTcpDpdkCb * c_tcp_io = (CTcpDpdkCb *)m_c_tcp_io;
    CTcpDpdkCb * s_tcp_io = (CTcpDpdkCb *)m_s_tcp_io;
    delete c_tcp_io;
    delete s_tcp_io;
}

