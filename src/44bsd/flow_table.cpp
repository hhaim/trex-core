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
    reset_stats();
    m_tcp_api=(CTcpAppApi    *)0;
    m_prog =(CTcpAppProgram *)0;
    return(true);
}

void CFlowTable::Delete(){
    m_ft.Delete();
}


void CFlowTable::dump(FILE *fd){
    m_ft.Dump(stdout);
    fprintf(fd,"stats_err_no_syn                   : %u \n",m_stats_err_no_syn);
    fprintf(fd,"stats_err_len_err                  : %u \n",m_stats_err_len_err);
    fprintf(fd,"stats_err_no_tcp                   : %u \n",m_stats_err_no_tcp);
    fprintf(fd,"stats_err_client_pkt_without_flow  : %u \n",m_stats_err_client_pkt_without_flow);
    fprintf(fd,"stats_err_no_template              : %u \n",m_stats_err_no_template);
    fprintf(fd,"stats_err_no_memory                : %u \n",m_stats_err_no_memory);
    fprintf(fd,"stats_err_duplicate_client_tuple   : %u \n",m_stats_err_duplicate_client_tuple);
}


void CFlowTable::reset_stats(){
   m_stats_err_no_syn =0;
   m_stats_err_len_err =0;
   m_stats_err_no_tcp =0;
   m_stats_err_client_pkt_without_flow=0;
   m_stats_err_no_template=0;  
   m_stats_err_no_memory=0;
   m_stats_err_duplicate_client_tuple=0;
}


bool CFlowTable::parse_packet(struct rte_mbuf * mbuf,
                              CSimplePacketParser & parser,
                              CFlowKeyTuple & tuple,
                              CFlowKeyFullTuple & ftuple){

    if (!parser.Parse()){
        return(false);
    }
    uint16_t l3_pkt_len=0;

    /* TBD fix, should support UDP/IPv6 */

    if ( parser.m_protocol != IPHeader::Protocol::TCP ){
        m_stats_err_no_tcp++;
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

        l3_pkt_len = ipv4->getTotalLength() + lpf->m_l3_offset;

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
        /* TBD need to find the last header here */

        l3_pkt_len = ipv6->getPayloadLen();
    }

    lpf->m_proto     =   parser.m_protocol;
    lpf->m_l4_offset = (uintptr_t)parser.m_l4 - (uintptr_t)p;

    TCPHeader    * lpTcp = (TCPHeader *)parser.m_l4;

    lpf->m_l7_offset = lpf->m_l4_offset + lpTcp->getHeaderLength();

    if (l3_pkt_len < lpf->m_l7_offset ) {
        m_stats_err_len_err++;
        return(false);
    }
    lpf->m_l7_total_len  =  l3_pkt_len - lpf->m_l7_offset;

    /* TBD need to check TCP header checksum */

    tuple.set_proto(lpf->m_proto);
    tuple.set_ipv4(lpf->m_ipv4);
    return(true);
}



void CFlowTable::handle_close(CTcpPerThreadCtx * ctx,
                              CTcpFlow * flow){
    m_ft.remove(&flow->m_hash);
    ctx->free_flow(flow);
}

void CFlowTable::process_tcp_packet(CTcpPerThreadCtx * ctx,
                                    CTcpFlow *  flow,
                                    struct rte_mbuf * mbuf,
                                    TCPHeader    * lpTcp,
                                    CFlowKeyFullTuple &ftuple){

    tcp_flow_input(ctx,
                   &flow->m_tcp,
                   mbuf,
                   lpTcp,
                   ftuple.m_l7_offset,
                   ftuple.m_l7_total_len
                   );

    if (flow->is_can_close()) {
        handle_close(ctx,flow);
    }
}

bool CFlowTable::insert_new_flow(CTcpFlow *  flow,
                                 CFlowKeyTuple  & tuple){

    flow_key_t key=tuple.get_as_uint64();
    uint32_t  hash=tuple.get_hash();
    flow->m_hash.key =key;

    HASH_STATUS  res=m_ft.insert(&flow->m_hash,hash);
    if (res!=hsOK) {
        m_stats_err_duplicate_client_tuple++;
        /* duplicate */
        return(false);
    }
    return(true);
}


bool CFlowTable::rx_handle_packet(CTcpPerThreadCtx * ctx,
                                  struct rte_mbuf * mbuf){

    CFlowKeyTuple tuple;
    CFlowKeyFullTuple ftuple;

    CSimplePacketParser parser(mbuf);


    if ( !parse_packet(mbuf,
                       parser,
                       tuple,
                       ftuple) ) {
        /* free memory */
        rte_pktmbuf_free(mbuf);
        return(false);
    }

    flow_key_t key=tuple.get_as_uint64();
    uint32_t  hash=tuple.get_hash();

    flow_hash_ent_t * lpflow;
    lpflow = m_ft.find(key,hash);
    CTcpFlow * lptflow;

    if ( lpflow ) {
        lptflow = CTcpFlow::cast_from_hash_obj(lpflow);
        TCPHeader    * lpTcp = (TCPHeader *)parser.m_l4;
        process_tcp_packet(ctx,lptflow,mbuf,lpTcp,ftuple);
        return (true);;
    }

    TCPHeader    * lpTcp = (TCPHeader *)parser.m_l4;

    /* not found in flowtable */
    if ( m_client_side ){
        m_stats_err_client_pkt_without_flow++;
        rte_pktmbuf_free(mbuf);
        return(false);
    }

    /* server side */
    if (  (lpTcp->getFlags() & TCPHeader::Flag::SYN) ==0 ) {
        /* no syn */
        /* TBD need to generate RST packet in this case */
        rte_pktmbuf_free(mbuf);
        m_stats_err_no_syn++;
        return(false);
    }

    /* server with SYN packet, it is OK 
    we need to build the flow and add it to the table */

    /* TBD template port */
    if (lpTcp->getDestPort() != 80) {
        rte_pktmbuf_free(mbuf);
        m_stats_err_no_template++;
        return(false);
    }

    IPHeader *  ipv4 = (IPHeader *)parser.m_ipv4;


    lptflow = ctx->alloc_flow(ipv4->getDestIp(),
                          tuple.get_ip(),
                          lpTcp->getDestPort(),
                          tuple.get_port(),
                          false);



    if (lptflow == 0 ) {
        rte_pktmbuf_free(mbuf);
        m_stats_err_no_memory++;
        return(false);
    }

    /* add to flow-table */
    lptflow->m_hash.key = key;

    /* no need to check, we just checked */
    m_ft.insert_nc(&lptflow->m_hash,hash);

    CTcpApp * app =&lptflow->m_app;

    app->set_program(m_prog);
    app->set_bh_api(m_tcp_api);
    app->set_flow_ctx(ctx,lptflow);

    lptflow->set_app(app);

    app->start(true); /* start the application */
    /* start listen */
    tcp_listen(ctx,&lptflow->m_tcp);

    /* process SYN packet */
    process_tcp_packet(ctx,lptflow,mbuf,lpTcp,ftuple);

    return(true);
}



