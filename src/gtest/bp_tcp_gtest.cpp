/*
 Hanoh Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2017 Cisco Systems, Inc.

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

#include <common/gtest.h>
#include <common/basic_utils.h>
#include "h_timer.h"
#include <common/utl_gcc_diag.h>
#include <cmath>

#include "44bsd/tcp.h"
#include "44bsd/tcp_var.h"
#include "44bsd/tcp.h"
#include "44bsd/tcp_fsm.h"
#include "44bsd/tcp_seq.h"
#include "44bsd/tcp_timer.h"
#include "44bsd/tcp_socket.h"
#include "44bsd/tcpip.h"
#include "44bsd/tcp_dpdk.h"
#include "mbuf.h"
#include <stdlib.h>


class gt_tcp  : public testing::Test {

protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }
public:
};







TEST_F(gt_tcp, tst2) {
    CTcpPerThreadCtx tcp_ctx;
    CTcpFlow   flow;
    flow.Create(&tcp_ctx);

    tcp_ctx.Create();
    tcp_ctx.timer_w_start(&flow);
    int i;
    for (i=0; i<40; i++) {
        printf(" tick %d \n",i);
        tcp_ctx.timer_w_on_tick();
    }
    tcp_ctx.Delete();
}


TEST_F(gt_tcp, tst1) {
#if 0
    CTcpFlow   flow;
    flow.Create();
    flow.on_tick();
    flow.on_tick();
    flow.Delete();
#endif
}



TEST_F(gt_tcp, tst3) {
    printf(" MSS %d \n",(int)TCP_MSS);
    printf(" sizeof_tcpcb %d \n",(int)sizeof(tcpcb));
    tcpstat tcp_stats;
    tcp_stats.Clear();
    tcp_stats.m_sts.tcps_accepts++;
    tcp_stats.m_sts.tcps_connects++;
    tcp_stats.m_sts.tcps_connects++;
    tcp_stats.Dump(stdout);
}

TEST_F(gt_tcp, tst4) {
    tcp_socket socket;
    socket.so_options=0;
    printf(" %d \n", (int)sizeof(socket));
}




class CTcpReassTest  {

public:
    bool Create();
    void Delete();
    void add_pkts(vec_tcp_reas_t & list);
    void expect(vec_tcp_reas_t & list,FILE *fd);

public:
    bool                    m_verbose;

    CTcpPerThreadCtx        m_ctx;
    CTcpReass               m_tcp_res;
    CTcpFlow                m_flow;
};

bool CTcpReassTest::Create(){
    m_ctx.Create();
    m_flow.Create(&m_ctx);
    return(true);
}

void CTcpReassTest::Delete(){
    m_flow.Delete();
    m_ctx.Delete();
}

void CTcpReassTest::add_pkts(vec_tcp_reas_t & lpkt){
    int i;
    tcpiphdr ti;

    for (i=0; i<lpkt.size(); i++) {
        ti.ti_seq  = lpkt[i].m_seq;
        ti.ti_len  = (uint16_t)lpkt[i].m_len;
        ti.ti_flags =lpkt[i].m_flags;
        m_tcp_res.pre_tcp_reass(&m_ctx,&m_flow.m_tcp,&ti,(struct rte_mbuf *)0);
        if (m_verbose) {
            fprintf(stdout," inter %d \n",i);
            m_tcp_res.Dump(stdout);
        }
    }
}

void CTcpReassTest::expect(vec_tcp_reas_t & lpkt,FILE *fd){
    m_tcp_res.expect(lpkt,fd);
}


TEST_F(gt_tcp, tst5) {
    vec_tcp_reas_t in_pkts = { {.m_seq=1000,.m_len=20,.m_flags=0},
                               {.m_seq=2000,.m_len=20,.m_flags=0} };

    vec_tcp_reas_t out_pkts={ {.m_seq=1000,.m_len=20,.m_flags=0},
                              {.m_seq=2000,.m_len=20,.m_flags=0}
    };

    CTcpReassTest tst;
    tst.Create();
    tst.m_verbose=true;
    tst.add_pkts(in_pkts);
    tst.expect(out_pkts,stdout);
    tst.m_ctx.m_tcpstat.Dump(stdout);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvoopack,2);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvoobyte,40);

    tst.Delete();
}





TEST_F(gt_tcp, tst6) {

    vec_tcp_reas_t in_pkts = { {.m_seq=1000,.m_len=20,.m_flags=0},
                               {.m_seq=1000,.m_len=20,.m_flags=0} };

    vec_tcp_reas_t out_pkts={ {.m_seq=1000,.m_len=20,.m_flags=0}
                              
    };

    CTcpReassTest tst;
    tst.Create();
    tst.m_verbose=true;
    tst.add_pkts(in_pkts);
    tst.expect(out_pkts,stdout);
    tst.m_ctx.m_tcpstat.Dump(stdout);

    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvduppack,1);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvdupbyte,20);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvoopack,2);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvoobyte,40);
    tst.Delete();

}

TEST_F(gt_tcp, tst7) {

    vec_tcp_reas_t in_pkts = { {.m_seq=1000,.m_len=20,.m_flags=0},
                               {.m_seq=1001,.m_len=20,.m_flags=0} };

    vec_tcp_reas_t out_pkts={ {.m_seq=1000,.m_len=21,.m_flags=0}

    };

    CTcpReassTest tst;
    tst.Create();
    tst.m_verbose=true;
    tst.add_pkts(in_pkts);
    tst.expect(out_pkts,stdout);
    tst.m_ctx.m_tcpstat.Dump(stdout);

    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvpartduppack,1);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvpartdupbyte,1);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvoopack,2);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvoobyte,40);

    tst.Delete();
}

TEST_F(gt_tcp, tst8) {

    vec_tcp_reas_t in_pkts = { {.m_seq=1000,.m_len=20,.m_flags=0},
                               {.m_seq=2000,.m_len=20,.m_flags=0},
                               {.m_seq=900,.m_len=3000,.m_flags=0} };

    vec_tcp_reas_t out_pkts={ {.m_seq=900,.m_len=3000,.m_flags=0}

    };

    CTcpReassTest tst;
    tst.Create();
    tst.m_verbose=true;
    tst.add_pkts(in_pkts);
    tst.expect(out_pkts,stdout);
    tst.m_ctx.m_tcpstat.Dump(stdout);

    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvduppack,2);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvdupbyte,40);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvoopack,3);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvoobyte,3040);

    tst.Delete();
}

TEST_F(gt_tcp, tst9) {

    vec_tcp_reas_t in_pkts = { {.m_seq=1000,.m_len=20,.m_flags=TH_FIN},
                               {.m_seq=2000,.m_len=20,.m_flags=0},
                               {.m_seq=2000,.m_len=21,.m_flags=0} };

    vec_tcp_reas_t out_pkts={ {.m_seq=1000,.m_len=20,.m_flags=TH_FIN},
                              {.m_seq=2000,.m_len=21,.m_flags=0}

    };

    CTcpReassTest tst;
    tst.Create();
    tst.m_verbose=true;
    tst.add_pkts(in_pkts);
    tst.expect(out_pkts,stdout);
    tst.m_ctx.m_tcpstat.Dump(stdout);


    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvpartduppack,1);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvpartdupbyte,1);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvoopack,3);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvoobyte,61);
    tst.Delete();
}

TEST_F(gt_tcp, tst10) {

    vec_tcp_reas_t in_pkts = { {.m_seq=1000,.m_len=20,.m_flags=0},
                               {.m_seq=2000,.m_len=20,.m_flags=0},
                               {.m_seq=3000,.m_len=20,.m_flags=0},
                               {.m_seq=4000,.m_len=20,.m_flags=0},
                               {.m_seq=5000,.m_len=20,.m_flags=0},

                                };

    vec_tcp_reas_t out_pkts={ {.m_seq=1000,.m_len=20,.m_flags=0},
                               {.m_seq=2000,.m_len=20,.m_flags=0},
                               {.m_seq=3000,.m_len=20,.m_flags=0},
                               {.m_seq=4000,.m_len=20,.m_flags=0},
    };

    CTcpReassTest tst;
    tst.Create();
    tst.m_verbose=true;
    tst.add_pkts(in_pkts);
    tst.expect(out_pkts,stdout);
    tst.m_ctx.m_tcpstat.Dump(stdout);

    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvoopack,5);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvoobyte,100);
    EXPECT_EQ(tst.m_ctx.m_tcpstat.m_sts.tcps_rcvoopackdrop,1);
    tst.Delete();
}


TEST_F(gt_tcp, tst11) {

    vec_tcp_reas_t in_pkts = { {.m_seq=1000,.m_len=20,.m_flags=0},
                               {.m_seq=1020,.m_len=20,.m_flags=0},
                               {.m_seq=1040,.m_len=20,.m_flags=0}

                                };

    vec_tcp_reas_t out_pkts={ {.m_seq=1000,.m_len=60,.m_flags=0},
    };

    CTcpReassTest tst;
    tst.Create();
    tst.m_verbose=true;
    tst.add_pkts(in_pkts);
    tst.expect(out_pkts,stdout);
    tst.m_ctx.m_tcpstat.Dump(stdout);
    tst.Delete();
}

TEST_F(gt_tcp, tst12) {

    vec_tcp_reas_t in_pkts = { {.m_seq=1000,.m_len=20,.m_flags=0},
                               {.m_seq=1020,.m_len=20,.m_flags=0},
                               {.m_seq=1040,.m_len=20,.m_flags=0},
                                };

    vec_tcp_reas_t out_pkts={ {.m_seq=1000,.m_len=60,.m_flags=0},
    };

    CTcpReassTest tst;
    tst.Create();
    tst.m_flow.m_tcp.rcv_nxt =500;
    tst.m_flow.m_tcp.t_state =TCPS_ESTABLISHED;

    tst.m_verbose=true;
    tst.add_pkts(in_pkts);
    tst.expect(out_pkts,stdout);

    tcpiphdr ti;

    ti.ti_seq  = 500;
    ti.ti_len  = 500;
    ti.ti_flags =0;

    tst.m_tcp_res.tcp_reass(&tst.m_ctx,&tst.m_flow.m_tcp,&ti,(struct rte_mbuf *)0);

    EXPECT_EQ(tst.m_tcp_res.get_active_blocks(),0);
    EXPECT_EQ(tst.m_flow.m_tcp.rcv_nxt,500+560);

    tst.m_ctx.m_tcpstat.Dump(stdout);
    tst.Delete();
}


void set_mbuf_test(struct rte_mbuf  *m,
                   uint32_t len,
                   uintptr_t addr){
    memset(m,0,sizeof(struct rte_mbuf));
    /* one segment */
    m->nb_segs=1;
    m->pkt_len  =len;
    m->data_len=len;
    m->buf_len=1024*2+128;
    m->data_off=128;
    m->refcnt_reserved=1;
    m->buf_addr=(void *)addr;
}



