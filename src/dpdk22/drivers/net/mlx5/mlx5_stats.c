/*-
 *   BSD LICENSE
 *
 *   Copyright 2015 6WIND S.A.
 *   Copyright 2015 Mellanox.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of 6WIND S.A. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* DPDK headers don't like -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-pedantic"
#endif
#include <rte_ethdev.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-pedantic"
#endif

#define RX_VPORT_UNICAST_BYTES 31
#define RX_VPORT_MULTICAST_BYTES 35
#define RX_VPORT_BROADCOAST_BYTES 39

#define RX_VPORT_UNICAST_PACKETS 30
#define RX_VPORT_MULTICAST_PACKETS 34
#define RX_VPORT_BROADCOAST_PACKETS 38


#define TX_VPORT_UNICAST_BYTES 33
#define TX_VPORT_MULTICAST_BYTES 37
#define TX_VPORT_BROADCOAST_BYTES 41

#define TX_VPORT_UNICAST_PACKETS 32
#define TX_VPORT_MULTICAST_PACKETS 36
#define TX_VPORT_BROADCOAST_PACKETS 40

#define RX_WQE_ERR 23
#define RX_CRC_ERRORS_PHY 52
#define RX_IN_RANGE_LEN_ERRORS_PHY 59
#define RX_SYMBOL_ERR_PHY 62

#define TX_ERRORS_PHY 70



#include "mlx5.h"
#include "mlx5_rxtx.h"
#include "mlx5_defs.h"

#include <linux/ethtool.h>
#include <linux/sockios.h>
/**
 * DPDK callback to get device statistics.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[out] stats
 *   Stats structure output buffer.
 */
