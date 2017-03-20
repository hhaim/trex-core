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


class gt_tcp  : public testing::Test {

protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }
public:
};


// base tick in msec
#define TCP_TIMER_TICK_BASE_MS 200 
#define TCP_TIMER_W_TICK       50

#define TCP_FAST_TICK (TCP_TIMER_TICK_BASE_MS/TCP_TIMER_W_TICK)


class CTcpFlow {

public:
    void Create();
    void Delete();

    void on_slow_tick(){
        printf(" slow tick \n");
    }

    void on_fast_tick(){
        printf(" fast tick \n");
    }

    void on_tick(){
        on_fast_tick();
        if (m_tick==1) {
            m_tick=0;
            on_slow_tick();
        }else{
            m_tick++;
        }
    }

public:
    uint8_t     m_tick; /* 0 and 1 */
    CHTimerObj  m_timer;
};

void CTcpFlow::Create(){
    m_tick=0;
    m_timer.reset();
}

void CTcpFlow::Delete(){
}

class CTcpPerThreadCtx {
public:
    bool Create(void);
    void Delete();
    RC_HTW_t timer_w_start(CTcpFlow * flow){
        return (m_timer_w.timer_start(&flow->m_timer,TCP_FAST_TICK));
    }

    RC_HTW_t timer_w_stop(CTcpFlow * flow){
        return (m_timer_w.timer_stop(&flow->m_timer));
    }

    void timer_w_on_tick();

private:

    CHTimerWheel  m_timer_w; /* TBD-FIXME*/
};

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
}


bool CTcpPerThreadCtx::Create(void){

    RC_HTW_t tw_res;
    tw_res = m_timer_w.Create(512,1);
    if (tw_res != RC_HTW_OK ){
        CHTimerWheelErrorStr err(tw_res);
        printf("Timer wheel configuration error,please look into the manual for details  \n");
        printf("ERROR  %-30s  - %s \n",err.get_str(),err.get_help_str());
        return(false);
    }
    return(true);
}



void CTcpPerThreadCtx::Delete(){
    m_timer_w.Delete();
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




