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
#include <common/c_common.h>
#include <common/captureFile.h>
#include <common/sim_event_driven.h>



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


#if 0
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

    
    //tx_sock.sb_start_new_buffer();

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
#endif

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

#if 0
TEST_F(gt_tcp, tst16) {

    CTcpPerThreadCtx        m_ctx;
    CTcpFlow                m_flow;

    m_ctx.Create();
    m_flow.Create(&m_ctx);


    //CTcpApp app;
    //utl_mbuf_buffer_create_and_fill(&app.m_write_buf,2048,2048*5+10);
    //app.m_write_buf.Dump(stdout);

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
#endif

#if 0

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
#endif

#if 0
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

#endif



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



class CTcpSimEventStop : public CSimEventBase {

public:
     CTcpSimEventStop(double time){
         m_time =time;
     }
     virtual bool on_event(CSimEventDriven *sched,
                           bool & reschedule){
         reschedule=false;
         return(true);
     }
};



class CClientServerTcp;

class CTcpSimEventTimers : public CSimEventBase {

public:
     CTcpSimEventTimers(CClientServerTcp *p,
                        double dtime){
         m_p = p;
         m_time = dtime;
         m_d_time =dtime;
     }

     virtual bool on_event(CSimEventDriven *sched,
                           bool & reschedule);

private:
    CClientServerTcp * m_p;
    double m_d_time;
};

class CTcpSimEventRx : public CSimEventBase {

public:
    CTcpSimEventRx(CClientServerTcp *p,
                   rte_mbuf_t *m,
                   int dir,
                   double time){
        m_p = p;
        m_pkt = m;
        m_dir = dir;
        m_time = time;
    }

    virtual bool on_event(CSimEventDriven *sched,
                          bool & reschedule);

private:
    CClientServerTcp * m_p;
    int                m_dir;
    rte_mbuf_t *       m_pkt;
};


class CTcpCtxPcapWrt  {
public:
    CTcpCtxPcapWrt(){
        m_writer=NULL;
        m_raw=NULL;
    }
    ~CTcpCtxPcapWrt(){
        close_pcap_file();
    }

    bool  open_pcap_file(std::string pcap);
    void  write_pcap_mbuf(rte_mbuf_t *m,double time);

    void  close_pcap_file();


public:

   CFileWriterBase         * m_writer;
   CCapPktRaw              * m_raw;
};


void  CTcpCtxPcapWrt::write_pcap_mbuf(rte_mbuf_t *m,
                                      double time){

    char *p;
    uint32_t pktlen=m->pkt_len;
    if (pktlen>MAX_PKT_SIZE) {
        printf("ERROR packet size is bigger than 9K \n");
    }

    p=utl_rte_pktmbuf_to_mem(m);
    memcpy(m_raw->raw,p,pktlen);
    m_raw->pkt_cnt++;
    m_raw->pkt_len =pktlen;
    m_raw->set_new_time(time);

    assert(m_writer);
    bool res=m_writer->write_packet(m_raw);
    if (res != true) {
        fprintf(stderr,"ERROR can't write to cap file");
    }
    free(p);
}


bool  CTcpCtxPcapWrt::open_pcap_file(std::string pcap){

    m_writer = CCapWriterFactory::CreateWriter(LIBPCAP,(char *)pcap.c_str());
    if (m_writer == NULL) {
        fprintf(stderr,"ERROR can't create cap file %s ",(char *)pcap.c_str());
        return (false);
    }
    m_raw = new CCapPktRaw();
    return(true);
}

void  CTcpCtxPcapWrt::close_pcap_file(){
    if (m_raw){
        delete m_raw;
        m_raw = NULL;
    }
    if (m_writer) {
        delete m_writer;
        m_writer = NULL;
    }
}



//typedef std::vector<rte_mbuf_t *> vec_queue_t;

class CTcpCtxDebug : public CTcpCtxCb {
public:

   int on_tx(CTcpPerThreadCtx *ctx,
             struct tcpcb * flow,
             rte_mbuf_t *m);

public:
    CClientServerTcp  * m_p ;

};

rte_mbuf_t * utl_rte_convert_tx_rx_mbuf(rte_mbuf_t *m){

    rte_mbuf_t * mc;
    rte_mempool_t * mpool = tcp_pktmbuf_get_pool(0,2047);
    mc =utl_rte_pktmbuf_deepcopy(mpool,m);
    assert(mc);
    rte_pktmbuf_free(m); /* free original */
    return (mc);
}

void utl_rte_pktmbuf_k12_format(FILE* fp,
                                rte_mbuf_t *m,
                                int time_sec) {

    char *p;
    uint32_t pktlen=m->pkt_len;
    p=utl_rte_pktmbuf_to_mem(m);
    utl_k12_pkt_format(stdout,p, pktlen, time_sec) ;    
    free(p);
}



#if 0
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
#endif



class CClientServerTcp {
public:
    bool Create(std::string pcap_file);
    void Delete();


    /* dir ==0 , C->S 
       dir ==1   S->C */
    void on_tx(int dir,rte_mbuf_t *m);
    void on_rx(int dir,rte_mbuf_t *m);

public:
    int test1();
    int test2();


public:
    CTcpPerThreadCtx        m_c_ctx;  /* context */
    CTcpPerThreadCtx        m_s_ctx;

    CTcpFlow                m_c_flow; /* flow */
    CTcpFlow                m_s_flow;

    CTcpAppApiImpl          m_tcp_bh_api_impl_c;
    CTcpAppApiImpl          m_tcp_bh_api_impl_s;

    CTcpCtxPcapWrt          m_c_pcap; /* capture to file */
    CTcpCtxPcapWrt          m_s_pcap;

    CTcpCtxDebug            m_io_debug;

    CSimEventDriven         m_sim;
    double                  m_rtt_sec;
    double                  m_tx_diff;
};


int CTcpCtxDebug::on_tx(CTcpPerThreadCtx *ctx,
                        struct tcpcb * flow,
                        rte_mbuf_t *m){
    int dir=0;
    if (flow->src_port==80) {
        dir=1;
    }
    rte_mbuf_t *m_rx= utl_rte_convert_tx_rx_mbuf(m);

    m_p->on_tx(dir,m_rx);
    return(0);
}


bool CClientServerTcp::Create(std::string pcap_file){

    m_io_debug.m_p = this;
    m_tx_diff =0.0;

    m_rtt_sec =0.05; /* 50msec */

    m_c_pcap.open_pcap_file(pcap_file+"_c.pcap");

    m_s_pcap.open_pcap_file(pcap_file+"_s.pcap");

    m_c_ctx.Create();
    m_c_ctx.set_cb(&m_io_debug);

    m_s_ctx.Create();
    m_s_ctx.set_cb(&m_io_debug);

    m_c_flow.Create(&m_c_ctx);
    m_c_flow.set_tuple(0x10000001,0x30000001,1025,80,false);
    m_c_flow.init();

    m_s_flow.Create(&m_s_ctx);
    m_s_flow.set_tuple(0x30000001,0x10000001,80,1025,false);
    m_s_flow.init();

    return(true);
}

void CClientServerTcp::on_tx(int dir,
                             rte_mbuf_t *m){

    m_tx_diff+=1/1000000.0;  /* to make sure there is no out of order */
    double t=m_sim.get_time()+m_tx_diff;


    /* write TX side */
    if (dir==0) {
        m_c_pcap.write_pcap_mbuf(m,t);
    }else{
        m_s_pcap.write_pcap_mbuf(m,t);
    }

    /* simulate drop/reorder/ corruption HERE !! */


    /* move the Tx packet to Rx side of server */
    m_sim.add_event( new CTcpSimEventRx(this,m,dir^1,(t+(m_rtt_sec/2.0) )) );
}

bool CTcpSimEventRx::on_event(CSimEventDriven *sched,
                              bool & reschedule){
    reschedule=false;
    m_p->on_rx(m_dir,m_pkt);
    return(false);
}

bool CTcpSimEventTimers::on_event(CSimEventDriven *sched,
                                  bool & reschedule){
     reschedule=true;
     m_time +=m_d_time;
     m_p->m_c_ctx.timer_w_on_tick();
     m_p->m_s_ctx.timer_w_on_tick();
     return(false);
 }


void CClientServerTcp::on_rx(int dir,
                             rte_mbuf_t *m){

    /* write RX side */
    double t = m_sim.get_time();
    CTcpPerThreadCtx * ctx;
    struct tcpcb *tp;


    if (dir==1) {
        ctx =&m_s_ctx,
        tp  =&m_s_flow.m_tcp;
        m_s_pcap.write_pcap_mbuf(m,t);
    }else{
        ctx =&m_c_ctx;
        tp  =&m_c_flow.m_tcp;
        m_c_pcap.write_pcap_mbuf(m,t);
    }


    /* TBD should be fixed */
    char *p=rte_pktmbuf_mtod(m,char *);
    TCPHeader * tcp=(TCPHeader *)(p+14+20);
    IPHeader   *ipv4=(IPHeader *)(p+14);


    assert(tcp_flow_input(ctx,
                   tp,
                   m,
                   tcp,
                   14+20+tcp->getHeaderLength(),
                   ipv4->getTotalLength()-(20+tcp->getHeaderLength()) 
                   )==0);

}


void CClientServerTcp::Delete(){
    m_c_flow.Delete();
    m_s_flow.Delete();
    m_c_ctx.Delete();
    m_s_ctx.Delete();
}



int CClientServerTcp::test1(){

#if 0
    CTcpApp app;
    /* TBD */
    //utl_mbuf_buffer_create_and_fill(&app.m_write_buf,2048,4024);
    /*TBD */
    //CMbufBuffer * lpbuf=&app.m_write_buf;
    //CMbufBuffer * lpbuf = NULL;

    m_rtt_sec = 1.2;

    m_c_flow.m_tcp.m_socket.m_app =&app;

    //m_s_flow.m_tcp.m_socket.m_app =&app;

    //CTcpSockBuf * lptxs=&m_c_flow.m_tcp.m_socket.so_snd;
    /* hack the code for now */
    //lptxs->m_app=&app;

    /* simulate buf adding */
    //lptxs->sb_start_new_buffer();

    /* add maximum of the buffer */
    //lptxs->sbappend(min(lpbuf->m_t_bytes,lptxs->sb_hiwat));

    m_sim.add_event( new CTcpSimEventTimers(this, ((double)(TCP_TIMER_W_TICK)/1000.0)));
    m_sim.add_event( new CTcpSimEventStop(100.0) );

    tcp_connect(&m_c_ctx,&m_c_flow.m_tcp);
    tcp_listen(&m_s_ctx,&m_s_flow.m_tcp);

    m_sim.run_sim();

    printf(" C counters \n");
    m_c_ctx.m_tcpstat.Dump(stdout);
    printf(" S counters \n");
    m_s_ctx.m_tcpstat.Dump(stdout);

    EXPECT_EQ(m_c_ctx.m_tcpstat.m_sts.tcps_sndbyte,4024);
    EXPECT_EQ(m_c_ctx.m_tcpstat.m_sts.tcps_rcvackbyte,4024);
    EXPECT_EQ(m_c_ctx.m_tcpstat.m_sts.tcps_connects,1);


    EXPECT_EQ(m_s_ctx.m_tcpstat.m_sts.tcps_rcvbyte,4024);
    EXPECT_EQ(m_s_ctx.m_tcpstat.m_sts.tcps_accepts,1);
    EXPECT_EQ(m_s_ctx.m_tcpstat.m_sts.tcps_preddat,m_s_ctx.m_tcpstat.m_sts.tcps_rcvpack-1);


    //app.m_write_buf.Delete();

    printf (" rx %d 8\n",m_s_flow.m_tcp.m_socket.so_rcv.sb_cc);
    assert( m_s_flow.m_tcp.m_socket.so_rcv.sb_cc == 4024);
#endif
    return(0);

}

int CClientServerTcp::test2(){


    CMbufBuffer * buf;
    CTcpAppProgram * prog_c;
    CTcpAppProgram * prog_s;
    CTcpApp * app_c;
    CTcpApp * app_s;
    CTcpAppCmd cmd;

    uint32_t tx_num_bytes=100*1024;

    /* CONST */
    buf = new CMbufBuffer();
    prog_c = new CTcpAppProgram();
    prog_s = new CTcpAppProgram();
    utl_mbuf_buffer_create_and_fill(buf,2048,tx_num_bytes);


    /* PER FLOW  */

    app_c = new CTcpApp();
    app_s = new CTcpApp();



    /* client program */
    cmd.m_cmd =tcTX_BUFFER;
    cmd.u.m_tx_cmd.m_buf =buf;

    prog_c->add_cmd(cmd);


    /* server program */

    cmd.m_cmd =tcRX_BUFFER;
    cmd.u.m_rx_cmd.m_flags =CTcpAppCmdRxBuffer::rxcmd_WAIT;
    cmd.u.m_rx_cmd.m_rx_bytes_wm = tx_num_bytes;

    prog_s->add_cmd(cmd);


    app_c->set_program(prog_c);
    app_c->set_bh_api(&m_tcp_bh_api_impl_c);
    app_c->set_flow_ctx(&m_c_ctx,&m_c_flow);
    app_c->set_debug_id(1);



    app_s->set_program(prog_s);
    app_s->set_bh_api(&m_tcp_bh_api_impl_s);
    app_s->set_flow_ctx(&m_s_ctx,&m_s_flow);
    app_s->set_debug_id(2);


    m_rtt_sec = 0.05;


    /* HACK !!! need to solve this */

    m_c_flow.m_tcp.m_socket.m_app =app_c;
    m_s_flow.m_tcp.m_socket.m_app =app_s;

    /* HACK !!! need to solve this */


    m_sim.add_event( new CTcpSimEventTimers(this, ((double)(TCP_TIMER_W_TICK)/1000.0)));
    m_sim.add_event( new CTcpSimEventStop(1000.0) );

    app_c->start(true);
    tcp_connect(&m_c_ctx,&m_c_flow.m_tcp);

    app_s->start(true);
    tcp_listen(&m_s_ctx,&m_s_flow.m_tcp);

    m_sim.run_sim();

    printf(" C counters \n");
    m_c_ctx.m_tcpstat.Dump(stdout);
    printf(" S counters \n");
    m_s_ctx.m_tcpstat.Dump(stdout);

    //EXPECT_EQ(m_c_ctx.m_tcpstat.m_sts.tcps_sndbyte,4024);
    //EXPECT_EQ(m_c_ctx.m_tcpstat.m_sts.tcps_rcvackbyte,4024);
    //EXPECT_EQ(m_c_ctx.m_tcpstat.m_sts.tcps_connects,1);


    //EXPECT_EQ(m_s_ctx.m_tcpstat.m_sts.tcps_rcvbyte,4024);
    //EXPECT_EQ(m_s_ctx.m_tcpstat.m_sts.tcps_accepts,1);
    //EXPECT_EQ(m_s_ctx.m_tcpstat.m_sts.tcps_preddat,m_s_ctx.m_tcpstat.m_sts.tcps_rcvpack-1);


    //app.m_write_buf.Delete();
    printf (" rx %d \n",m_s_flow.m_tcp.m_socket.so_rcv.sb_cc);
    //assert( m_s_flow.m_tcp.m_socket.so_rcv.sb_cc == 4024);


    delete app_c;
    delete app_s;

    delete prog_c;
    delete prog_s;

    buf->Delete();
    delete buf;

    return(0);

}



CClientServerTcp tcp_test1;


/* tcp_output simulation .. */
TEST_F(gt_tcp, tst19) {
    CClientServerTcp *lpt1=new CClientServerTcp;

    lpt1->Create("tcp1");

    lpt1->test1();

    lpt1->Delete();

    delete lpt1;
}





TEST_F(gt_tcp, tst30) {

    CClientServerTcp *lpt1=new CClientServerTcp;

    lpt1->Create("tcp2");

    lpt1->test2();

    lpt1->Delete();

    delete lpt1;

}



TEST_F(gt_tcp, tst31) {

    CMbufBuffer * buf;
    CTcpAppProgram * prog;
    CTcpApp * app;

    app = new CTcpApp();
    buf = new CMbufBuffer();

    utl_mbuf_buffer_create_and_fill(buf,2048,100*1024);

    prog = new CTcpAppProgram();

    CTcpAppCmd cmd;
    cmd.m_cmd =tcTX_BUFFER;
    cmd.u.m_tx_cmd.m_buf =buf;


    prog->add_cmd(cmd);

    cmd.m_cmd = tcRX_BUFFER;
    cmd.u.m_rx_cmd.m_flags = CTcpAppCmdRxBuffer::rxcmd_WAIT;
    cmd.u.m_rx_cmd.m_rx_bytes_wm = 1000;

    prog->add_cmd(cmd);

    cmd.m_cmd = tcDELAY;
    cmd.u.m_delay_cmd.m_usec_delay  = 1000;

    prog->add_cmd(cmd);

    prog->Dump(stdout);

    app->set_program(prog);


    delete app;
    delete prog;
    buf->Delete();
    delete buf;
}