void
mlx5_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct priv *priv = mlx5_get_priv(dev);
	struct rte_eth_stats tmp = {0};
	unsigned int i;
	unsigned int idx;
        char ifname[IF_NAMESIZE];

        struct ifreq ifr;

	struct ethtool_stats    *et_stats   = NULL;
        struct ethtool_drvinfo drvinfo;
	struct ethtool_gstrings *strings = NULL;

	unsigned int n_stats, sz_str, sz_stats;


	int rx_vport_unicast_bytes = 0;
	int rx_vport_multicast_bytes = 0;
	int rx_vport_broadcast_bytes = 0;
	int rx_vport_unicast_packets = 0;
	int rx_vport_multicast_packets = 0;
	int rx_vport_broadcast_packets = 0;
	int tx_vport_unicast_bytes = 0;
	int tx_vport_multicast_bytes = 0;
	int tx_vport_broadcast_bytes = 0;
	int tx_vport_unicast_packets = 0;
	int tx_vport_multicast_packets = 0;
	int tx_vport_broadcast_packets = 0;
	int rx_wqe_err = 0;
	int rx_crc_errors_phy = 0;
	int rx_in_range_len_errors_phy = 0;
	int rx_symbol_err_phy = 0;
	int tx_errors_phy = 0;

	priv_lock(priv);


        if (priv_get_ifname(priv, &ifname)) {
                WARN("unable to get interface name");
                return;
        }
        /* How many statistics are available ? */
        drvinfo.cmd = ETHTOOL_GDRVINFO;
        ifr.ifr_data = (caddr_t) &drvinfo;
        if (priv_ifreq(priv, SIOCETHTOOL, &ifr) != 0) {
                WARN("unable to get driver info for %s", ifname);
                return;
        }

        n_stats = drvinfo.n_stats;
        if (n_stats < 1) {
                WARN("no statistics available for %s", ifname);
                return;
        }


	/* Allocate memory to grab stat names and values */ 
	sz_str = n_stats * ETH_GSTRING_LEN; 
	sz_stats = n_stats * sizeof(uint64_t); 
	strings = calloc(1, sz_str + sizeof(struct ethtool_gstrings)); 
	if (!strings) { 
		WARN("unable to allocate memory for strings"); 
		return;
	} 

	et_stats = calloc(1, sz_stats + sizeof(struct ethtool_stats)); 
	if (!et_stats) { 
		WARN("unable to allocate memory for stats"); 
	} 

	strings->cmd = ETHTOOL_GSTRINGS; 
	strings->string_set = ETH_SS_STATS; 
	strings->len = n_stats; 
	ifr.ifr_data = (caddr_t) strings; 
	if (priv_ifreq(priv, SIOCETHTOOL, &ifr) != 0) { 
		WARN("unable to get statistic names for %s", ifname); 
		return;
	} 

        for (i = 0; (i != n_stats); ++i) {
		const char * curr_string = (const char*) &(strings->data[i * ETH_GSTRING_LEN]);
                if (!strcmp("rx_vport_unicast_bytes", curr_string)) rx_vport_unicast_bytes = i;
                if (!strcmp("rx_vport_multicast_bytes", curr_string)) rx_vport_multicast_bytes = i;
		if (!strcmp("rx_vport_broadcast_bytes", curr_string)) rx_vport_broadcast_bytes = i;

                if (!strcmp("rx_vport_unicast_packets", curr_string)) rx_vport_unicast_packets = i;
                if (!strcmp("rx_vport_multicast_packets", curr_string)) rx_vport_multicast_packets = i;
                if (!strcmp("rx_vport_broadcast_packets", curr_string)) rx_vport_broadcast_packets = i;

                if (!strcmp("tx_vport_unicast_bytes", curr_string)) tx_vport_unicast_bytes = i;
                if (!strcmp("tx_vport_multicast_bytes", curr_string)) tx_vport_multicast_bytes = i;
                if (!strcmp("tx_vport_broadcast_bytes", curr_string)) tx_vport_broadcast_bytes = i;

                if (!strcmp("tx_vport_unicast_packets", curr_string)) tx_vport_unicast_packets = i;
                if (!strcmp("tx_vport_multicast_packets", curr_string)) tx_vport_multicast_packets = i;
                if (!strcmp("tx_vport_broadcast_packets", curr_string)) tx_vport_broadcast_packets = i;

                if (!strcmp("rx_wqe_err", curr_string)) rx_wqe_err = i;
                if (!strcmp("rx_crc_errors_phy", curr_string)) rx_crc_errors_phy = i;
                if (!strcmp("rx_in_range_len_errors_phy", curr_string)) rx_in_range_len_errors_phy = i;
                if (!strcmp("rx_symbol_err_phy", curr_string)) rx_symbol_err_phy = i;

                if (!strcmp("tx_errors_phy", curr_string)) tx_errors_phy = i;
	}

	/* Grab stat values */
	et_stats->cmd = ETHTOOL_GSTATS;
	et_stats->n_stats = n_stats;

	ifr.ifr_data = (caddr_t) et_stats;

	if (priv_ifreq(priv, SIOCETHTOOL, &ifr) != 0) { 
		WARN("unable to get statistic values for %s", ifname); 
	}

	if (et_stats) free(et_stats);

        if (!rx_vport_unicast_bytes ||
	    !rx_vport_multicast_bytes ||
	    !rx_vport_broadcast_bytes || 
	    !rx_vport_unicast_packets ||
	    !rx_vport_multicast_packets ||
	    !rx_vport_broadcast_packets ||
	    !tx_vport_unicast_bytes || 
	    !tx_vport_multicast_bytes ||
	    !tx_vport_broadcast_bytes ||
	    !tx_vport_unicast_packets ||
	    !tx_vport_multicast_packets ||
	    !tx_vport_broadcast_packets ||
	    !rx_wqe_err ||
	    !rx_crc_errors_phy ||
	    !rx_in_range_len_errors_phy) {
                WARN("Counters are not recognized %s", ifname);
	} else {
		tmp.ibytes += et_stats->data[rx_vport_unicast_bytes];// +
//			      et_stats->data[rx_vport_multicast_bytes] +
//			      et_stats->data[rx_vport_broadcast_bytes];
        
		tmp.ipackets += et_stats->data[rx_vport_unicast_packets] ;// +
//				et_stats->data[rx_vport_multicast_packets] +
//				et_stats->data[rx_vport_broadcast_packets];
			
		tmp.ierrors += 	et_stats->data[rx_wqe_err] +
				et_stats->data[rx_crc_errors_phy] +
				et_stats->data[rx_in_range_len_errors_phy] +
				et_stats->data[rx_symbol_err_phy];

		tmp.obytes += et_stats->data[tx_vport_unicast_bytes];// +
//			      et_stats->data[tx_vport_multicast_bytes] +
//			      et_stats->data[tx_vport_broadcast_bytes];

		tmp.opackets += et_stats->data[tx_vport_unicast_packets] ;// +
//				et_stats->data[tx_vport_multicast_packets] +
//				et_stats->data[tx_vport_broadcast_packets];

		tmp.oerrors += et_stats->data[tx_errors_phy];
	}

	for (i = 0; (i != priv->rxqs_n); ++i) {
		struct rxq *rxq = (*priv->rxqs)[i];

		if (rxq == NULL)
			continue;
		idx = rxq->stats.idx;
		if (idx < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
#ifdef MLX5_PMD_SOFT_COUNTERS
			tmp.q_ipackets[idx] += rxq->stats.ipackets;
			tmp.q_ibytes[idx] += rxq->stats.ibytes;
#endif
			tmp.q_errors[idx] += (rxq->stats.idropped +
					     rxq->stats.rx_nombuf);
		}
		tmp.rx_nombuf += rxq->stats.rx_nombuf;
	}
	for (i = 0; (i != priv->txqs_n); ++i) {
		struct txq *txq = (*priv->txqs)[i];

		if (txq == NULL)
			continue;
		idx = txq->stats.idx;
		if (idx < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
#ifdef MLX5_PMD_SOFT_COUNTERS
			tmp.q_opackets[idx] += txq->stats.opackets;
			tmp.q_obytes[idx] += txq->stats.obytes;
#endif
			tmp.q_errors[idx] += txq->stats.odropped;
		}
	}
#ifndef MLX5_PMD_SOFT_COUNTERS
	/* FIXME: retrieve and add hardware counters. */
#endif
	*stats = tmp;
	priv_unlock(priv);
}

/**
 * DPDK callback to clear device statistics.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
void
mlx5_stats_reset(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	unsigned int i;
	unsigned int idx;

	priv_lock(priv);
	for (i = 0; (i != priv->rxqs_n); ++i) {
		if ((*priv->rxqs)[i] == NULL)
			continue;
		idx = (*priv->rxqs)[i]->stats.idx;
		(*priv->rxqs)[i]->stats =
			(struct mlx5_rxq_stats){ .idx = idx };
	}
	for (i = 0; (i != priv->txqs_n); ++i) {
		if ((*priv->txqs)[i] == NULL)
			continue;
		idx = (*priv->txqs)[i]->stats.idx;
		(*priv->txqs)[i]->stats =
			(struct mlx5_txq_stats){ .idx = idx };
	}
#ifndef MLX5_PMD_SOFT_COUNTERS
	/* FIXME: reset hardware counters. */
#endif
	priv_unlock(priv);
}
