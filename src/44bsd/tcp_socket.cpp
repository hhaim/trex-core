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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <common/basic_utils.h>
#include "tcp_fsm.h"
#include "tcp_seq.h"
#include "tcp_timer.h"
#include "tcp_var.h"
#include "tcpip.h"
#include "tcp_debug.h"
#include "tcp_socket.h"



void CBufMbufRef::Dump(FILE *fd){
    CMbufObject::Dump(fd);
    fprintf(fd," (offset:%lu) (rsize:%lu)  \n",(ulong)m_offset,(ulong)get_mbuf_size());
}


std::string get_mbuf_type_name(mbuf_types_t mbuf_type){
    switch (mbuf_type) {
    case MO_CONST:
        return ("CONST");
        break;
    case MO_RW:
        return ("RW");
        break;
    default:
        return ("Unknown");
    }
}


void CMbufObject::Dump(FILE *fd){
   fprintf(fd," %p (len:%lu) %s \n",m_mbuf->buf_addr,(ulong)m_mbuf->pkt_len,
           get_mbuf_type_name(m_type).c_str());
}


void CMbufBuffer::Reset(){
    m_num_pkt=0;
    m_blk_size=0;
    m_t_bytes=0;
    m_vec.clear();
}

void CMbufBuffer::Create(uint32_t blk_size){
    Reset();
    m_blk_size = blk_size;
}

void CMbufBuffer::Delete(){
    int i;
    for (i=0; i<m_vec.size(); i++) {
        m_vec[i].Free();
    }
    Reset();
}

void CMbufBuffer::get_by_offset(uint32_t offset,CBufMbufRef & res){
    assert(offset<m_t_bytes);
    /* TBD might  worth to cache the calculation (%/) and save the next offset calculation ..,
       in case of MSS == m_blk_size it would be just ++ */
    uint32_t index=offset/m_blk_size;
    uint32_t mod=offset%m_blk_size;

    res.m_offset = mod;
    CMbufObject *lp= &m_vec[index];
    res.m_mbuf   = lp->m_mbuf;
    res.m_type   = lp->m_type; 
}


void CMbufBuffer::Dump(FILE *fd){
    fprintf(fd," total_size %lu, blk: %lu, pkts: %lu \n",(ulong)m_t_bytes,
            (ulong)m_blk_size,(ulong)m_num_pkt);

    int i;
    for (i=0; i<m_vec.size(); i++) {
        m_vec[i].Dump(fd);
    }
}


bool CMbufBuffer::Verify(){

    assert(m_vec.size()==m_num_pkt);
    if (m_num_pkt==0) {
        return(true);
    }
    int i=0;
    for (i=0; i<m_num_pkt-1; i++) {
        assert(m_vec[i].m_mbuf->pkt_len==m_blk_size);
    }
    assert(m_vec[i].m_mbuf->pkt_len<=m_blk_size);
    return(true);
}


void CMbufBuffer::Add_mbuf(CMbufObject & obj){
    /* should have one segment */
    assert(obj.m_mbuf);
    assert(obj.m_mbuf->nb_segs==1);

    m_num_pkt +=1;
    m_t_bytes +=obj.m_mbuf->pkt_len;
    m_vec.push_back(obj);
}









