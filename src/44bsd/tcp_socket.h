#ifndef BSD44_BR_TCP_SOCKET
#define BSD44_BR_TCP_SOCKET

/*
 * Copyright (c) 1982, 1986, 1993
 *  The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *  This product includes software developed by the University of
 *  California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *  @(#)tcp_socket.h    8.1 (Berkeley) 6/10/93
 */

#include <vector>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

                 

struct  sockbuf {
    uint32_t    sb_cc;      /* actual chars in buffer */
    uint32_t    sb_hiwat;   /* max actual char count */
    uint32_t    sb_mbcnt;   /* chars of mbufs used */
    uint32_t    sb_mbmax;   /* max chars of mbufs to use */
    //int64_t       sb_lowat;   /* low water mark */
    //struct    rte_mbuf *sb_mb;    /* the mbuf chain */
    short   sb_flags;   /* flags, see below */
    //short sb_timeo;   /* timeout for read/write */
};

// so_state
#define US_SS_CANTRCVMORE  1


#define US_SO_DEBUG 0x0001      /* turn on debugging info recording */
#define US_SO_ACCEPTCONN    0x0002      /* socket has had listen() */
#define US_SO_REUSEADDR 0x0004      /* allow local address reuse */
#define US_SO_KEEPALIVE 0x0008      /* keep connections alive */
#define US_SO_DONTROUTE 0x0010      /* just use interface addresses */
#define US_SO_BROADCAST 0x0020      /* permit sending of broadcast msgs */

#define US_SO_LINGER    0x0080      /* linger on close if data present */
#define US_SO_OOBINLINE 0x0100      /* leave received OOB data in line */
//#if __BSD_VISIBLE

#define US_SO_REUSEPORT 0x0200      /* allow local address & port reuse */

#define US_SO_TIMESTAMP 0x0400      /* timestamp received dgram traffic */
#define US_SO_NOSIGPIPE 0x0800      /* no SIGPIPE from EPIPE */
#define US_SO_ACCEPTFILTER  0x1000      /* there is an accept filter */
#define US_SO_BINTIME   0x2000      /* timestamp received dgram traffic */
//#endif
#define US_SO_NO_OFFLOAD    0x4000      /* socket cannot be offloaded */
#define US_SO_NO_DDP    0x8000      /* disable direct data placement */


#define SB_MAX      (256*1024)  /* default for max chars in sockbuf */
#define SB_LOCK     0x01        /* lock on data queue */
#define SB_WANT     0x02        /* someone is waiting to lock */
#define SB_WAIT     0x04        /* someone is waiting for data/space */
#define SB_SEL      0x08        /* someone is selecting */
#define SB_ASYNC    0x10        /* ASYNC I/O, need signals */
#define SB_NOTIFY   (SB_WAIT|SB_SEL|SB_ASYNC)
#define SB_NOINTR   0x40        /* operations not interruptible */
#define SB_DROPCALL 0x80



struct tcp_socket * sonewconn(struct tcp_socket *head, int connstatus);

long    sbspace(struct sockbuf *sb); 
void    sbdrop(struct sockbuf *sb, int len);

int soabort(struct tcp_socket *so);
void sowwakeup(struct tcp_socket *so);
void sorwakeup(struct tcp_socket *so);


void    soisdisconnected(struct tcp_socket *so);
void    soisdisconnecting(struct tcp_socket *so);


void sbflush (struct sockbuf *sb);
void    sbappend(struct sockbuf *sb, struct rte_mbuf *m);
void    sbappend_bytes(struct sockbuf *sb, uint64_t bytes);

#ifdef FIXME
#define sorwakeup(so)   { sowakeup((so), &(so)->so_rcv); \
              if ((so)->so_upcall) \
                (*((so)->so_upcall))((so), (so)->so_upcallarg, M_DONTWAIT); \
                
            }
#else


#endif            

void    soisconnected(struct tcp_socket *so);
void    soisconnecting(struct tcp_socket *so);

void    socantrcvmore(struct tcp_socket *so);


//#define   IN_CLASSD(i)        (((u_int32_t)(i) & 0xf0000000) == 0xe0000000)
/* send/received as ip/link-level broadcast */
#define TCP_PKT_M_BCAST     (1ULL << 1) 

