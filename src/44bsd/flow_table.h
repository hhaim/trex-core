#ifndef __FLOW_TABLE_
#define __FLOW_TABLE_

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

#include <common/closehash.h>


typedef uint64_t flow_key_t; 

static inline uint32_t ft_hash_rot(uint32_t v,uint16_t r ){
    return ( (v<<r) | ( v>>(32-(r))) );
}


static inline uint32_t ft_hash1(uint64_t u ){
  uint64_t v = u * 3935559000370003845 + 2691343689449507681;

  v ^= v >> 21;
  v ^= v << 37;
  v ^= v >>  4;

  v *= 4768777513237032717;

  v ^= v << 20;
  v ^= v >> 41;
  v ^= v <<  5;

  return (uint32_t)v;
}

static inline uint32_t ft_hash2(uint64_t in){
    uint64_t in1=in*2654435761;
    /* convert to 32bit */
    uint32_t x= (in1>>32) ^ (in1 & 0xffffffff);
    return (x);
}


class CFlowKeyTuple {
public:
    CFlowKeyTuple(){
        u.m_raw=0;
    }

    void set_ip(uint32_t ip){
        u.m_bf.m_ip = ip;
    }

    void set_port(uint16_t port){
        u.m_bf.m_port = port;
    }

    void set_proto(uint8_t proto){
        u.m_bf.m_proto = proto;
    }

    void set_ipv4(bool ipv4){
        u.m_bf.m_ipv4 = ipv4?1:0;
    }

    uint32_t get_ip(){
        return(u.m_bf.m_ip);
    }

    uint32_t get_port(){
        return(u.m_bf.m_port);
    }

    uint8_t get_proto(){
        return(u.m_bf.m_proto);
    }

    bool get_is_ipv4(){
        return(u.m_bf.m_ipv4?true:false);
    }

    uint64_t get_as_uint64(){
        return (u.m_raw);
    }

    uint32_t get_hash_worse(){
        uint16_t p = get_port();
        uint32_t res = ft_hash_rot(get_ip(),((p %16)+1)) ^ (p + get_proto()) ;
        return (res);
    }

    uint32_t get_hash(){
        return ( ft_hash2(get_as_uint64()) );
    }

    void dump(FILE *fd);
private:
    union {
        struct {
            uint64_t m_ip:32,
                     m_port:16,
                     m_proto:8,
                     m_ipv4:1,
                     m_spare:7;
        }        m_bf;
        uint64_t m_raw;
    } u;

};

/* full tuple -- for directional flow */

class CFlowKeyFullTuple {
public:
    uint8_t  m_proto;
    uint8_t  m_l3_offset;
    uint8_t  m_l4_offset;
    uint8_t  m_l5_offset;
    uint16_t m_total_l7;
    bool     m_ipv4;
};



typedef CHashEntry<flow_key_t> flow_hash_ent_t;
typedef CCloseHash<flow_key_t> flow_hash_t;


class CFlowTable {
public:
    bool Create(uint32_t size,
                bool client_side);
    void Delete();

public:
      bool parse_packet(struct rte_mbuf *   mbuf,
                        CSimplePacketParser & parser,
                        CFlowKeyTuple      & tuple,
                        CFlowKeyFullTuple  & ftuple);

      bool rx_handle_packet(CTcpPerThreadCtx * ctx,
                            struct rte_mbuf * mbuf);

private:
    bool            m_client_side;
    flow_hash_t     m_ft;
};




#endif


