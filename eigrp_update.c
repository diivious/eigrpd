/*
 * Sending and Receiving EIGRP Update Packets.
 * Copyright (C) 2013-2016
 * Authors:
 *   Donnie Savage
 *   Jan Janovic
 *   Matej Perina
 *   Peter Orsag
 *   Peter Paluch
 *   Frantisek Gazo
 *   Tomas Hvorkovy
 *   Martin Kontsek
 *   Lukas Koribsky
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; see the file COPYING; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <zebra.h>

#include "thread.h"
#include "memory.h"
#include "linklist.h"
#include "prefix.h"
#include "if.h"
#include "table.h"
#include "sockunion.h"
#include "stream.h"
#include "log.h"
#include "sockopt.h"
#include "checksum.h"
#include "md5.h"
#include "vty.h"
#include "plist.h"
#include "plist_int.h"
#include "routemap.h"
#include "vty.h"

#include "eigrpd/eigrp_structs.h"
#include "eigrpd/eigrpd.h"
#include "eigrpd/eigrp_interface.h"
#include "eigrpd/eigrp_neighbor.h"
#include "eigrpd/eigrp_packet.h"
#include "eigrpd/eigrp_zebra.h"
#include "eigrpd/eigrp_vty.h"
#include "eigrpd/eigrp_dump.h"
#include "eigrpd/eigrp_macros.h"
#include "eigrpd/eigrp_topology.h"
#include "eigrpd/eigrp_fsm.h"
#include "eigrpd/eigrp_network.h"
#include "eigrpd/eigrp_memory.h"
#include "eigrpd/eigrp_metric.h"

bool eigrp_update_prefix_apply(struct eigrp *eigrp, eigrp_interface_t *ei,
			       int in, struct prefix *prefix)
{
	struct access_list *alist;
	struct prefix_list *plist;

	alist = eigrp->list[in];
	if (alist && access_list_apply(alist, prefix) == FILTER_DENY)
		return true;

	plist = eigrp->prefix[in];
	if (plist && prefix_list_apply(plist, prefix) == PREFIX_DENY)
		return true;

	alist = ei->list[in];
	if (alist && access_list_apply(alist, prefix) == FILTER_DENY)
		return true;

	plist = ei->prefix[in];
	if (plist && prefix_list_apply(plist, prefix) == PREFIX_DENY)
		return true;

	return false;
}

/**
 * @fn remove_received_prefix_gr
 *
 * @param[in]		nbr_prefixes	List of neighbor prefixes
 * @param[in]		recv_prefix 	Prefix which needs to be removed from
 * list
 *
 * @return void
 *
 * @par
 * Function is used for removing received prefix
 * from list of neighbor prefixes
 */
static void remove_received_prefix_gr(struct list *nbr_prefixes,
				      eigrp_prefix_descriptor_t *recv_prefix)
{
	struct listnode *node1, *node11;
	eigrp_prefix_descriptor_t *prefix = NULL;

	/* iterate over all prefixes in list */
	for (ALL_LIST_ELEMENTS(nbr_prefixes, node1, node11, prefix)) {
		/* remove prefix from list if found */
		if (prefix == recv_prefix) {
			listnode_delete(nbr_prefixes, prefix);
		}
	}
}

/**
 * @fn eigrp_update_receive_GR_ask
 *
 * @param[in]		eigrp			EIGRP process
 * @param[in]		nbr 			Neighbor update of who we
 * received
 * @param[in]		nbr_prefixes 	Prefixes which weren't advertised
 *
 * @return void
 *
 * @par
 * Function is used for notifying FSM about prefixes which
 * weren't advertised by neighbor:
 * We will send message to FSM with prefix delay set to infinity.
 */
static void eigrp_update_receive_GR_ask(struct eigrp *eigrp,
					eigrp_neighbor_t *nbr,
					struct list *nbr_prefixes)
{
	struct listnode *node1;
	eigrp_prefix_descriptor_t *prefix;
	eigrp_fsm_action_message_t fsm_msg;

	/* iterate over all prefixes which weren't advertised by neighbor */
	for (ALL_LIST_ELEMENTS_RO(nbr_prefixes, node1, prefix)) {
		char buffer[PREFIX_STRLEN];
		zlog_debug(
			"GR receive: Neighbor not advertised %s",
			prefix2str(prefix->destination, buffer, PREFIX_STRLEN));

		fsm_msg.metrics = prefix->reported_metric;
		/* set delay to MAX */
		fsm_msg.metrics.delay = EIGRP_MAX_METRIC;

		eigrp_route_descriptor_t *route =
			eigrp_prefix_descriptor_lookup(prefix->entries, nbr);

		fsm_msg.packet_type = EIGRP_OPC_UPDATE;
		fsm_msg.eigrp = eigrp;
		fsm_msg.data_type = EIGRP_INT;
		fsm_msg.adv_router = nbr;
		fsm_msg.route = route;
		fsm_msg.prefix = prefix;

		/* send message to FSM */
		eigrp_fsm_event(&fsm_msg);
	}
}