/* send/received as iplink-level multicast */
#define TCP_PKT_M_MCAST     (1ULL << 2) 
#define TCP_PKT_M_B_OR_MCAST    (TCP_PKT_M_BCAST  | TCP_PKT_M_MCAST)

typedef uint8_t tcp_l2_pkt_flags_t ;



#define TCP_US_ECONNREFUSED  (7)
#define TCP_US_ECONNRESET    (8)

#define TCP_US_ETIMEDOUT     (9)
#define TCP_US_ECONNABORTED  (10)
#define TCP_US_ENOBUFS       (11)


typedef enum { MO_CONST=17,
               MO_RW 
             } mbuf_types_enum_t;

typedef uint32_t mbuf_types_t;

class CMbufObject{
public:
    mbuf_types_t      m_type;
    struct rte_mbuf * m_mbuf;
    void Dump(FILE *fd);
    void Free(){
        if (m_mbuf) {
            rte_pktmbuf_free(m_mbuf);
            m_mbuf=0;
        }
    }
};

class CBufMbufRef : public CMbufObject {

public:
    inline uint16_t get_mbuf_size(){
        return (m_mbuf->pkt_len-m_offset);
    }

    inline bool need_indirect_mbuf(uint16_t asked_pkt_size){
        /* need indirect if we need partial packet*/
        if ( (m_offset!=0) || (m_mbuf->pkt_len!=asked_pkt_size) ) {
            return(true);
        }
        return(false);
    }

    /*uint16_t get_mbuf_ptr(){
        m_mbuf->pkt_len
    } */

    void Dump(FILE *fd);

public:
    uint16_t m_offset;  /* offset to the packet */
};



typedef std::vector<CMbufObject> vec_mbuf_t;

class CMbufBuffer {
public:
    void Create(uint32_t blk_size);
    void Delete();

    /**
     * the blocks must be in the same size 
     * 
     * @param obj
     */
    void Add_mbuf(CMbufObject & obj);
    bool Verify();
    void Dump(FILE *fd);


    /**
     * return pointer of mbuf in offset and res size
     * 
     * @param offset
     * @param res
     * 
     */
    void get_by_offset(uint32_t offset,CBufMbufRef & res);

private:
    void Reset();
public:
    vec_mbuf_t   m_vec;
    uint32_t     m_num_pkt;      // number of packet in the vector, for speed can be taken from vec.size()
    uint32_t     m_blk_size;     // the size of each mbuf ( maximum , except the last block */
    uint32_t     m_t_bytes;      // the total bytes in all mbufs 
};


class CTcpApp {
public:

    CMbufBuffer m_write_buf;
};

class CTcpPerThreadCtx;

class   CTcpSockBuf {

public:
    void Create(uint32_t max_size){
        sb_hiwat=max_size;
        m_head_offset=0;
        sb_cc=0;
        sb_flags=0;
    }
    void Delete(){
    }

    inline bool is_empty(void){
        return (sb_cc==0);
    }

    uint32_t get_sbspace(void){
        return (sb_hiwat-sb_cc);
    }

    void sb_start_new_buffer(){
        m_head_offset=0;
    }

    void sbappend(uint32_t bytes){
        sb_cc+=bytes;
    }

    void sbdrop_all(){
        sbdrop(sb_cc);
    }

    /* drop x bytes from tail */
    void sbdrop(int len){
        assert(sb_cc>=len);
        m_head_offset+=len;
        sb_cc-=len;
        sb_flags |=SB_DROPCALL;
    }

    void get_by_offset(CTcpPerThreadCtx *ctx,uint32_t offset,CBufMbufRef & res){
        m_app->m_write_buf.get_by_offset(m_head_offset+offset,res);
        //callback ctx->
        //get_offset( m_head_offset+offset,CBufMbufRef & res);
    }

public:

    uint32_t    sb_cc;      /* actual chars in buffer */
    uint32_t    sb_hiwat;   /* max actual char count */
    uint32_t    sb_flags;   /* flags, see below */
    uint32_t    m_head_offset; /* offset to head*/

    CTcpApp  *  m_app;
};


/*
 * Kernel structure per socket.
 * Contains send and receive buffer queues,
 * handle on protocol and pointer to protocol
 * private data and error information.
 */
struct tcp_socket {
    short   so_options; 
    int     so_error;
    int     so_state;
/*
 * Variables for socket buffering.
 */
    struct  sockbuf so_rcv;
    CTcpSockBuf     so_snd;

};



#endif


