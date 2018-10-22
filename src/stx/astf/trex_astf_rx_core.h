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
#ifndef __TREX_ASTF_RX_CORE_H__
#define __TREX_ASTF_RX_CORE_H__

#include "common/trex_rx_core.h"
#include "bp_sim.h"
#include "common/trex_messaging.h"


class TrexRxStartLatency : public TrexCpToRxMsgBase {
public:
    TrexRxStartLatency() {
        m_cps=0.0;
        m_client_ip.v4=0;
        m_server_ip.v4=0;
        m_dual_port_mask=0;
        m_active_ports_mask=0xffffffff;
    }

    virtual bool handle(CRxCore *rx_core);

public:
    double    m_cps;
    uint32_t  m_active_ports_mask;
    ipaddr_t  m_client_ip;
    ipaddr_t  m_server_ip;
    uint32_t  m_dual_port_mask;
};


class TrexRxStopLatency : public TrexCpToRxMsgBase {
public:
    TrexRxStopLatency() {
    }

    virtual bool handle(CRxCore *rx_core);

private:
};


class TrexRxUpdateLatency : public TrexCpToRxMsgBase {
public:
    TrexRxUpdateLatency() {
        m_cps=0.0;
    }

    virtual bool handle(CRxCore *rx_core);

public:
    double    m_cps;
};


class CRxAstfCore;

class CRxAstfPort : public CPortLatencyHWBase {
public:
    void Create(CRxAstfCore *lp,uint8_t port){
        m_rx=lp;
        m_port_id=port;
    }

    virtual int tx(rte_mbuf_t *m);

    /* nothing special with HW implementation */
    virtual int tx_latency(rte_mbuf_t *m) {
        return tx(m);
    }

    virtual int tx_raw(rte_mbuf_t *m) {
        assert(0);
    }

    virtual rte_mbuf_t * rx(){
        assert(0);
        return(0);
    }

    virtual uint16_t rx_burst(struct rte_mbuf **rx_pkts, uint16_t nb_pkts){
        return(0);
    }

public:
    CRxAstfCore * m_rx;
    uint8_t       m_port_id;
};


/**
 * TRex ASTF RX core
 * 
 */
class CRxAstfCore : public CRxCore {
public:
    CRxAstfCore();

public:
    /* commands */ 
    void start_latency(TrexRxStartLatency * msg);
    void stop_latency();
    void update_latency(TrexRxUpdateLatency * msg);
    void cp_update_stats();


    void cp_dump(FILE *fd);
    void cp_get_json(std::string & json);

protected:
    virtual uint32_t handle_msg_packets(void);
    virtual uint32_t handle_rx_one_queue(uint8_t thread_id, CNodeRing *r);
    virtual bool work_tick(void);
    virtual int _do_start(void);
    void do_background(void);

    void handle_rx_pkt(CLatencyManagerPerPort * lp,rte_mbuf_t * m);
    bool send_pkt_all_ports(void);
    virtual void handle_astf_latency_pkt(const rte_mbuf_t *m,
                                 uint8_t port_id);

private:
    void create_latency_context();
    void delete_latency_context();

private:
    bool                    m_active_context; /* context for latency streams is allocated */
    bool                    m_latency_active; /* active latency process */
    volatile uint32_t       m_cp_ports_mask_cache; /* cache for CP for active ports */


    CRxAstfPort             m_io_ports[TREX_MAX_PORTS];
    CMessagingManager *     m_rx_dp;
    pqueue_t                m_p_queue; /* priorty queue */
    CLatencyPktInfo         m_pkt_gen;
    CLatencyManagerPerPort  m_ports[TREX_MAX_PORTS];
    uint64_t                m_start_time; // calc tick between sending
    double                  m_delta_sec;
    std::vector<uint8_t>    m_port_ids; // (non dummy) port IDs
    CLatencyPktMode         *m_l_pkt_mode;
    uint32_t                m_port_mask;
    uint32_t                m_max_ports;
    CPortLatencyHWBase *    m_port_io[TREX_MAX_PORTS];
    uint8_t                 m_epoc;
};

#endif /* __TREX_ASTF_RX_CORE_H__ */

