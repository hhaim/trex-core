/*
 TRex team
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

#include "trex_astf_rx_core.h"
#include "utl_json.h"
#include "trex_watchdog.h"
#include "pkt_gen.h"
#include "common/basic_utils.h"


#define UPDATE_TIME_SEC (0.5)
#define MAX_CPS         (1000000.0)
#define ASTF_SYNC_TIME_OUT_SEC (1.0)

int CRxAstfPort::tx(rte_mbuf_t *m){
   return(m_rx->tx_pkt(m, m_port_id)?0:-1);
}

                                           
bool TrexRxStartLatency::handle(CRxCore *rx_core){
    CRxAstfCore * lp=(CRxAstfCore *)rx_core;
    lp->start_latency(this);
    return(true);
}


bool TrexRxStopLatency::handle(CRxCore *rx_core){
    CRxAstfCore * lp=(CRxAstfCore *)rx_core;
    lp->stop_latency();
    return(true);
}



CRxAstfCore::CRxAstfCore(void) : CRxCore() {
    m_active_context =false;
    m_rx_dp = CMsgIns::Ins()->getRxDp();
    m_epoc = 0x17;
    m_start_time=0;
    m_delta_sec=0.0;
    m_port_ids.clear();
    m_port_mask=0;
    m_max_ports=0;
    m_latency_active =false;
    int i;
    for (i=0; i<TREX_MAX_PORTS; i++) {
        m_io_ports[i].Create(this,(uint8_t)i);
    }
    m_l_pkt_mode = 0;
}


void CRxAstfCore::do_background(void){
    bool did_something = work_tick();
    if (likely( did_something || m_latency_active )) {
        delay_lowend();
    } else {
        rte_pause_or_delay_lowend();
    }
}


int CRxAstfCore::_do_start(void){

    double n_time;
    CGenNode * node = new CGenNode();
    node->m_type = CGenNode::FLOW_SYNC;   /* general stuff */
    node->m_time = now_sec()+0.007;
    m_p_queue.push(node);
    int cnt=0;

    while (  !m_p_queue.empty() ) {
        node = m_p_queue.top();
        n_time = node->m_time;
        bool restart=true;
        /* wait for event */
        while ( true ) {
            double dt = now_sec() - n_time ;
            if (dt> (0.0)) {
                break;
            }
            do_background();
        }
        m_cpu_dp_u.start_work1();
        // re-read it as background might add new nodes 
        node = m_p_queue.top();
        n_time = node->m_time;

        m_p_queue.pop();
        switch (node->m_type) {
        case CGenNode::FLOW_SYNC:
            tickle();
            node->m_time += ASTF_SYNC_TIME_OUT_SEC;
            break;
        case CGenNode::FLOW_PKT:
            node->m_time += m_delta_sec;
            if (!send_pkt_all_ports() && (node->m_pad2==m_epoc) ){
            }else{
                restart=false;
            } 
            break;
        case  CGenNode::TW_SYNC:
            /* this might affect latency performance, we should keep this very light */
            node->m_time += UPDATE_TIME_SEC;
            if (m_latency_active && node->m_pad2==m_epoc){
                update_stats();
                cnt++;
#ifdef LATENCY_DEBUG
                ///if (cnt%10==0) {
                   //cp_dump(stdout);
                //}
#endif
            }else{
                restart=false;
            }
            break;

        default:
            assert(0);
        }

        /* put the node again into the queue */
        if (restart) {
            m_p_queue.push(node);
        }else{
            delete node;
        }

        m_cpu_dp_u.commit1();

        if (m_state == STATE_QUIT){
            break;
        }
    }

    /* free all nodes in the queue */
    while (!m_p_queue.empty()) {
        node = m_p_queue.top();
        m_p_queue.pop();
        delete node;
    }
    return (0);
}

void CRxAstfCore::handle_astf_latency_pkt(const rte_mbuf_t *m,
                                      uint8_t port_id){
    /* nothing to do */
    if (!m_latency_active)
        return;
    CLatencyManagerPerPort * lp_port = &m_ports[port_id];
    handle_rx_pkt(lp_port,(rte_mbuf_t *)m);
}


bool CRxAstfCore::work_tick(void) {
    bool did_something = CRxCore::work_tick();
    did_something |= handle_msg_packets();
    return did_something;
}