TEST_F(gt_tcp, tst13) {

    CMbufBuffer buf;

    struct rte_mbuf  mbuf0;
    struct rte_mbuf  mbuf1;
    struct rte_mbuf  mbuf2;

    uintptr_t addr1=0x1000;
    uint32_t blk_size=1024;

    buf.Create(blk_size);

    set_mbuf_test(&mbuf0,blk_size,addr1);
    addr1+=1024;
    set_mbuf_test(&mbuf1,blk_size,addr1);
    addr1+=1024;
    set_mbuf_test(&mbuf2,blk_size-10,addr1);

    CMbufObject  mbuf_obj;

    mbuf_obj.m_mbuf=&mbuf0;
    mbuf_obj.m_type=MO_CONST;

    buf.Add_mbuf(mbuf_obj);

    mbuf_obj.m_mbuf=&mbuf1;
    buf.Add_mbuf(mbuf_obj);

    mbuf_obj.m_mbuf=&mbuf2;
    buf.Add_mbuf(mbuf_obj);


    buf.Verify();

    CBufMbufRef res;

    buf.get_by_offset(0,res);
    EXPECT_EQ(res.m_offset,0);
    EXPECT_EQ(res.m_mbuf,&mbuf0);
    EXPECT_EQ(res.get_mbuf_size(),blk_size);
    EXPECT_EQ(res.need_indirect_mbuf(blk_size),false);

    buf.get_by_offset(1,res);
    EXPECT_EQ(res.m_offset,1);
    EXPECT_EQ(res.m_mbuf,&mbuf0);
    EXPECT_EQ(res.get_mbuf_size(),blk_size-1);
    EXPECT_EQ(res.need_indirect_mbuf(blk_size),true);

    buf.get_by_offset(2048+2,res);
    EXPECT_EQ(res.m_offset,2);
    EXPECT_EQ(res.m_mbuf,&mbuf2);
    EXPECT_EQ(res.get_mbuf_size(),blk_size-2-10);
    EXPECT_EQ(res.need_indirect_mbuf(blk_size),true);


    buf.get_by_offset(2048+1024-11,res);
    EXPECT_EQ(res.m_offset,1024-11);
    EXPECT_EQ(res.m_mbuf,&mbuf2);
    EXPECT_EQ(res.get_mbuf_size(),1);
    EXPECT_EQ(res.need_indirect_mbuf(blk_size),true);


    buf.Dump(stdout);
    //buf.Delete();
}