/*
 * EIGRP UPDATE read function
 */
void eigrp_update_receive(struct eigrp *eigrp, eigrp_neighbor_t *nbr,
			  struct eigrp_header *eigrph, struct stream *pkt,
			  eigrp_interface_t *ei, int length)
{
	eigrp_prefix_descriptor_t *prefix;
	eigrp_route_descriptor_t *ne;
	eigrp_route_descriptor_t *route;
	uint32_t flags;
	uint8_t same;
	struct prefix dest_addr;
	uint8_t graceful_restart;
	uint8_t graceful_restart_final;
	struct list *nbr_prefixes = NULL;

	/* increment statistics. */
	ei->stats.rcvd.update++;

	flags = ntohl(eigrph->flags);
	if (flags & EIGRP_CR_FLAG) {
		return;
	}

	same = 0;
	graceful_restart = 0;
	graceful_restart_final = 0;
	if ((nbr->recv_sequence_number) == (ntohl(eigrph->sequence)))
		same = 1;

	nbr->recv_sequence_number = ntohl(eigrph->sequence);
	if (IS_DEBUG_EIGRP_PACKET(0, RECV))
		zlog_debug(
			"Processing Update len[%u] int(%s) nbr(%s) seq [%u] flags [%0x]",
			length,
			ifindex2ifname(nbr->ei->ifp->ifindex, VRF_DEFAULT),
			inet_ntoa(nbr->src), nbr->recv_sequence_number, flags);


	if ((flags == (EIGRP_INIT_FLAG + EIGRP_RS_FLAG + EIGRP_EOT_FLAG))
	    && (!same)) {
		/* Graceful restart Update received with all routes */
		zlog_info("Neighbor %s (%s) is resync: peer graceful-restart",
			  inet_ntoa(nbr->src),
			  ifindex2ifname(nbr->ei->ifp->ifindex, VRF_DEFAULT));

		/* get all prefixes from neighbor from topology table */
		nbr_prefixes = eigrp_neighbor_prefixes_lookup(eigrp, nbr);
		graceful_restart = 1;
		graceful_restart_final = 1;

	} else if ((flags == (EIGRP_INIT_FLAG + EIGRP_RS_FLAG)) && (!same)) {
		/* Graceful restart Update received, routes also in next packet
		 */
		zlog_info("Neighbor %s (%s) is resync: peer graceful-restart",
			  inet_ntoa(nbr->src),
			  ifindex2ifname(nbr->ei->ifp->ifindex, VRF_DEFAULT));

		/* get all prefixes from neighbor from topology table */
		nbr_prefixes = eigrp_neighbor_prefixes_lookup(eigrp, nbr);

		/* save prefixes to neighbor for later use */
		nbr->nbr_gr_prefixes = nbr_prefixes;
		graceful_restart = 1;
		graceful_restart_final = 0;

	} else if ((flags == (EIGRP_EOT_FLAG)) && (!same)) {
		/* If there was INIT+RS Update packet before,
		 *  consider this as GR EOT */
		if (nbr->nbr_gr_prefixes != NULL) {
			/* this is final packet of GR */
			nbr_prefixes = nbr->nbr_gr_prefixes;
			nbr->nbr_gr_prefixes = NULL;

			graceful_restart = 1;
			graceful_restart_final = 1;
		}

	} else if ((flags == (0)) && (!same)) {
		/* If there was INIT+RS Update packet before,
		 *  consider this as GR not final packet */
		if (nbr->nbr_gr_prefixes != NULL) {
			/* this is GR not final route packet */
			nbr_prefixes = nbr->nbr_gr_prefixes;

			graceful_restart = 1;
			graceful_restart_final = 0;
		}

	} else if ((flags & EIGRP_INIT_FLAG) && (!same)) {
		/*
		 * When in pending state, send INIT update only if it wasn't
		 * already sent before (only if init_sequence is 0)
		 */
		if ((nbr->state == EIGRP_NEIGHBOR_PENDING)
		    && (nbr->init_sequence_number == 0))
			eigrp_update_send_init(eigrp, nbr);

		if (nbr->state == EIGRP_NEIGHBOR_UP) {
			eigrp_nbr_state_set(nbr, EIGRP_NEIGHBOR_DOWN);
			eigrp_topology_neighbor_down(nbr->ei->eigrp, nbr);
			nbr->recv_sequence_number = ntohl(eigrph->sequence);
			zlog_info("Neighbor %s (%s) is down: peer restarted",
				  inet_ntoa(nbr->src),
				  ifindex2ifname(nbr->ei->ifp->ifindex,
						 VRF_DEFAULT));
			eigrp_nbr_state_set(nbr, EIGRP_NEIGHBOR_PENDING);
			zlog_info("Neighbor %s (%s) is pending: new adjacency",
				  inet_ntoa(nbr->src),
				  ifindex2ifname(nbr->ei->ifp->ifindex,
						 VRF_DEFAULT));
			eigrp_update_send_init(eigrp, nbr);
		}
	}

	/*If there is topology information*/
	while (pkt->endp > pkt->getp) {
		route = (nbr->tlv_decoder)(eigrp, nbr, pkt, length);

		// should hsve got route off the packet, but one never knows
		if (route) {

			eigrp_prefix_descriptor_t *dest =
				eigrp_topology_table_lookup_ipv4(
					eigrp->topology_table, &dest_addr);
			/*if exists it comes to DUAL*/
			if (dest != NULL) {
				/* remove received prefix from neighbor prefix
				 * list if in GR */
				if (graceful_restart)
					remove_received_prefix_gr(nbr_prefixes,
								  dest);

				struct eigrp_fsm_action_message msg;
				ne = eigrp_prefix_descriptor_lookup(
					dest->entries, nbr);

				msg.packet_type = EIGRP_OPC_UPDATE;
				msg.eigrp = eigrp;
				msg.data_type = EIGRP_INT;
				msg.adv_router = nbr;
				msg.metrics = route->metric;
				msg.route = ne;
				msg.prefix = dest;
				eigrp_fsm_event(&msg);

			} else {
				/*Here comes topology information save*/
				prefix = eigrp_prefix_descriptor_new();
				prefix->serno = eigrp->serno;
				prefix->destination =
					(struct prefix *)prefix_ipv4_new();
				prefix_copy(prefix->destination, &dest_addr);
				prefix->af = AF_INET;
				prefix->state = EIGRP_FSM_STATE_PASSIVE;
				prefix->nt = EIGRP_TOPOLOGY_TYPE_REMOTE;

				ne = eigrp_route_descriptor_new();
				ne->ei = ei;
				ne->adv_router = nbr;
				ne->reported_metric = prefix->reported_metric;
				ne->reported_distance = eigrp_calculate_metrics(
					eigrp, prefix->reported_metric);
				/*
				 * Filtering
				 */
				if (eigrp_update_prefix_apply(eigrp, ei,
							      EIGRP_FILTER_IN,
							      &dest_addr))
					ne->reported_metric.delay =
						EIGRP_MAX_METRIC;

				ne->distance = eigrp_calculate_total_metrics(
					eigrp, ne);
				prefix->fdistance = prefix->distance =
					prefix->rdistance = ne->distance;
				ne->prefix = prefix;
				ne->flags =
					EIGRP_ROUTE_DESCRIPTOR_SUCCESSOR_FLAG;

				eigrp_prefix_descriptor_add(
					eigrp->topology_table, prefix);
				eigrp_route_descriptor_add(eigrp, prefix, ne);
				prefix->distance = prefix->fdistance =
					prefix->rdistance = ne->distance;
				prefix->reported_metric = ne->total_metric;
				eigrp_topology_update_node_flags(eigrp, prefix);

				prefix->req_action |= EIGRP_FSM_NEED_UPDATE;
				listnode_add(eigrp->topology_changes, prefix);
			}
			break;
		}
	}

	/* ask about prefixes not present in GR update,
	 * if this is final GR packet */
	if (graceful_restart_final) {
		eigrp_update_receive_GR_ask(eigrp, nbr, nbr_prefixes);
	}

	/*
	 * We don't need to send separate Ack for INIT Update. INIT will be
	 * acked in EOT Update.
	 */
	if ((nbr->state == EIGRP_NEIGHBOR_UP) && !(flags == EIGRP_INIT_FLAG)) {
		eigrp_hello_send_ack(nbr);
	}

	eigrp_query_send_all(eigrp);
	eigrp_update_send_all(eigrp, ei);

	if (nbr_prefixes)
		list_delete(&nbr_prefixes);
}

