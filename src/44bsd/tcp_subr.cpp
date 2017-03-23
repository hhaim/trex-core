/*
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1995
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
 *	@(#)tcp_subr.c	8.2 (Berkeley) 5/24/95
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <common/basic_utils.h>
#include <stdlib.h>
#include "tcp_fsm.h"
#include "tcp_seq.h"
#include "tcp_timer.h"
#include "tcp_var.h"
#include "tcpip.h"
#include "tcp_debug.h"
#include "tcp_socket.h"
           

//extern	struct inpcb *tcp_last_inpcb;


bool CTcpPerThreadCtx::Create(void){
    sb_max = SB_MAX;		/* patchable */
    tcp_mssdflt = TCP_MSS;
    tcp_rttdflt = TCPTV_SRTTDFLT / PR_SLOWHZ;
    tcp_do_rfc1323 = 1;
    m_tick=0;
    tcp_keepidle = TCPTV_KEEP_IDLE;
    tcp_keepintvl = TCPTV_KEEPINTVL;
    tcp_keepcnt = TCPTV_KEEPCNT;		/* max idle probes */
    tcp_maxpersistidle = TCPTV_KEEP_IDLE;	/* max idle time in persist */
    tcp_maxidle=0;
    tcp_ttl=0;
    tcp_iss = rand();	/* wrong, but better than a constant */

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

/*
 * Create template to be used to send tcp packets on a connection.
 * Call after host entry created, allocates an mbuf and fills
 * in a skeletal tcp/ip header, minimizing the amount of work
 * necessary when the connection is used.
 */
void tcp_template(struct tcpcb *tp){
    /* TCPIPV6*/

    const uint8_t default_ipv4_header[] = {
        0x00,0x00,0x00,0x01,0x0,0x0,  // Ethr
        0x00,0x00,0x00,0x02,0x0,0x0,
        0x08,00,


        0x45,0x00,0x00,0x00,          //Ipv4
        0x00,0x00,0x40,0x00,
        0x7f,0x06,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,

        0x00, 0x00, 0x00, 0x00, // src, dst ports  //TCP
        0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, // seq num, ack num
        0x50, 0x00, 0x00, 0x00, // Header size, flags, window size
        0x00, 0x00, 0x00, 0x00 // checksum ,urgent pointer
    };


    uint8_t *p=tp->template_pkt;
    memcpy(p,default_ipv4_header,sizeof(default_ipv4_header) );
    /* set default value */
    p+=14;
    IPHeader *lpIpv4=(IPHeader *)(p);
    lpIpv4->setDestIp(0x48000001);
    lpIpv4->setSourceIp(0x16000001);
    p+=20;
    TCPHeader *lpTCP=(TCPHeader *)(p);
    lpTCP->setSourcePort(1024);
    lpTCP->setDestPort(80);
}


/*
 * Send a single message to the TCP at address specified by
 * the given TCP/IP header.  If m == 0, then we make a copy
 * of the tcpiphdr at ti and send directly to the addressed host.
 * This is used to force keep alive messages out using the TCP
 * template for a connection tp->t_template.  If flags are given
 * then we send a message back to the TCP which originated the
 * segment ti, and discard the mbuf containing it and any other
 * attached mbufs.
 *
 * In any case the ack and sequence number of the transmitted
 * segment are as specified by the parameters.
 */
void
tcp_respond(struct tcpcb *tp, 
            struct tcpiphdr *ti, 
            struct rte_mbuf *m, 
            tcp_seq ack, 
            tcp_seq seq, 
            int flags){
#if MBUF_BUILD
	int tlen;
	int win = 0;
	if (tp) {
		win = sbspace(&tp->m_socket.so_rcv);
	}


	if (m == 0) {
		m = m_gethdr(M_DONTWAIT, MT_HEADER);
		if (m == NULL)
			return;
#ifdef TCP_COMPAT_42
		tlen = 1;
#else
		tlen = 0;
#endif
		m->m_data += max_linkhdr;
		*mtod(m, struct tcpiphdr *) = *ti;
		ti = mtod(m, struct tcpiphdr *);
		flags = TH_ACK;
	} else {
		m_freem(m->m_next);
		m->m_next = 0;
		m->m_data = (caddr_t)ti;
		m->m_len = sizeof (struct tcpiphdr);
		tlen = 0;
#define xchg(a,b,type) { type t; t=a; a=b; b=t; }
		xchg(ti->ti_dst.s_addr, ti->ti_src.s_addr, u_long);
		xchg(ti->ti_dport, ti->ti_sport, u_short);
#undef xchg
	}
	ti->ti_len = htons((u_short)(sizeof (struct tcphdr) + tlen));
	tlen += sizeof (struct tcpiphdr);
	m->m_len = tlen;
	m->m_pkthdr.len = tlen;
	m->m_pkthdr.rcvif = (struct ifnet *) 0;
	ti->ti_next = ti->ti_prev = 0;
	ti->ti_x1 = 0;
	ti->ti_seq = htonl(seq);
	ti->ti_ack = htonl(ack);
	ti->ti_x2 = 0;
	ti->ti_off = sizeof (struct tcphdr) >> 2;
	ti->ti_flags = flags;
	if (tp)
		ti->ti_win = htons((u_short) (win >> tp->rcv_scale));
	else
		ti->ti_win = htons((u_short)win);
	ti->ti_urp = 0;
	ti->ti_sum = 0;
	ti->ti_sum = in_cksum(m, tlen);
	((struct ip *)ti)->ip_len = tlen;
	((struct ip *)ti)->ip_ttl = 0x80;
	(void) ip_output(m, NULL, ro, 0, NULL);
#endif
}



