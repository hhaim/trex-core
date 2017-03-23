#ifndef BSD44_BR_TCPIP
#define BSD44_BR_TCPIP

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
 *	@(#)tcpip.h	8.1 (Berkeley) 6/10/93
 */

/*
 * Tcp+ip header, after ip options removed.
 */

#include <common/Network/Packet/TcpHeader.h>
#include <common/Network/Packet/UdpHeader.h>
#include <common/Network/Packet/IcmpHeader.h>
#include <common/Network/Packet/IPHeader.h>
#include <common/Network/Packet/IPv6Header.h>
#include <common/Network/Packet/EthernetHeader.h>


/*
 * Definitions for internet protocol version 4.
 *
 * Per RFC 791, September 1981.
 */
#define	IPVERSION	4

/*
 * Structure of an internet header, naked of options.
 */
struct ip {
#if BYTE_ORDER == LITTLE_ENDIAN
	u_char	ip_hl: 4,		/* header length */
		ip_v: 4;			/* version */
#endif
#if BYTE_ORDER == BIG_ENDIAN
	u_char	ip_v: 4,			/* version */
		ip_hl: 4;		/* header length */
#endif
	u_char	ip_tos;			/* type of service */
	u_short	ip_len;			/* total length */
	u_short	ip_id;			/* identification */
	u_short	ip_off;			/* fragment offset field */
#define	IP_RF 0x8000			/* reserved fragment flag */
#define	IP_DF 0x4000			/* dont fragment flag */
#define	IP_MF 0x2000			/* more fragments flag */
#define	IP_OFFMASK 0x1fff		/* mask for fragmenting bits */
	u_char	ip_ttl;			/* time to live */
	u_char	ip_p;			/* protocol */
	u_short	ip_sum;			/* checksum */
	uint32_t ip_src;
    uint32_t ip_dst;	    /* source and dest address */
} __attribute__((__packed__)) ;

#define	IP_MAXPACKET	65535		/* maximum packet size */

/*
 * Definitions for IP type of service (ip_tos).
 */
#define	IPTOS_LOWDELAY		0x10
#define	IPTOS_THROUGHPUT	0x08
#define	IPTOS_RELIABILITY	0x04
#define	IPTOS_MINCOST		0x02

/*
 * Definitions for IP precedence (also in ip_tos) (hopefully unused).
 */
#define	IPTOS_PREC_NETCONTROL		0xe0
#define	IPTOS_PREC_INTERNETCONTROL	0xc0
#define	IPTOS_PREC_CRITIC_ECP		0xa0
#define	IPTOS_PREC_FLASHOVERRIDE	0x80
#define	IPTOS_PREC_FLASH		0x60
#define	IPTOS_PREC_IMMEDIATE		0x40
#define	IPTOS_PREC_PRIORITY		0x20
#define	IPTOS_PREC_ROUTINE		0x00

/*
 * Definitions for DiffServ Codepoints as per RFC2474
 */
#define	IPTOS_DSCP_CS0		0x00
#define	IPTOS_DSCP_CS1		0x20
#define	IPTOS_DSCP_AF11		0x28
#define	IPTOS_DSCP_AF12		0x30
#define	IPTOS_DSCP_AF13		0x38
#define	IPTOS_DSCP_CS2		0x40
#define	IPTOS_DSCP_AF21		0x48
#define	IPTOS_DSCP_AF22		0x50
#define	IPTOS_DSCP_AF23		0x58
#define	IPTOS_DSCP_CS3		0x60
#define	IPTOS_DSCP_AF31		0x68
#define	IPTOS_DSCP_AF32		0x70
#define	IPTOS_DSCP_AF33		0x78
#define	IPTOS_DSCP_CS4		0x80
#define	IPTOS_DSCP_AF41		0x88
#define	IPTOS_DSCP_AF42		0x90
#define	IPTOS_DSCP_AF43		0x98
#define	IPTOS_DSCP_CS5		0xa0
#define	IPTOS_DSCP_EF		0xb8
#define	IPTOS_DSCP_CS6		0xc0
#define	IPTOS_DSCP_CS7		0xe0

          
     
struct ipovly {
	u_char	ih_x1[9];		/* (unused) */
	u_char	ih_pr;			/* protocol */
	u_short	ih_len;			/* protocol length */
	uint32_t ih_src;		/* source internet address */
	uint32_t ih_dst;		/* destination internet address */
};
 
struct tcpiphdr {
	struct 	ipovly ti_i;		/* overlaid ip structure */
	struct	    tcphdr ti_t;		/* tcp header */
    uint16_t    l7_len;
};

#define	ti_next		ti_i.ih_next
#define	ti_prev		ti_i.ih_prev
#define	ti_x1		ti_i.ih_x1
#define	ti_pr		ti_i.ih_pr
#define	ti_len		ti_i.ih_len
#define	ti_src		ti_i.ih_src
#define	ti_dst		ti_i.ih_dst
#define	ti_sport	ti_t.th_sport
#define	ti_dport	ti_t.th_dport
#define	ti_seq		ti_t.th_seq
#define	ti_ack		ti_t.th_ack
#define	ti_x2		ti_t.th_x2
#define	ti_off		ti_t.th_off
#define	ti_flags	ti_t.th_flags
#define	ti_win		ti_t.th_win
#define	ti_sum		ti_t.th_sum
#define	ti_urp		ti_t.th_urp



#endif