/* check for messages from DP cores */
uint32_t CRxAstfCore::handle_msg_packets(void) {
    uint32_t pkts = 0;
    for (uint8_t thread_id=0; thread_id<m_tx_cores; thread_id++) {
        CNodeRing *r = m_rx_dp->getRingDpToCp(thread_id);
        pkts += handle_rx_one_queue(thread_id, r);
    }
    return pkts;
}

uint32_t CRxAstfCore::handle_rx_one_queue(uint8_t thread_id, CNodeRing *r) {
    CGenNode * node;
    uint32_t got_pkts;
    CFlowGenListPerThread* lpt = get_platform_api().get_fl()->m_threads_info[thread_id];
    uint8_t port1, port2;
    lpt->get_port_ids(port1, port2);
    RXPortManager* rx_mngr[2] = {m_rx_port_mngr[port1], m_rx_port_mngr[port2]};

    for ( got_pkts=0; got_pkts<64; got_pkts++ ) { // read 64 packets at most
        if ( r->Dequeue(node)!=0 ){
            break;
        }
        assert(node);

        CGenNodeLatencyPktInfo *pkt_info = (CGenNodeLatencyPktInfo *) node;
        assert(pkt_info->m_msg_type==CGenNodeMsgBase::LATENCY_PKT);
        assert(pkt_info->m_latency_offset==0xdead);
        uint8_t dir = pkt_info->m_dir & 1;
        rx_mngr[dir]->handle_pkt((rte_mbuf_t *)pkt_info->m_pkt);

        CGlobalInfo::free_node(node);
    }
    return got_pkts;
}


void CRxAstfCore::start_latency(TrexRxStartLatency * msg){

    /* create a node */
    assert(m_latency_active ==false);
    enable_astf_latency_fia(true);
    m_epoc++;

    if (m_active_context) {
        /* lazy delete, so we would be able to read the stats*/
        delete_context();
        m_active_context=false;
    }

    uint8_t pkt_type = msg->m_pkt_type;

    switch (pkt_type) {
    default:
    case 0:
        m_l_pkt_mode = (CLatencyPktModeSCTP *) new CLatencyPktModeSCTP(CGlobalInfo::m_options.get_l_pkt_mode());
        break;
    case 1:
    case 2:
    case 3:
        m_l_pkt_mode =  (CLatencyPktModeICMP *) new CLatencyPktModeICMP(CGlobalInfo::m_options.get_l_pkt_mode());
        break;
    }

    double cps= msg->m_cps;
    if (cps <= 1.0) {
        m_delta_sec = 1.0;
    } else {
        if (cps>MAX_CPS){
            cps=MAX_CPS;
        }
        m_delta_sec =(1.0/cps);
    }

    m_max_ports = msg->m_max_ports;
    assert (m_max_ports <= TREX_MAX_PORTS);
    assert ((m_max_ports%2)==0);
    m_port_mask =0xffffffff;
    m_pkt_gen.Create(m_l_pkt_mode);
    for (int i=0; i<m_max_ports; i++) {
        CLatencyManagerPerPort * lp=&m_ports[i];
        CCPortLatency * lpo=&m_ports[dual_port_pair(i)].m_port;

        lp->m_io= &m_io_ports[i];
        lp->m_port.Create(i,
                          m_pkt_gen.get_payload_offset(),
                          m_pkt_gen.get_l4_offset(),
                          m_pkt_gen.get_pkt_size(),lpo,
                          m_l_pkt_mode,
                          0 );
        lp->m_port.set_enable_none_latency_processing(false);
        lp->m_port.m_hist.set_hot_max_cnt((int(cps)/2));

        if ( !CGlobalInfo::m_options.m_dummy_port_map[i] ) {
            m_port_ids.push_back(i);
        }
    }
    m_pkt_gen.set_ip(msg->m_client_ip.v4,
                     msg->m_server_ip.v4,
                     msg->m_dual_port_mask);


    m_active_context =true;

    CGenNode * node;
    node = new CGenNode();
    node->m_type = CGenNode::FLOW_PKT;   /* general stuff */
    node->m_time = now_sec()+0.01;
    node->m_pad2 = m_epoc;
    m_p_queue.push(node);

    node = new CGenNode();

    node->m_type = CGenNode::TW_SYNC;   /* update stats node, every 0.5 sec */
    node->m_time = now_sec()+0.02;
    node->m_pad2 = m_epoc;
    m_p_queue.push(node);
    

    /* make sure we are in NAT mode, this is due to old code that check this variables to work */
    CGlobalInfo::m_options.m_l_pkt_mode = L_PKT_SUBMODE_REPLY;
    CGlobalInfo::is_learn_mode(CParserOption::LEARN_MODE_TCP_ACK);
    m_latency_active =true;
}