TEST_F(gt_tcp, tst14) {

    CMbufBuffer buf;

    struct rte_mbuf  mbuf0;
    struct rte_mbuf  mbuf1;
    struct rte_mbuf  mbuf2;

    uintptr_t addr1=0x1000;
    uint32_t blk_size=1024;

    buf.Create(blk_size);

    set_mbuf_test(&mbuf0,blk_size,addr1);
    addr1+=1024;
    set_mbuf_test(&mbuf1,blk_size,addr1);
    addr1+=1024;
    set_mbuf_test(&mbuf2,blk_size-10,addr1);

    CMbufObject  mbuf_obj;

    mbuf_obj.m_mbuf=&mbuf0;
    mbuf_obj.m_type=MO_CONST;

    buf.Add_mbuf(mbuf_obj);

    mbuf_obj.m_mbuf=&mbuf1;
    buf.Add_mbuf(mbuf_obj);

    mbuf_obj.m_mbuf=&mbuf2;
    buf.Add_mbuf(mbuf_obj);

    buf.Verify();

    buf.Dump(stdout);
    //buf.Delete();
}


TEST_F(gt_tcp, tst15) {

    CTcpApp app;

    CTcpSockBuf  tx_sock;
    tx_sock.Create(8*1024);
    tx_sock.m_app=&app;

    CMbufBuffer * buf =&app.m_write_buf;

    struct rte_mbuf  mbuf0;
    struct rte_mbuf  mbuf1;
    struct rte_mbuf  mbuf2;

    uintptr_t addr1=0x1000;
    uint32_t blk_size=1024;

    buf->Create(blk_size);

    set_mbuf_test(&mbuf0,blk_size,addr1);
    addr1+=1024;
    set_mbuf_test(&mbuf1,blk_size,addr1);
    addr1+=1024;
    set_mbuf_test(&mbuf2,blk_size-10,addr1);

    CMbufObject  mbuf_obj;

    mbuf_obj.m_mbuf=&mbuf0;
    mbuf_obj.m_type=MO_CONST;

    buf->Add_mbuf(mbuf_obj);

    mbuf_obj.m_mbuf=&mbuf1;
    buf->Add_mbuf(mbuf_obj);

    mbuf_obj.m_mbuf=&mbuf2;
    buf->Add_mbuf(mbuf_obj);

    buf->Verify();

    buf->Dump(stdout);

    EXPECT_EQ(tx_sock.get_sbspace(),8*1024);

    
    tx_sock.sb_start_new_buffer();

    /* append maximum */
    tx_sock.sbappend(min(buf->m_t_bytes,tx_sock.sb_hiwat));
    EXPECT_EQ(tx_sock.get_sbspace(),8*1024-(1024*3-10));

    int mss=120;
    CBufMbufRef res;
    int i;
    for (i=0; i<2; i++) {
        tx_sock.get_by_offset((CTcpPerThreadCtx *)0,mss*i,res);
        res.Dump(stdout);
    }

    tx_sock.sbdrop(1024);

    mss=120;
    for (i=0; i<2; i++) {
        tx_sock.get_by_offset((CTcpPerThreadCtx *)0,mss*i,res);
        res.Dump(stdout);
    }

    tx_sock.sbdrop(1024+1024-10);
    EXPECT_EQ(tx_sock.sb_cc,0);


    //buf.Delete();
}