/*send EIGRP Update packet*/
void eigrp_update_send_init(struct eigrp *eigrp, eigrp_neighbor_t *nbr)
{
	eigrp_packet_t *ep;
	uint16_t length = EIGRP_HEADER_LEN;

	ep = eigrp_packet_new(EIGRP_PACKET_MTU(nbr->ei->ifp->mtu), nbr);

	/* Prepare EIGRP INIT UPDATE header */
	if (IS_DEBUG_EIGRP_PACKET(0, RECV))
		zlog_debug("Enqueuing Update Init Seq [%u] Ack [%u]",
			   nbr->ei->eigrp->sequence_number,
			   nbr->recv_sequence_number);

	eigrp_packet_header_init(
		EIGRP_OPC_UPDATE, nbr->ei->eigrp, ep->s, EIGRP_INIT_FLAG,
		nbr->ei->eigrp->sequence_number, nbr->recv_sequence_number);

	// encode Authentication TLV, if needed
	if ((nbr->ei->params.auth_type == EIGRP_AUTH_TYPE_MD5)
	    && (nbr->ei->params.auth_keychain != NULL)) {
		length += eigrp_add_authTLV_MD5_encode(ep->s, nbr->ei);
		eigrp_make_md5_digest(nbr->ei, ep->s,
				      EIGRP_AUTH_UPDATE_INIT_FLAG);
	}

	/* EIGRP Checksum */
	eigrp_packet_checksum(nbr->ei, ep->s, length);

	ep->length = length;
	ep->dst.s_addr = nbr->src.s_addr;

	/*This ack number we await from neighbor*/
	nbr->init_sequence_number = nbr->ei->eigrp->sequence_number;
	ep->sequence_number = nbr->ei->eigrp->sequence_number;
	if (IS_DEBUG_EIGRP_PACKET(0, RECV))
		zlog_debug("Enqueuing Update Init Len [%u] Seq [%u] Dest [%s]",
			   ep->length, ep->sequence_number, inet_ntoa(ep->dst));

	/*Put packet to retransmission queue*/
	eigrp_packet_enqueue(nbr->retrans_queue, ep);

	if (nbr->retrans_queue->count == 1) {
		eigrp_packet_send_reliably(eigrp, nbr);
	}
}

