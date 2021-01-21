/*
 * EIGRP Sending and Receiving EIGRP Query Packets.
 * Copyright (C) 2013-2014
 * Authors:
 *   Donnie Savage
 *   Jan Janovic
 *   Matej Perina
 *   Peter Orsag
 *   Peter Paluch
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
#include "eigrpd/eigrp_memory.h"

uint32_t eigrp_query_send_all(struct eigrp *eigrp)
{
	eigrp_interface_t *iface;
	struct listnode *node, *node2, *nnode2;
	eigrp_prefix_descriptor_t *prefix;
	uint32_t counter;

	counter = 0;
	for (ALL_LIST_ELEMENTS_RO(eigrp->eiflist, node, iface)) {
		eigrp_query_send(eigrp, iface);
		counter++;
	}

	for (ALL_LIST_ELEMENTS(eigrp->topology_changes, node2, nnode2,
			       prefix)) {
		if (prefix->req_action & EIGRP_FSM_NEED_QUERY) {
			prefix->req_action &= ~EIGRP_FSM_NEED_QUERY;
			listnode_delete(eigrp->topology_changes, prefix);
		}
	}

	return counter;
}

/*EIGRP QUERY read function*/
void eigrp_query_receive(struct eigrp *eigrp, eigrp_neighbor_t *nbr,
			 struct eigrp_header *eigrph, struct stream *pkt,
			 eigrp_interface_t *ei, int length)
{
	eigrp_fsm_action_message_t msg;
	eigrp_route_descriptor_t *route;

	/* increment statistics. */
	ei->stats.rcvd.query++;

	/* get neighbor struct */
	nbr->recv_sequence_number = ntohl(eigrph->sequence);

	// process all TLVs in the packet
	while (pkt->endp > pkt->getp) {
		route = (nbr->tlv_decoder)(eigrp, nbr, pkt, length);

		// should have got route off the packet, but one never know
		if (route) {
			msg.packet_type = EIGRP_OPC_QUERY;
			msg.eigrp = eigrp;
			msg.data_type = EIGRP_INT;
			msg.adv_router = nbr;
			msg.route = route;
			msg.metrics = route->metric;
			msg.prefix = route->prefix;
			eigrp_fsm_event(&msg);
		} else {
			// neighbor sent corrupted packet - flush remaining
			// packet
			break;
		}
	}

	eigrp_hello_send_ack(nbr);
	eigrp_query_send_all(eigrp);
	eigrp_update_send_all(eigrp, nbr->ei);
}

void eigrp_query_send(struct eigrp *eigrp, eigrp_interface_t *ei)
{
	eigrp_packet_t *ep = NULL;
	eigrp_neighbor_t *nbr;

	uint16_t length = EIGRP_HEADER_LEN;
	struct listnode *node, *nnode, *node2, *nnode2;
	eigrp_prefix_descriptor_t *prefix;
	bool has_tlv = false;
	bool new_packet = true;
	uint16_t eigrp_mtu = EIGRP_PACKET_MTU(ei->ifp->mtu);

	for (ALL_LIST_ELEMENTS(ei->eigrp->topology_changes, node, nnode,
			       prefix)) {
		if (!(prefix->req_action & EIGRP_FSM_NEED_QUERY))
			continue;

		if (new_packet) {
			ep = eigrp_packet_new(eigrp_mtu, NULL);

			/* Prepare EIGRP INIT UPDATE header */
			eigrp_packet_header_init(EIGRP_OPC_QUERY, ei->eigrp,
						 ep->s, 0,
						 ei->eigrp->sequence_number, 0);

			// encode Authentication TLV, if needed
			if ((ei->params.auth_type == EIGRP_AUTH_TYPE_MD5)
			    && (ei->params.auth_keychain != NULL)) {
				length +=
					eigrp_add_authTLV_MD5_encode(ep->s, ei);
			}
			new_packet = false;
		}

		length += (nbr->tlv_encoder)(eigrp, nbr, ep->s, prefix);
		has_tlv = true;
		for (ALL_LIST_ELEMENTS(ei->nbrs, node2, nnode2, nbr)) {
			if (nbr->state == EIGRP_NEIGHBOR_UP)
				listnode_add(prefix->rij, nbr);
		}

		if (length + EIGRP_TLV_MAX_IPV4_BYTE > eigrp_mtu) {
			if ((ei->params.auth_type == EIGRP_AUTH_TYPE_MD5)
			    && ei->params.auth_keychain != NULL) {
				eigrp_make_md5_digest(ei, ep->s,
						      EIGRP_AUTH_UPDATE_FLAG);
			}

			eigrp_packet_checksum(ei, ep->s, length);
			ep->length = length;

			ep->dst.s_addr = htonl(EIGRP_MULTICAST_ADDRESS);

			ep->sequence_number = ei->eigrp->sequence_number;
			ei->eigrp->sequence_number++;

			for (ALL_LIST_ELEMENTS(ei->nbrs, node2, nnode2, nbr)) {
				eigrp_packet_t *dup;

				if (nbr->state != EIGRP_NEIGHBOR_UP)
					continue;

				dup = eigrp_packet_duplicate(ep, nbr);
				/*Put packet to retransmission queue*/
				eigrp_packet_enqueue(nbr->retrans_queue, dup);

				if (nbr->retrans_queue->count == 1)
					eigrp_packet_send_reliably(eigrp, nbr);
			}

			has_tlv = false;
			length = 0;
			eigrp_packet_free(ep);
			ep = NULL;
			new_packet = true;
		}
	}

	if (!has_tlv)
		return;

	if ((ei->params.auth_type == EIGRP_AUTH_TYPE_MD5)
	    && ei->params.auth_keychain != NULL)
		eigrp_make_md5_digest(ei, ep->s, EIGRP_AUTH_UPDATE_FLAG);


	/* EIGRP Checksum */
	eigrp_packet_checksum(ei, ep->s, length);

	ep->length = length;
	ep->dst.s_addr = htonl(EIGRP_MULTICAST_ADDRESS);

	/*This ack number we await from neighbor*/
	ep->sequence_number = ei->eigrp->sequence_number;
	ei->eigrp->sequence_number++;

	for (ALL_LIST_ELEMENTS(ei->nbrs, node2, nnode2, nbr)) {
		eigrp_packet_t *dup;

		if (nbr->state != EIGRP_NEIGHBOR_UP)
			continue;

		dup = eigrp_packet_duplicate(ep, nbr);
		/*Put packet to retransmission queue*/
		eigrp_packet_enqueue(nbr->retrans_queue, dup);

		if (nbr->retrans_queue->count == 1)
			eigrp_packet_send_reliably(eigrp, nbr);
	}

	eigrp_packet_free(ep);
}