int utl_mbuf_buffer_create_and_fill(CMbufBuffer * buf,
                                    uint32_t blk_size,
                                    uint32_t size){
    buf->Create(blk_size);
    uint8_t cnt=0; 
    while (size>0) {
        uint32_t alloc_size=min(blk_size,size);
        rte_mbuf_t   * m=tcp_pktmbuf_alloc(0,alloc_size);
        assert(m);
        char *p=(char *)rte_pktmbuf_append(m, alloc_size);
        int i;
        for (i=0;i<alloc_size; i++) {
            *p=cnt;
            cnt++;
            p++;
        }
        CMbufObject obj;
        obj.m_type =MO_CONST;
        obj.m_mbuf=m;
        
        buf->Add_mbuf(obj);
        size-=alloc_size;
    }

    return(0);
}

TEST_F(gt_tcp, tst16) {

    CTcpPerThreadCtx        m_ctx;
    CTcpFlow                m_flow;

    m_ctx.Create();
    m_flow.Create(&m_ctx);


    CTcpApp app;
    utl_mbuf_buffer_create_and_fill(&app.m_write_buf,2048,2048*5+10);
    app.m_write_buf.Dump(stdout);

    CMbufBuffer * lpbuf=&app.m_write_buf;
    CTcpSockBuf *lptxs=&m_flow.m_tcp.m_socket.so_snd;
    /* hack the code for now */
    lptxs->m_app=&app;

    /* simulate buf adding */
    lptxs->sb_start_new_buffer();

    /* add maximum of the buffer */
    lptxs->sbappend(min(lpbuf->m_t_bytes,lptxs->sb_hiwat));

    CTcpPkt pkt;
    uint32_t offset=0;
    uint32_t diff=250;

    int i;
    for (i=0; i<10; i++) {
        printf(" %lu \n",(ulong)offset);
        tcp_build_dpkt(&m_ctx,
                       &m_flow.m_tcp,
                       offset, 
                       diff,
                       20, 
                       pkt);
        offset+=diff;

        rte_pktmbuf_dump(pkt.m_buf, diff+500);
        rte_pktmbuf_free(pkt.m_buf);
    }




    m_flow.Delete();
    m_ctx.Delete();

    app.m_write_buf.Delete();

}