static void eigrp_update_place_on_nbr_queue(struct eigrp *eigrp,
					    eigrp_neighbor_t *nbr,
					    eigrp_packet_t *ep, uint32_t seq_no,
					    int length)
{
	if ((nbr->ei->params.auth_type == EIGRP_AUTH_TYPE_MD5)
	    && (nbr->ei->params.auth_keychain != NULL)) {
		eigrp_make_md5_digest(nbr->ei, ep->s, EIGRP_AUTH_UPDATE_FLAG);
	}

	/* EIGRP Checksum */
	eigrp_packet_checksum(nbr->ei, ep->s, length);

	ep->length = length;
	ep->dst.s_addr = nbr->src.s_addr;

	/*This ack number we await from neighbor*/
	ep->sequence_number = seq_no;

	if (IS_DEBUG_EIGRP_PACKET(0, RECV))
		zlog_debug("Enqueuing Update Init Len [%u] Seq [%u] Dest [%s]",
			   ep->length, ep->sequence_number, inet_ntoa(ep->dst));

	/*Put packet to retransmission queue*/
	eigrp_packet_enqueue(nbr->retrans_queue, ep);

	if (nbr->retrans_queue->count == 1)
		eigrp_packet_send_reliably(eigrp, nbr);
}

static void eigrp_update_send_to_all_nbrs(struct eigrp *eigrp,
					  eigrp_interface_t *ei,
					  eigrp_packet_t *ep)
{
	struct listnode *node, *nnode;
	eigrp_neighbor_t *nbr;
	bool packet_sent = false;

	for (ALL_LIST_ELEMENTS(ei->nbrs, node, nnode, nbr)) {
		eigrp_packet_t *ep_dup;

		if (nbr->state != EIGRP_NEIGHBOR_UP)
			continue;

		if (packet_sent)
			ep_dup = eigrp_packet_duplicate(ep, NULL);
		else
			ep_dup = ep;

		ep_dup->nbr = nbr;
		packet_sent = true;

		/*Put packet to retransmission queue*/
		eigrp_packet_enqueue(nbr->retrans_queue, ep_dup);

		if (nbr->retrans_queue->count == 1) {
			eigrp_packet_send_reliably(eigrp, nbr);
		}
	}

	if (!packet_sent)
		eigrp_packet_free(ep);
}