void CRxAstfCore::delete_context(){
        
    m_pkt_gen.Delete();

    for (int i=0; i<m_max_ports; i++) {
        CLatencyManagerPerPort * lp=&m_ports[i];
        lp->m_port.Delete();
    }
    m_port_ids.clear();
    if (m_l_pkt_mode) {
        delete m_l_pkt_mode;
        m_l_pkt_mode =0;
    }
}

void CRxAstfCore::stop_latency(){
    m_latency_active = false;
}


void CRxAstfCore::handle_rx_pkt(CLatencyManagerPerPort * lp,
                               rte_mbuf_t * m){
    CRx_check_header *rxc = NULL;

#ifdef LATENCY_DEBUG
    fprintf(stdout,"RX --- \n");
    /****************************************/
    uint8_t *p=rte_pktmbuf_mtod(m, uint8_t*);
    uint16_t pkt_size=rte_pktmbuf_pkt_len(m);
    utl_k12_pkt_format(stdout,p ,pkt_size) ;
    /****************************************/
#endif 

    lp->m_port.check_packet(m,rxc);
    if ( unlikely(rxc!=NULL) ){
        /* not supported in this mode */
        assert(0);
    }
}


bool  CRxAstfCore::send_pkt_all_ports(){
    if (!m_latency_active){
        return(true);
    }
    m_start_time = os_get_hr_tick_64();
    for (auto &i: m_port_ids) {
        if ( m_port_mask & (1<<i)  ){
            CLatencyManagerPerPort * lp=&m_ports[i];
            if (lp->m_port.can_send_packet(i%2) ){
                rte_mbuf_t * m=m_pkt_gen.generate_pkt(i,lp->m_port.external_nat_ip(),
                                                        lp->m_port.external_dest_ip());
                lp->m_port.update_packet(m, i);

#ifdef LATENCY_DEBUG
                fprintf(stdout,"TX --- \n");
                /****************************************/
                uint8_t *p=rte_pktmbuf_mtod(m, uint8_t*);
                uint16_t pkt_size=rte_pktmbuf_pkt_len(m);
                utl_k12_pkt_format(stdout,p ,pkt_size) ;
                /****************************************/
#endif


                if ( lp->m_io->tx_latency(m) == 0 ){
                    lp->m_port.m_tx_pkt_ok++;
                }else{
                    lp->m_port.m_tx_pkt_err++;
                    rte_pktmbuf_free(m);
                }

            }
        }
    }
    return (false);
}

/* this function will be called from CP !! watchout from shared variables */
void CRxAstfCore::cp_dump(FILE *fd){

    fprintf(fd," Cpu Utilization : %2.1f %%  \n",m_cpu_cp_u.GetVal());
    if (!m_active_context) {
        fprintf(fd," no active context  \n");
        return;
    }

    CCPortLatency::DumpShortHeader(fd);
    for (auto &i: m_port_ids) {
        fprintf(fd," %d | ",i);
        CLatencyManagerPerPort * lp=&m_ports[i];
        lp->m_port.DumpShort(fd);
        fprintf(fd,"\n");
    }
}

void CRxAstfCore::update_stats(){
    assert(m_active_context);
    for (auto &i: m_port_ids) {
        CLatencyManagerPerPort * lp=&m_ports[i];
        lp->m_port.m_hist.update();
    }
}

void CRxAstfCore::cp_get_json(std::string & json){
    json="{\"name\":\"trex-latecny-v2\",\"type\":0,\"data\":{";
    json+=add_json("cpu_util",m_cpu_cp_u.GetVal());
    if (!m_active_context) {
        return;
    }

    for (auto &i: m_port_ids) {
        CLatencyManagerPerPort * lp=&m_ports[i];
        lp->m_port.dump_json_v2(json);
    }

    json+="\"unknown\":0}}"  ;
}



