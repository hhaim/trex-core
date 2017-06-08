#include "flow_table.h"
#include "tcp_var.h"
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


#include "flow_stat_parser.h"

void CFlowKeyTuple::dump(FILE *fd){
    fprintf(fd,"m_ip       : %lu \n",(ulong)get_ip());
    fprintf(fd,"m_port     : %lu \n",(ulong)get_port());
    fprintf(fd,"m_proto    : %lu \n",(ulong)get_proto());
    fprintf(fd,"m_ipv4     : %lu \n",(ulong)get_is_ipv4());
    fprintf(fd,"hash       : %u \n",get_hash());
}


bool CFlowTable::Create(uint32_t size,
                        bool client_side){

    m_client_side = client_side;
    if (!m_ft.Create(size) ){
        printf("ERROR can't allocate flow-table \n");
        return(false);
    }
    return(true);
}

void CFlowTable::Delete(){
    m_ft.Delete();
}


bool CFlowTable::parse_packet(struct rte_mbuf * mbuf,
                              CSimplePacketParser & parser,
                              CFlowKeyTuple & tuple,
                              CFlowKeyFullTuple & ftuple){

    if (!parser.Parse()){
        return(false);
    }

    /* TBD fix, should support UDP/IPv6 */

    if ( parser.m_protocol != IPHeader::Protocol::TCP ){
        return(false);
    }
    /* it is TCP, only supported right now */

    uint8_t *p=rte_pktmbuf_mtod(mbuf, uint8_t*);
    /* check packet length and checksum in case of TCP */

    CFlowKeyFullTuple *lpf= &ftuple;

    if (parser.m_ipv4) {
        /* IPv4 */
        lpf->m_ipv4      =true;
        lpf->m_l3_offset = (uintptr_t)parser.m_ipv4 - (uintptr_t)p;
        IPHeader *   ipv4= parser.m_ipv4;
        TCPHeader    * lpTcp = (TCPHeader *)parser.m_l4;
        if ( m_client_side ) {
            tuple.set_ip(ipv4->getDestIp());
            tuple.set_port(lpTcp->getDestPort());
        }else{
            tuple.set_ip(ipv4->getSourceIp());
            tuple.set_port(lpTcp->getSourcePort());
        }


    }else{
        lpf->m_ipv4      =false;
        lpf->m_l3_offset = (uintptr_t)parser.m_ipv6 - (uintptr_t)p;

        IPv6Header *   ipv6= parser.m_ipv6;
        TCPHeader    * lpTcp = (TCPHeader *)parser.m_l4;

        if ( m_client_side ) {
            tuple.set_ip(ipv6->getDestIpv6LSB());
            tuple.set_port(lpTcp->getDestPort());
        }else{
            tuple.set_ip(ipv6->getSourceIpv6LSB());
            tuple.set_port(lpTcp->getSourcePort());
        }
    }

    lpf->m_proto     =   parser.m_protocol;
    lpf->m_l4_offset = (uintptr_t)parser.m_l4 - (uintptr_t)p;

    lpf->m_l5_offset = lpf->m_l4_offset + tcp->getHeaderLength();
    lpf->m_total_l7  =

    tuple.set_proto(lpf->m_proto);
    tuple.set_ipv4(lpf->m_ipv4);
    return(true);
}

bool CFlowTable::rx_handle_packet(CTcpPerThreadCtx * ctx,
                                  struct rte_mbuf * mbuf){

    CFlowKeyTuple tuple;
    CFlowKeyFullTuple ftuple;

    CSimplePacketParser parser(mbuf);


    if ( !parse_packet(mbuf,
                      tuple,
                      ftuple) ) {
        /* free memory */
        rte_pktmbuf_free(mbuf);
    }

    flow_key_t key=tuple.get_as_uint64();
    uint32_t  hash=tuple.get_hash();

    flow_hash_ent_t * lpflow;
    lpflow = m_ft.find(key,hash);
    CTcpFlow * lptflow;


    if ( lpflow ) {
        lptflow = CTcpFlow::cast_from_hash_obj(lpflow);

        TCPHeader    * lpTcp = (TCPHeader *)parser.m_l4;
        tcp_flow_input(ctx,
                       &lptflow->m_tcp,
                       mbuf,
                       lpTcp,
                       ftuple.m_l5_offset,
                       ipv4->getTotalLength()-(20+tcp->getHeaderLength()) 
                       )



    }



    return(true);
}