void eigrp_update_send_EOT(eigrp_neighbor_t *nbr)
{
	eigrp_packet_t *ep;
	uint16_t length = EIGRP_HEADER_LEN;
	eigrp_route_descriptor_t *te;
	eigrp_prefix_descriptor_t *prefix;
	struct listnode *node2, *nnode2;
	eigrp_interface_t *ei = nbr->ei;
	struct eigrp *eigrp = ei->eigrp;
	struct prefix *dest_addr;
	uint32_t seq_no = eigrp->sequence_number;
	uint16_t eigrp_mtu = EIGRP_PACKET_MTU(ei->ifp->mtu);
	struct route_node *rn;

	ep = eigrp_packet_new(eigrp_mtu, nbr);

	/* Prepare EIGRP EOT UPDATE header */
	eigrp_packet_header_init(EIGRP_OPC_UPDATE, eigrp, ep->s, EIGRP_EOT_FLAG,
				 seq_no, nbr->recv_sequence_number);

	// encode Authentication TLV, if needed
	if ((ei->params.auth_type == EIGRP_AUTH_TYPE_MD5)
	    && (ei->params.auth_keychain != NULL)) {
		length += eigrp_add_authTLV_MD5_encode(ep->s, ei);
	}

	for (rn = route_top(eigrp->topology_table); rn; rn = route_next(rn)) {
		if (!rn->info)
			continue;

		prefix = rn->info;
		for (ALL_LIST_ELEMENTS(prefix->entries, node2, nnode2, te)) {
			if (eigrp_nbr_split_horizon_check(te, ei))
				continue;

			if ((length + EIGRP_TLV_MAX_IPV4_BYTE) > eigrp_mtu) {
				eigrp_update_place_on_nbr_queue(eigrp, nbr, ep,
								seq_no, length);
				seq_no++;

				length = EIGRP_HEADER_LEN;
				ep = eigrp_packet_new(eigrp_mtu, nbr);
				eigrp_packet_header_init(
					EIGRP_OPC_UPDATE, nbr->ei->eigrp, ep->s,
					EIGRP_EOT_FLAG, seq_no,
					nbr->recv_sequence_number);

				if ((ei->params.auth_type
				     == EIGRP_AUTH_TYPE_MD5)
				    && (ei->params.auth_keychain != NULL)) {
					length += eigrp_add_authTLV_MD5_encode(
						ep->s, ei);
				}
			}
			/* Get destination address from prefix */
			dest_addr = prefix->destination;

			/* Check if any list fits */
			if (eigrp_update_prefix_apply(
				    eigrp, ei, EIGRP_FILTER_OUT, dest_addr))
				continue;
			else {
				length += (nbr->tlv_encoder)(eigrp, nbr, ep->s,
							     prefix);
			}
		}
	}

	eigrp_update_place_on_nbr_queue(eigrp, nbr, ep, seq_no, length);
	eigrp->sequence_number = seq_no++;
}

void eigrp_update_send(struct eigrp *eigrp, eigrp_neighbor_t *nbr,
		       eigrp_interface_t *ei)
{
	eigrp_packet_t *ep;

	eigrp_prefix_descriptor_t *prefix;
	eigrp_route_descriptor_t *route;
	uint8_t has_tlv;
	uint32_t seq_no = eigrp->sequence_number;
	uint16_t eigrp_mtu = EIGRP_PACKET_MTU(ei->ifp->mtu);
	uint16_t length = EIGRP_HEADER_LEN;

	struct listnode *node, *nnode;
	struct prefix *dest_addr;

	/* if we dont have peers on this interface, then we're done. */
	if (ei->nbrs->count == 0)
		return;

	ep = eigrp_packet_new(eigrp_mtu, NULL);

	/* Prepare EIGRP INIT UPDATE header */
	eigrp_packet_header_init(EIGRP_OPC_UPDATE, eigrp, ep->s, 0, seq_no, 0);

	// encode Authentication TLV, if needed
	if ((ei->params.auth_type == EIGRP_AUTH_TYPE_MD5)
	    && (ei->params.auth_keychain != NULL)) {
		length += eigrp_add_authTLV_MD5_encode(ep->s, ei);
	}

	has_tlv = 0;
	for (ALL_LIST_ELEMENTS(ei->eigrp->topology_changes, node, nnode,
			       prefix)) {

		if (!(prefix->req_action & EIGRP_FSM_NEED_UPDATE))
			continue;

		route = listnode_head(prefix->entries);
		if (eigrp_nbr_split_horizon_check(route, ei))
			continue;

		if ((length + EIGRP_TLV_MAX_IPV4_BYTE) > eigrp_mtu) {
			if ((ei->params.auth_type == EIGRP_AUTH_TYPE_MD5)
			    && (ei->params.auth_keychain != NULL)) {
				eigrp_make_md5_digest(ei, ep->s,
						      EIGRP_AUTH_UPDATE_FLAG);
			}

			eigrp_packet_checksum(ei, ep->s, length);
			ep->length = length;

			ep->dst.s_addr = htonl(EIGRP_MULTICAST_ADDRESS);

			ep->sequence_number = seq_no;
			seq_no++;
			eigrp_update_send_to_all_nbrs(eigrp, ei, ep);

			length = EIGRP_HEADER_LEN;
			ep = eigrp_packet_new(eigrp_mtu, NULL);
			eigrp_packet_header_init(EIGRP_OPC_UPDATE, eigrp, ep->s,
						 0, seq_no, 0);
			if ((ei->params.auth_type == EIGRP_AUTH_TYPE_MD5)
			    && (ei->params.auth_keychain != NULL)) {
				length +=
					eigrp_add_authTLV_MD5_encode(ep->s, ei);
			}
			has_tlv = 0;
		}
		/* Get destination address from prefix */
		dest_addr = prefix->destination;

		if (eigrp_update_prefix_apply(eigrp, ei, EIGRP_FILTER_OUT,
					      dest_addr)) {
			// prefix->reported_metric.delay = EIGRP_MAX_METRIC;
			continue;
		} else {
			/* DVS:
			 * OK. baby steps.  Fist lets get it working as if we
			 * are TLV1. Once we step up to TLV2, we will ahve
			 * account fot eh fact that some of Cisco router are
			 * legacy 32bit.
			 *
			 * To do this we meed to check the interface and either
			 * encode only 64bit, or both 32 and 64 bit versions of
			 * the tlv.
			 */
			length += (nbr->tlv_encoder)(eigrp, nbr, ep->s, prefix);
			has_tlv = 1;
		}
	}

	if (!has_tlv) {
		eigrp_packet_free(ep);
		return;
	}

	if ((ei->params.auth_type == EIGRP_AUTH_TYPE_MD5)
	    && (ei->params.auth_keychain != NULL)) {
		eigrp_make_md5_digest(ei, ep->s, EIGRP_AUTH_UPDATE_FLAG);
	}

	/* EIGRP Checksum */
	eigrp_packet_checksum(ei, ep->s, length);
	ep->length = length;

	ep->dst.s_addr = htonl(EIGRP_MULTICAST_ADDRESS);

	/*This ack number we await from neighbor*/
	ep->sequence_number = eigrp->sequence_number;

	if (IS_DEBUG_EIGRP_PACKET(0, RECV))
		zlog_debug("Enqueuing Update length[%u] Seq [%u]", length,
			   ep->sequence_number);

	eigrp_update_send_to_all_nbrs(eigrp, ei, ep);
	ei->eigrp->sequence_number = seq_no++;
}