TEST_F(gt_tcp, tst17) {

    CTcpPerThreadCtx        m_ctx;
    CTcpFlow                m_flow;

    m_ctx.Create();
    m_flow.Create(&m_ctx);


    CTcpApp app;
    utl_mbuf_buffer_create_and_fill(&app.m_write_buf,2048,10);
    app.m_write_buf.Dump(stdout);

    CMbufBuffer * lpbuf=&app.m_write_buf;
    CTcpSockBuf *lptxs=&m_flow.m_tcp.m_socket.so_snd;
    /* hack the code for now */
    lptxs->m_app=&app;

    /* simulate buf adding */
    lptxs->sb_start_new_buffer();

    /* add maximum of the buffer */
    lptxs->sbappend(min(lpbuf->m_t_bytes,lptxs->sb_hiwat));

    CTcpPkt pkt;
    uint32_t offset=0;
    uint32_t diff=10;

    int i;
    for (i=0; i<1; i++) {
        printf(" %lu \n",(ulong)offset);
        tcp_build_dpkt(&m_ctx,
                       &m_flow.m_tcp,
                       offset, 
                       diff,
                       20, 
                       pkt);
        offset+=diff;

        rte_pktmbuf_dump(pkt.m_buf, diff+500);
        rte_pktmbuf_free(pkt.m_buf);
    }


    m_ctx.m_tcpstat.Dump(stdout);


    m_flow.Delete();
    m_ctx.Delete();

    app.m_write_buf.Delete();

}