/*
 * Create a new TCP control block, making an
 * empty reassembly queue and hooking it to the argument
 * protocol control block.
 */
struct tcpcb * tcp_newtcpcb(CTcpPerThreadCtx * ctx){

    struct tcpcb *tp;

	tp = (struct tcpcb *)malloc(sizeof(*tp));
	if (tp == NULL)
		return ((struct tcpcb *)0);
    /* TCP_OPTIM  */
	memset((char *) tp, 0,sizeof(struct tcpcb));

	tp->t_maxseg = ctx->tcp_mssdflt;

	tp->t_flags = ctx->tcp_do_rfc1323 ? (TF_REQ_SCALE|TF_REQ_TSTMP) : 0;

    /*
	 * Init srtt to TCPTV_SRTTBASE (0), so we can tell that we have no
	 * rtt estimate.  Set rttvar so that srtt + 2 * rttvar gives
	 * reasonable initial retransmit time.
	 */
	tp->t_srtt = TCPTV_SRTTBASE;
	tp->t_rttvar = ctx->tcp_rttdflt * PR_SLOWHZ << 2;
	tp->t_rttmin = TCPTV_MIN;
	TCPT_RANGESET(tp->t_rxtcur, 
	    ((TCPTV_SRTTBASE >> 2) + (TCPTV_SRTTDFLT << 2)) >> 1,
	    TCPTV_MIN, TCPTV_REXMTMAX);
	tp->snd_cwnd = TCP_MAXWIN << TCP_MAX_WINSHIFT;
	tp->snd_ssthresh = TCP_MAXWIN << TCP_MAX_WINSHIFT;
	return (tp);
}

/*
 * Drop a TCP connection, reporting
 * the specified error.  If connection is synchronized,
 * then send a RST to peer.
 */
struct tcpcb *
tcp_drop_now(CTcpPerThreadCtx * ctx,
         struct tcpcb *tp, 
         int errno){
	struct tcp_socket *so = &tp->m_socket;

	if (TCPS_HAVERCVDSYN(tp->t_state)) {
		tp->t_state = TCPS_CLOSED;
		(void) tcp_output(ctx,tp);
        INC_STAT(ctx,tcps_drops);
	} else{
        INC_STAT(ctx,tcps_conndrops);
    }
	if (errno == ETIMEDOUT && tp->t_softerror)
		errno = tp->t_softerror;
	so->so_error = errno;
	return (tcp_close(ctx,tp));
}


/*
 * Close a TCP control block:
 *	discard all space held by the tcp
 *	discard internet protocol block
 *	wake up any sleepers
 */