void eigrp_update_send_all(struct eigrp *eigrp, eigrp_interface_t *exception)
{
	eigrp_interface_t *iface;
	struct listnode *node, *node2, *nnode2;
	eigrp_prefix_descriptor_t *prefix;

	for (ALL_LIST_ELEMENTS_RO(eigrp->eiflist, node, iface)) {
		if (iface != exception) {
			eigrp_update_send(eigrp, NULL, iface);
		}
	}

	for (ALL_LIST_ELEMENTS(eigrp->topology_changes, node2, nnode2,
			       prefix)) {
		if (prefix->req_action & EIGRP_FSM_NEED_UPDATE) {
			prefix->req_action &= ~EIGRP_FSM_NEED_UPDATE;
			listnode_delete(eigrp->topology_changes, prefix);
		}
	}
}

/**
 * @fn eigrp_update_send_GR_part
 *
 * @param[in]		nbr		contains neighbor who would receive
 * Graceful
 * restart
 *
 * @return void
 *
 * @par
 * Function used for sending Graceful restart Update packet
 * and if there are multiple chunks, send only one of them.
 * It is called from thread. Do not call it directly.
 *
 * Uses nbr_gr_packet_type from neighbor.
 */
static void eigrp_update_send_GR_part(eigrp_neighbor_t *nbr)
{
	eigrp_interface_t *ei = nbr->ei;
	struct eigrp *eigrp = ei->eigrp;
	eigrp_packet_t *ep;
	eigrp_prefix_descriptor_t *prefix;

	uint16_t length = EIGRP_HEADER_LEN;
	struct prefix *dest_addr;
	struct list *prefixes;
	uint32_t flags;
	unsigned int send_prefixes;
	struct route_node *rn;

	/* get prefixes to send to neighbor */
	prefixes = nbr->nbr_gr_prefixes_send;

	send_prefixes = 0;

	/* if there already were last packet chunk, we won't continue */
	if (nbr->nbr_gr_packet_type == EIGRP_PACKET_PART_LAST)
		return;

	/* if this is first packet chunk, we need to decide,
	 * if there will be one or more chunks */
	if (nbr->nbr_gr_packet_type == EIGRP_PACKET_PART_FIRST) {
		if (prefixes->count <= EIGRP_TLV_MAX_IPv4) {
			/* there will be only one chunk */
			flags = EIGRP_INIT_FLAG + EIGRP_RS_FLAG
				+ EIGRP_EOT_FLAG;
			nbr->nbr_gr_packet_type = EIGRP_PACKET_PART_LAST;
		} else {
			/* there will be more chunks */
			flags = EIGRP_INIT_FLAG + EIGRP_RS_FLAG;
			nbr->nbr_gr_packet_type = EIGRP_PACKET_PART_NA;
		}
	} else {
		/* this is not first chunk, and we need to decide,
		 * if there will be more chunks */
		if (prefixes->count <= EIGRP_TLV_MAX_IPv4) {
			/* this is last chunk */
			flags = EIGRP_EOT_FLAG;
			nbr->nbr_gr_packet_type = EIGRP_PACKET_PART_LAST;
		} else {
			/* there will be more chunks */
			flags = 0;
			nbr->nbr_gr_packet_type = EIGRP_PACKET_PART_NA;
		}
	}

	ep = eigrp_packet_new(EIGRP_PACKET_MTU(ei->ifp->mtu), nbr);

	/* Prepare EIGRP Graceful restart UPDATE header */
	eigrp_packet_header_init(EIGRP_OPC_UPDATE, eigrp, ep->s, flags,
				 eigrp->sequence_number,
				 nbr->recv_sequence_number);

	// encode Authentication TLV, if needed
	if ((ei->params.auth_type == EIGRP_AUTH_TYPE_MD5)
	    && (ei->params.auth_keychain != NULL)) {
		length += eigrp_add_authTLV_MD5_encode(ep->s, ei);
	}

	for (rn = route_top(eigrp->topology_table); rn; rn = route_next(rn)) {
		if (!rn->info)
			continue;

		prefix = rn->info;
		/*
		 * Filtering
		 */
		dest_addr = prefix->destination;

		if (eigrp_update_prefix_apply(eigrp, ei, EIGRP_FILTER_OUT,
					      dest_addr)) {
			/* do not send filtered route */
			zlog_info("Filtered prefix %s won't be sent out.",
				  inet_ntoa(dest_addr->u.prefix4));
		} else {
			/* sending route which wasn't filtered */
			length += (nbr->tlv_encoder)(eigrp, nbr, ep->s, prefix);
			send_prefixes++;
		}

		/*
		 * This makes no sense, Filter out then filter in???
		 * Look into this more - DBS
		 */
		if (eigrp_update_prefix_apply(eigrp, ei, EIGRP_FILTER_IN,
					      dest_addr)) {
			/* do not send filtered route */
			zlog_info("Filtered prefix %s will be removed.",
				  inet_ntoa(dest_addr->u.prefix4));

			/* prepare message for FSM */
			eigrp_fsm_action_message_t fsm_msg;

			eigrp_route_descriptor_t *route =
				eigrp_prefix_descriptor_lookup(prefix->entries,
							       nbr);

			fsm_msg.packet_type = EIGRP_OPC_UPDATE;
			fsm_msg.eigrp = eigrp;
			fsm_msg.data_type = EIGRP_INT;
			fsm_msg.adv_router = nbr;
			fsm_msg.metrics = prefix->reported_metric;
			/* Set delay to MAX */
			fsm_msg.metrics.delay = EIGRP_MAX_METRIC;
			fsm_msg.route = route;
			fsm_msg.prefix = prefix;

			/* send message to FSM */
			eigrp_fsm_event(&fsm_msg);
		}

		/* NULL the pointer */
		dest_addr = NULL;

		/* delete processed prefix from list */
		listnode_delete(prefixes, prefix);

		/* if there are enough prefixes, send packet */
		if (send_prefixes >= EIGRP_TLV_MAX_IPv4)
			break;
	}

	/* compute Auth digest */
	if ((ei->params.auth_type == EIGRP_AUTH_TYPE_MD5)
	    && (ei->params.auth_keychain != NULL)) {
		eigrp_make_md5_digest(ei, ep->s, EIGRP_AUTH_UPDATE_FLAG);
	}

	/* EIGRP Checksum */
	eigrp_packet_checksum(ei, ep->s, length);

	ep->length = length;
	ep->dst.s_addr = nbr->src.s_addr;

	/*This ack number we await from neighbor*/
	ep->sequence_number = eigrp->sequence_number;

	if (IS_DEBUG_EIGRP_PACKET(0, RECV))
		zlog_debug("Enqueuing Update Init Len [%u] Seq [%u] Dest [%s]",
			   ep->length, ep->sequence_number, inet_ntoa(ep->dst));

	/*Put packet to retransmission queue*/
	eigrp_packet_enqueue(nbr->retrans_queue, ep);

	if (nbr->retrans_queue->count == 1) {
		eigrp_packet_send_reliably(eigrp, nbr);
	}
}