/* tcp_output simulation .. */
TEST_F(gt_tcp, tst18) {
#if 0
    CTcpPerThreadCtx        m_ctx;
    CTcpFlow                m_flow;

    m_ctx.Create();
    m_flow.Create(&m_ctx);


#if 0

    CTcpApp app;
    utl_mbuf_buffer_create_and_fill(&app.m_write_buf,2048,10);
    app.m_write_buf.Dump(stdout);

    CMbufBuffer * lpbuf=&app.m_write_buf;
    CTcpSockBuf *lptxs=&m_flow.m_tcp.m_socket.so_snd;
    /* hack the code for now */
    lptxs->m_app=&app;

    /* simulate buf adding */
    lptxs->sb_start_new_buffer();

    /* add maximum of the buffer */
    lptxs->sbappend(min(lpbuf->m_t_bytes,lptxs->sb_hiwat));
#endif


    tcp_connect(&m_ctx,&m_flow.m_tcp);
    int i;
    for (i=0; i<1000; i++) {
        //printf(" tick %lu \n",(ulong)i);
        m_ctx.timer_w_on_tick();
    }


    m_ctx.m_tcpstat.Dump(stdout);

    m_flow.Delete();
    m_ctx.Delete();

#endif
    //app.m_write_buf.Delete();

}


typedef std::vector<rte_mbuf_t *> vec_queue_t;

class CTcpCtxDebug : public CTcpCtxCb {
public:

   int on_tx(CTcpPerThreadCtx *ctx,
             struct tcpcb * flow,
             rte_mbuf_t *m);
public:

   vec_queue_t  m_queue[2];

};


int CTcpCtxDebug::on_tx(CTcpPerThreadCtx *ctx,
                        struct tcpcb * flow,
                        rte_mbuf_t *m){
    int dir=0;
    if (flow->src_port==80) {
        dir=1;
    }
    m_queue[dir].push_back(m);
    utl_k12_pkt_format(stdout,rte_pktmbuf_mtod(m,char *),  m->pkt_len,(ctx->tcp_now)) ;
    return(0);
}

bool test_handle_queue(vec_queue_t * lpq,
                       CTcpPerThreadCtx *ctx,
                       struct tcpcb * flow){

    bool do_work=false;
    rte_mbuf_t *m;
    while (lpq->size()) {
        do_work=true;
        m=(*lpq)[0];
        lpq->erase(lpq->begin());

        char *p=rte_pktmbuf_mtod(m,char *);
        TCPHeader * tcp=(TCPHeader *)(p+14+20);
        IPHeader   *ipv4=(IPHeader *)(p+14);


        assert(tcp_flow_input(ctx,
                       flow,
                       m,
                       tcp,
                       14+20+tcp->getHeaderLength(),
                       ipv4->getTotalLength()-(20+tcp->getHeaderLength()) 
                       )==0);

    }
    return (do_work);
}

