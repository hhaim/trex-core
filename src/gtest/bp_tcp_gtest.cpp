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
#include "mbuf.h"


class gt_tcp  : public testing::Test {

protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }
public:
};


#define MYC(f) if (m_sts.f)  fprintf(fd," %-40s: %llu \n",#f,(unsigned long long)m_sts.f)
#define MYC_A(f)     fprintf(fd," %-40s: %llu \n",#f,(unsigned long long)m_sts.f)


void tcpstat::Clear(){
    memset(&m_sts,0,sizeof(tcpstat_int_t));
}


void tcpstat::Dump(FILE *fd){

    MYC(tcps_connattempt);
    MYC(tcps_accepts);	   
    MYC(tcps_connects);	   
    MYC(tcps_drops);	       
    MYC(tcps_conndrops);
    MYC(tcps_closed); 
    MYC(tcps_segstimed);	   
    MYC(tcps_rttupdated);   
    MYC(tcps_delack); 
    MYC(tcps_timeoutdrop);
    MYC(tcps_rexmttimeo);
    MYC(tcps_persisttimeo);
    MYC(tcps_keeptimeo);
    MYC(tcps_keepprobe);   
    MYC(tcps_keepdrops);   

    MYC(tcps_sndtotal);	   
    MYC(tcps_sndpack);	   
    MYC(tcps_sndbyte);	   
    MYC(tcps_sndrexmitpack);
    MYC(tcps_sndrexmitbyte);
    MYC(tcps_sndacks);
    MYC(tcps_sndprobe);	   
    MYC(tcps_sndurg);
    MYC(tcps_sndwinup);
    MYC(tcps_sndctrl);

    MYC(tcps_rcvtotal);	   
    MYC(tcps_rcvpack);	   
    MYC(tcps_rcvbyte);	   
    MYC(tcps_rcvbadsum);	   
    MYC(tcps_rcvbadoff);	   
    MYC(tcps_rcvshort);	   
    MYC(tcps_rcvduppack);   
    MYC(tcps_rcvdupbyte);   
    MYC(tcps_rcvpartduppack);  
    MYC(tcps_rcvpartdupbyte);  
    MYC(tcps_rcvoopack);		
    MYC(tcps_rcvoobyte);		
    MYC(tcps_rcvpackafterwin);  
    MYC(tcps_rcvbyteafterwin);  
    MYC(tcps_rcvafterclose);	   
    MYC(tcps_rcvwinprobe);	   
    MYC(tcps_rcvdupack);		   
    MYC(tcps_rcvacktoomuch);	   
    MYC(tcps_rcvackpack);	   
    MYC(tcps_rcvackbyte);	   
    MYC(tcps_rcvwinupd);		   
    MYC(tcps_pawsdrop);		   
    MYC(tcps_predack);		   
    MYC(tcps_preddat);		   
    MYC(tcps_pcbcachemiss);
    MYC(tcps_persistdrop);	   
    MYC(tcps_badsyn);		   
}



void CTcpFlow::Create(){
    m_tick=0;
    m_timer.reset();
}

void CTcpFlow::Delete(){
}


#define unsafe_container_of(var,ptr, type, member)              \
    ((type *) ((uint8_t *)(ptr) - offsetof(type, member)))


static void tcp_timer(void *userdata,
                       CHTimerObj *tmr){
    CTcpPerThreadCtx * tcp_ctx=(CTcpPerThreadCtx * )userdata;
    UNSAFE_CONTAINER_OF_PUSH;
    CTcpFlow * tcp_flow=unsafe_container_of(tcp_flow,tmr,CTcpFlow,m_timer);
    UNSAFE_CONTAINER_OF_POP;
    tcp_flow->on_tick();
    tcp_ctx->timer_w_start(tcp_flow);
}

void CTcpPerThreadCtx::timer_w_on_tick(){
    m_timer_w.on_tick((void*)this,tcp_timer);

    if ( m_tick==TCP_SLOW_RATIO_TICK ) {
        tcp_maxidle = tcp_keepcnt * tcp_keepintvl;

        tcp_iss += TCP_ISSINCR/PR_SLOWHZ;		/* increment iss */
    #ifdef TCP_COMPAT_42
        if ((int)tcp_iss < 0)
            tcp_iss = TCP_ISSINCR;			/* XXX */
    #endif
        tcp_now++;					/* for timestamps */
        m_tick=0;
    } else{
        m_tick++;
    }
}





TEST_F(gt_tcp, tst2) {
    CTcpPerThreadCtx tcp_ctx;
    CTcpFlow   flow;
    flow.Create();

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

    CTcpFlow   flow;
    flow.Create();
    flow.on_tick();
    flow.on_tick();
    flow.Delete();
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