/**
 * @fn eigrp_update_send_GR_thread
 *
 * @param[in]		thread		contains neighbor who would receive
 * Graceful restart
 *
 * @return int      always 0
 *
 * @par
 * Function used for sending Graceful restart Update packet
 * in thread, it is prepared for multiple chunks of packet.
 *
 * Uses nbr_gr_packet_type and t_nbr_send_gr from neighbor.
 */
int eigrp_update_send_GR_thread(struct thread *thread)
{
	eigrp_neighbor_t *nbr;

	/* get argument from thread */
	nbr = THREAD_ARG(thread);
	/* remove this thread pointer */
	nbr->t_nbr_send_gr = NULL;

	/* if there is packet waiting in queue,
	 * schedule this thread again with small delay */
	if (nbr->retrans_queue->count > 0) {
		nbr->t_nbr_send_gr = NULL;
		thread_add_timer_msec(master, eigrp_update_send_GR_thread, nbr,
				      10, &nbr->t_nbr_send_gr);
		return 0;
	}

	/* send GR EIGRP packet chunk */
	eigrp_update_send_GR_part(nbr);

	/* if it wasn't last chunk, schedule this thread again */
	if (nbr->nbr_gr_packet_type != EIGRP_PACKET_PART_LAST) {
		thread_execute(master, eigrp_update_send_GR_thread, nbr, 0);
		nbr->t_nbr_send_gr = NULL;
	}

	return 0;
}

