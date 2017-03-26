#ifndef BSD44_BR_TCP_SOCKET
#define BSD44_BR_TCP_SOCKET

/*
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)tcp_socket.h	8.1 (Berkeley) 6/10/93
 */
                 

struct	sockbuf {
    uint32_t	sb_cc;		/* actual chars in buffer */
    uint32_t	sb_hiwat;	/* max actual char count */
    uint32_t	sb_mbcnt;	/* chars of mbufs used */
    uint32_t	sb_mbmax;	/* max chars of mbufs to use */
    //int64_t	    sb_lowat;	/* low water mark */
    struct	rte_mbuf *sb_mb;	/* the mbuf chain */
    short	sb_flags;	/* flags, see below */
    //short	sb_timeo;	/* timeout for read/write */
};

// so_state
#define US_SS_CANTRCVMORE  1

/*
 * Kernel structure per socket.
 * Contains send and receive buffer queues,
 * handle on protocol and pointer to protocol
 * private data and error information.
 */
struct tcp_socket {
	short	so_options;	
    int     so_error;
    int     so_state;
/*
 * Variables for socket buffering.
 */
    struct	sockbuf so_rcv;
    struct	sockbuf so_snd;

#define	SB_MAX		(256*1024)	/* default for max chars in sockbuf */
#define	SB_LOCK		0x01		/* lock on data queue */
#define	SB_WANT		0x02		/* someone is waiting to lock */
#define	SB_WAIT		0x04		/* someone is waiting for data/space */
#define	SB_SEL		0x08		/* someone is selecting */
#define	SB_ASYNC	0x10		/* ASYNC I/O, need signals */
#define	SB_NOTIFY	(SB_WAIT|SB_SEL|SB_ASYNC)
#define	SB_NOINTR	0x40		/* operations not interruptible */

};

/*
 * Macros for sockets and socket buffering.
 */

/*
 * How much space is there in a socket buffer (so->so_snd or so->so_rcv)?
 * This is problematical if the fields are unsigned, as the space might
 * still be negative (cc > hiwat or mbcnt > mbmax).  Should detect
 * overflow and return 0.  Should use "lmin" but it doesn't exist now.
 */
#define	sbspace(sb) \
    ((long) imin((int)((sb)->sb_hiwat - (sb)->sb_cc), \
	 (int)((sb)->sb_mbmax - (sb)->sb_mbcnt)))


#define	US_SO_DEBUG	0x0001		/* turn on debugging info recording */
#define	US_SO_ACCEPTCONN	0x0002		/* socket has had listen() */
#define	US_SO_REUSEADDR	0x0004		/* allow local address reuse */
#define	US_SO_KEEPALIVE	0x0008		/* keep connections alive */
#define	US_SO_DONTROUTE	0x0010		/* just use interface addresses */
#define	US_SO_BROADCAST	0x0020		/* permit sending of broadcast msgs */

#define	US_SO_LINGER	0x0080		/* linger on close if data present */
#define	US_SO_OOBINLINE	0x0100		/* leave received OOB data in line */
//#if __BSD_VISIBLE

#define	US_SO_REUSEPORT	0x0200		/* allow local address & port reuse */

#define	US_SO_TIMESTAMP	0x0400		/* timestamp received dgram traffic */
#define	US_SO_NOSIGPIPE	0x0800		/* no SIGPIPE from EPIPE */
#define	US_SO_ACCEPTFILTER	0x1000		/* there is an accept filter */
#define	US_SO_BINTIME	0x2000		/* timestamp received dgram traffic */
//#endif
#define	US_SO_NO_OFFLOAD	0x4000		/* socket cannot be offloaded */
#define	US_SO_NO_DDP	0x8000		/* disable direct data placement */

struct tcp_socket * sonewconn(struct tcp_socket *head, int connstatus);

void	sbdrop(struct sockbuf *sb, int len);
void sowwakeup(struct tcp_socket *so);
void sorwakeup(struct tcp_socket *so);


void	soisdisconnected(struct tcp_socket *so);

void	sbappend(struct sockbuf *sb, struct rte_mbuf *m);

#ifdef FIXME
#define	sorwakeup(so)	{ sowakeup((so), &(so)->so_rcv); \
			  if ((so)->so_upcall) \
			    (*((so)->so_upcall))((so), (so)->so_upcallarg, M_DONTWAIT); \
                
			}
#else


#endif            

void	soisconnected(struct tcp_socket *so);
void	socantrcvmore(struct tcp_socket *so);


//#define	IN_CLASSD(i)		(((u_int32_t)(i) & 0xf0000000) == 0xe0000000)
/* send/received as ip/link-level broadcast */
#define	TCP_PKT_M_BCAST		(1ULL << 1) 

/* send/received as iplink-level multicast */
#define	TCP_PKT_M_MCAST		(1ULL << 2) 
#define	TCP_PKT_M_B_OR_MCAST	(TCP_PKT_M_BCAST  | TCP_PKT_M_MCAST)

typedef uint8_t tcp_l2_pkt_flags_t ;

int	soabort(struct tcp_socket *so);


#define TCP_US_ECONNREFUSED  (7)
#define TCP_US_ECONNRESET    (8)

#define TCP_US_ETIMEDOUT     (9)
#define TCP_US_ECONNABORTED  (10)
#define TCP_US_ENOBUFS       (11)



#endif