/* tcp_output simulation .. */
TEST_F(gt_tcp, tst19) {


    CTcpPerThreadCtx        m_ctx;
    CTcpFlow                m_flow;
    CTcpFlow                m_flow_server;
    CTcpCtxDebug            m_io_debug;

    m_ctx.Create();
    m_ctx.set_cb(&m_io_debug);
    m_flow.Create(&m_ctx);
    m_flow.set_tuple(0x10000001,0x30000001,1025,80,false);
    m_flow.init();


    m_flow_server.Create(&m_ctx);
    m_flow_server.set_tuple(0x30000001,0x10000001,80,1025,false);
    m_flow_server.init();

    //m_flow_server.m_tcp.m_socket.so_options |=US_SO_DEBUG;
    //m_flow.m_tcp.m_socket.so_options |=US_SO_DEBUG;


#if 1

    CTcpApp app;
    utl_mbuf_buffer_create_and_fill(&app.m_write_buf,2048,4024);
    app.m_write_buf.Dump(stdout);

    CMbufBuffer * lpbuf=&app.m_write_buf;
    CTcpSockBuf *lptxs=&m_flow.m_tcp.m_socket.so_snd;
    /* hack the code for now */
    lptxs->m_app=&app;

    /* simulate buf adding */
    lptxs->sb_start_new_buffer();

    /* add maximum of the buffer */
    lptxs->sbappend(min(lpbuf->m_t_bytes,lptxs->sb_hiwat));
#endif


    tcp_connect(&m_ctx,&m_flow.m_tcp);
    tcp_listen(&m_ctx,&m_flow_server.m_tcp);

    int i;
    for (i=0; i<1000; i++) {

        bool do_work=false;
        while (true) {

            do_work=test_handle_queue(&m_io_debug.m_queue[0],
                                      &m_ctx,
                                      &m_flow_server.m_tcp);

            do_work|=test_handle_queue(&m_io_debug.m_queue[1],
                                      &m_ctx,
                                      &m_flow.m_tcp);
            if (do_work==false) {
                  break;
            }
        }

        //printf("*");
        m_ctx.timer_w_on_tick();
    }



    m_flow.Delete();
    m_ctx.Delete();

    //app.m_write_buf.Delete();

}



/* 

copy from m + offset -> end into  p 
*/
struct rte_mbuf * utl_mbuf_cpy(char *p,
                               struct rte_mbuf *mi,
                               uint16_t cp_size, 
                               uint16_t & off){

    while (cp_size) {
        char *md=rte_pktmbuf_mtod(mi, char *);
        assert(mi->data_len > off);
        uint16_t msz=mi->data_len-off;
        uint16_t sz=min(msz,cp_size);
        memcpy(p,md+off,sz);
        p+=sz;
        cp_size-=sz;

        if (sz == msz) {
            mi=mi->next;
            if (mi==NULL) {
                /* error */
                return(mi);
            }
            off=0;
        }else{
            off+=sz;
        }
    }
    return(mi);
}


/**
 * Creates a "clone" of the given packet mbuf - allocate new mbuf in size of block_size and chain them 
 * this is to simulate Tx->Rx packet convertion 
 *
 */
struct rte_mbuf * utl_rte_pktmbuf_deepcopy(struct rte_mempool *mp,
                                           struct rte_mbuf *mi){
    struct rte_mbuf *mc, *m;
    uint32_t pktlen;
    uint32_t origin_pktlen;
    uint8_t nseg;

    mc = rte_pktmbuf_alloc(mp);
    if ( mc== NULL){
        return NULL;
    }

    uint16_t nsegsize = rte_pktmbuf_tailroom(mc);

    m = mc; /* root */
    pktlen = mi->pkt_len;
    origin_pktlen = mi->pkt_len;
    
    uint16_t off=0;
        
    nseg = 0;

    while (true) {

        uint16_t cp_size=min(nsegsize,pktlen);

        char *p=rte_pktmbuf_append(mc, cp_size);

        if (mi == NULL) {
            goto err;
        }
        mi=utl_mbuf_cpy(p,mi,cp_size,off);

        nseg++; /* new */

        pktlen-=cp_size;

        if (pktlen==0) {
            break;  /* finish */
        }else{
            struct rte_mbuf * mt = rte_pktmbuf_alloc(mp);
            if (mt == NULL) {
                goto err;
            }
          mc->next=mt;
          mc=mt;
        }
    }
    m->nb_segs = nseg;
    m->pkt_len = origin_pktlen;
    

    return m;
err:
   rte_pktmbuf_free(m);
   return (NULL); 
}