/**
 * @fn eigrp_update_send_GR
 *
 * @param[in]		nbr			Neighbor who would receive
 * Graceful
 * restart
 * @param[in]		gr_type 	Who executed Graceful restart
 * @param[in]		vty 		Virtual terminal for log output
 *
 * @return void
 *
 * @par
 * Function used for sending Graceful restart Update packet:
 * Creates Update packet with INIT, RS, EOT flags and include
 * all route except those filtered
 */
void eigrp_update_send_GR(eigrp_neighbor_t *nbr, enum GR_type gr_type,
			  struct vty *vty)
{
	eigrp_prefix_descriptor_t *prefix2;
	struct list *prefixes;
	struct route_node *rn;
	eigrp_interface_t *ei = nbr->ei;
	struct eigrp *eigrp = ei->eigrp;

	if (gr_type == EIGRP_GR_FILTER) {
		/* function was called after applying filtration */
		zlog_info(
			"Neighbor %s (%s) is resync: route configuration changed",
			inet_ntoa(nbr->src),
			ifindex2ifname(ei->ifp->ifindex, eigrp->vrf_id));
	} else if (gr_type == EIGRP_GR_MANUAL) {
		/* Graceful restart was called manually */
		zlog_info("Neighbor %s (%s) is resync: manually cleared",
			  inet_ntoa(nbr->src),
			  ifindex2ifname(ei->ifp->ifindex, eigrp->vrf_id));

		if (vty != NULL) {
			vty_time_print(vty, 0);
			vty_out(vty,
				"Neighbor %s (%s) is resync: manually cleared\n",
				inet_ntoa(nbr->src),
				ifindex2ifname(ei->ifp->ifindex,
					       eigrp->vrf_id));
		}
	}

	prefixes = list_new();
	/* add all prefixes from topology table to list */
	for (rn = route_top(eigrp->topology_table); rn; rn = route_next(rn)) {
		if (!rn->info)
			continue;

		prefix2 = rn->info;
		listnode_add(prefixes, prefix2);
	}

	/* save prefixes to neighbor */
	nbr->nbr_gr_prefixes_send = prefixes;
	/* indicate, that this is first GR Update packet chunk */
	nbr->nbr_gr_packet_type = EIGRP_PACKET_PART_FIRST;
	/* execute packet sending in thread */
	thread_execute(master, eigrp_update_send_GR_thread, nbr, 0);
	nbr->t_nbr_send_gr = NULL;
}

/**
 * @fn eigrp_update_send_interface_GR
 *
 * @param[in]		ei			Interface to neighbors of which
 * the
 * GR
 * is sent
 * @param[in]		gr_type 	Who executed Graceful restart
 * @param[in]		vty 		Virtual terminal for log output
 *
 * @return void
 *
 * @par
 * Function used for sending Graceful restart Update packet
 * to all neighbors on specified interface.
 */
void eigrp_update_send_interface_GR(eigrp_interface_t *ei, enum GR_type gr_type,
				    struct vty *vty)
{
	struct listnode *node;
	eigrp_neighbor_t *nbr;

	/* iterate over all neighbors on eigrp interface */
	for (ALL_LIST_ELEMENTS_RO(ei->nbrs, node, nbr)) {
		/* send GR to neighbor */
		eigrp_update_send_GR(nbr, gr_type, vty);
	}
}

/**
 * @fn eigrp_update_send_process_GR
 *
 * @param[in]		eigrp		EIGRP process
 * @param[in]		gr_type 	Who executed Graceful restart
 * @param[in]		vty 		Virtual terminal for log output
 *
 * @return void
 *
 * @par
 * Function used for sending Graceful restart Update packet
 * to all neighbors in eigrp process.
 */
void eigrp_update_send_process_GR(struct eigrp *eigrp, enum GR_type gr_type,
				  struct vty *vty)
{
	struct listnode *node;
	eigrp_interface_t *ei;

	/* iterate over all eigrp interfaces */
	for (ALL_LIST_ELEMENTS_RO(eigrp->eiflist, node, ei)) {
		/* send GR to all neighbors on interface */
		eigrp_update_send_interface_GR(ei, gr_type, vty);
	}
}