struct tcpcb *
tcp_close(CTcpPerThreadCtx * ctx,
          struct tcpcb *tp){

    //register struct tcpiphdr *t;
	//register struct rte_mbuf *m;

// no need for that , RTT will be calculated again for each flow 
#ifdef RTV_RTT
	register struct rtentry *rt;

	/*
	 * If we sent enough data to get some meaningful characteristics,
	 * save them in the routing entry.  'Enough' is arbitrarily 
	 * defined as the sendpipesize (default 4K) * 16.  This would
	 * give us 16 rtt samples assuming we only get one sample per
	 * window (the usual case on a long haul net).  16 samples is
	 * enough for the srtt filter to converge to within 5% of the correct
	 * value; fewer samples and we could save a very bogus rtt.
	 *
	 * Don't update the default route's characteristics and don't
	 * update anything that the user "locked".
	 */
	if (SEQ_LT(tp->iss + so->so_snd.sb_hiwat * 16, tp->snd_max) &&
	    (rt = inp->inp_route.ro_rt) &&
	    ((struct sockaddr_in *)rt_key(rt))->sin_addr.s_addr != INADDR_ANY) {
		register u_long i;

		if ((rt->rt_rmx.rmx_locks & RTV_RTT) == 0) {
			i = tp->t_srtt *
			    (RTM_RTTUNIT / (PR_SLOWHZ * TCP_RTT_SCALE));
			if (rt->rt_rmx.rmx_rtt && i)
				/*
				 * filter this update to half the old & half
				 * the new values, converting scale.
				 * See route.h and tcp_var.h for a
				 * description of the scaling constants.
				 */
				rt->rt_rmx.rmx_rtt =
				    (rt->rt_rmx.rmx_rtt + i) / 2;
			else
				rt->rt_rmx.rmx_rtt = i;
		}
		if ((rt->rt_rmx.rmx_locks & RTV_RTTVAR) == 0) {
			i = tp->t_rttvar *
			    (RTM_RTTUNIT / (PR_SLOWHZ * TCP_RTTVAR_SCALE));
			if (rt->rt_rmx.rmx_rttvar && i)
				rt->rt_rmx.rmx_rttvar =
				    (rt->rt_rmx.rmx_rttvar + i) / 2;
			else
				rt->rt_rmx.rmx_rttvar = i;
		}
		/*
		 * update the pipelimit (ssthresh) if it has been updated
		 * already or if a pipesize was specified & the threshhold
		 * got below half the pipesize.  I.e., wait for bad news
		 * before we start updating, then update on both good
		 * and bad news.
		 */
		if ((rt->rt_rmx.rmx_locks & RTV_SSTHRESH) == 0 &&
		    (i = tp->snd_ssthresh) && rt->rt_rmx.rmx_ssthresh ||
		    i < (rt->rt_rmx.rmx_sendpipe / 2)) {
			/*
			 * convert the limit from user data bytes to
			 * packets then to packet data bytes.
			 */
			i = (i + tp->t_maxseg / 2) / tp->t_maxseg;
			if (i < 2)
				i = 2;
			i *= (u_long)(tp->t_maxseg + sizeof (struct tcpiphdr));
			if (rt->rt_rmx.rmx_ssthresh)
				rt->rt_rmx.rmx_ssthresh =
				    (rt->rt_rmx.rmx_ssthresh + i) / 2;
			else
				rt->rt_rmx.rmx_ssthresh = i;
		}
	}
#endif /* RTV_RTT */
	/* free the reassembly queue, if any */
#if TCP_SEG_QUEUE
	t = tp->seg_next;
	while (t != (struct tcpiphdr *)tp) {
		t = (struct tcpiphdr *)t->ti_next;
		m = REASS_MBUF((struct tcpiphdr *)t->ti_prev);
		remque(t->ti_prev);
		m_freem(m);
	}
#endif

    free(tp);
    INC_STAT(ctx,tcps_closed);
	return ((struct tcpcb *)0);
}

void tcp_drain(){

}

#if 0
/*
 * Notify a tcp user of an asynchronous error;
 * store error as soft error, but wake up user
 * (for now, won't do anything until can select for soft error).
 */
void
tcp_notify(inp, error)
	struct inpcb *inp;
	int error;
{
	register struct tcpcb *tp = (struct tcpcb *)inp->inp_ppcb;
	register struct socket *so = inp->inp_socket;

	/*
	 * Ignore some errors if we are hooked up.
	 * If connection hasn't completed, has retransmitted several times,
	 * and receives a second error, give up now.  This is better
	 * than waiting a long time to establish a connection that
	 * can never complete.
	 */
	if (tp->t_state == TCPS_ESTABLISHED &&
	     (error == EHOSTUNREACH || error == ENETUNREACH ||
	      error == EHOSTDOWN)) {
		return;
	} else if (tp->t_state < TCPS_ESTABLISHED && tp->t_rxtshift > 3 &&
	    tp->t_softerror)
		so->so_error = error;
	else 
		tp->t_softerror = error;
	wakeup((caddr_t) &so->so_timeo);
	sorwakeup(so);
	sowwakeup(so);
}

void
tcp_ctlinput(cmd, sa, ip)
	int cmd;
	struct sockaddr *sa;
	register struct ip *ip;
{
	register struct tcphdr *th;
	extern struct in_addr zeroin_addr;
	extern u_char inetctlerrmap[];
	void (*notify) __P((struct inpcb *, int)) = tcp_notify;

	if (cmd == PRC_QUENCH)
		notify = tcp_quench;
	else if (!PRC_IS_REDIRECT(cmd) &&
		 ((unsigned)cmd > PRC_NCMDS || inetctlerrmap[cmd] == 0))
		return;
	if (ip) {
		th = (struct tcphdr *)((caddr_t)ip + (ip->ip_hl << 2));
		in_pcbnotify(&tcb, sa, th->th_dport, ip->ip_src, th->th_sport,
			cmd, notify);
	} else
		in_pcbnotify(&tcb, sa, 0, zeroin_addr, 0, cmd, notify);
}
#endif
/*
 * When a source quench is received, close congestion window
 * to one segment.  We will gradually open it again as we proceed.
 */
void tcp_quench(struct tcpcb *tp){

    tp->snd_cwnd = tp->t_maxseg;
}