char * utl_rte_pktmbuf_to_mem(struct rte_mbuf *m){
    char * p=(char *)malloc(m->pkt_len);
    char * op=p;
    uint32_t pkt_len=m->pkt_len;

    while (m) {
        uint16_t seg_len=m->data_len;
        memcpy(p,rte_pktmbuf_mtod(m,char *),seg_len);
        p+=seg_len;
        assert(pkt_len>=seg_len);
        pkt_len-=seg_len;
        m = m->next;
        if (pkt_len==0) {
            assert(m==0);
        }
    }
    return(op);
}


/* return 0   equal
          -1  not equal 

*/          
int utl_rte_pktmbuf_deepcmp(struct rte_mbuf *ma,
                            struct rte_mbuf *mb){

    if (ma->pkt_len != mb->pkt_len) {
        return(-1);
    }
    int res;
    char *pa;
    char *pb;
    pa=utl_rte_pktmbuf_to_mem(ma);
    pb=utl_rte_pktmbuf_to_mem(mb);
    res=memcmp(pa,pb,ma->pkt_len);
    free(pa);
    free(pb);
    return(res);
}


/* add test of buffer with 100 bytes-> 100byte .. deepcopy to 1024 byte buffer */


void  test_fill_mbuf(rte_mbuf_t   * m,
                     uint16_t      b_size,
                     uint8_t  &     start_cnt,
                     int to_rand){

    char *p=rte_pktmbuf_append(m, b_size);
    int i;
    for (i=0; i<(int)b_size; i++) {
        if (to_rand) {
            *p=(uint8_t)(rand()&0xff);
        }else{
            *p=start_cnt++;
        }
        p++;
    }
}

rte_mbuf_t   * test_build_packet_size(int pkt_len,
                                      int chunk_size,
                                      int rand){

    rte_mbuf_t   * m;
    rte_mbuf_t   * prev_m=NULL;;
    rte_mbuf_t   * mhead=NULL;
    uint8_t seg=0;
    uint32_t save_pkt_len=pkt_len;

    uint8_t cnt=1;
    while (pkt_len>0) {
        m=tcp_pktmbuf_alloc(0,chunk_size);
        seg++;

        assert(m);
        if (prev_m) {
            prev_m->next=m;
        }
        prev_m=m;
        if (mhead==NULL) {
            mhead=m;
        }

        uint16_t   csize=min(chunk_size,pkt_len);
        test_fill_mbuf(m,
                       csize,
                       cnt,
                       rand);
        pkt_len-=csize;
    }
    mhead->pkt_len = save_pkt_len;
    mhead->nb_segs = seg;
    return(mhead);
}


TEST_F(gt_tcp, tst20) {
    uint16_t off=0;
    char buf[2048];
    char *p =buf;

    rte_mbuf_t   * m;
    m=test_build_packet_size(1024,
                             60,
                             0);
    //rte_pktmbuf_dump(m, 1024);

    off=8;
    m=utl_mbuf_cpy(p,m,512,off);
//    utl_DumpBuffer(stdout,p,512,0);
}

/* deep copy of mbufs from tx to rx */
int test_mbuf_deepcpy(int pkt_size,int chunk_size){

    rte_mbuf_t   * m;
    rte_mbuf_t   * mc;

    m=test_build_packet_size(pkt_size,chunk_size,1);
    //rte_pktmbuf_dump(m, pkt_size);

    rte_mempool_t * mpool = tcp_pktmbuf_get_pool(0,2047);

    mc =utl_rte_pktmbuf_deepcopy(mpool,m);
    assert(mc);
    //rte_pktmbuf_dump(mc, pkt_size);

    int res=utl_rte_pktmbuf_deepcmp(m,mc);

    rte_pktmbuf_free(m);
    rte_pktmbuf_free(mc);
    return(res);
}

TEST_F(gt_tcp, tst21) {
   EXPECT_EQ(test_mbuf_deepcpy(128,60),0);
   EXPECT_EQ(test_mbuf_deepcpy(1024,60),0);
   EXPECT_EQ(test_mbuf_deepcpy(1025,60),0);
   EXPECT_EQ(test_mbuf_deepcpy(1026,67),0);
   EXPECT_EQ(test_mbuf_deepcpy(4025,129),0);
}


