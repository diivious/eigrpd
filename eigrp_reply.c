/*
 * Eigrp Sending and Receiving EIGRP Reply Packets.
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

#include "eigrpd/eigrpd.h"
#include "eigrpd/eigrp_structs.h"
#include "eigrpd/eigrp_neighbor.h"
#include "eigrpd/eigrp_packet.h"
#include "eigrpd/eigrp_topology.h"
#include "eigrpd/eigrp_fsm.h"

void eigrp_reply_send(struct eigrp *eigrp, eigrp_neighbor_t *nbr,
		      eigrp_prefix_descriptor_t *prefix)
{
	eigrp_interface_t *ei = nbr->ei;
	eigrp_packet_t *packet = NULL;
	eigrp_route_descriptor_t *route;
	uint16_t length = EIGRP_HEADER_LEN;
	struct list *successors;

	/* Prepare EIGRP INIT UPDATE header */
	packet = eigrp_packet_new(EIGRP_PACKET_MTU(ei->ifp->mtu), nbr);
	eigrp_packet_header_init(EIGRP_OPC_REPLY, eigrp, packet->s, 0,
				 eigrp->sequence_number, 0);

	// encode Authentication TLV, if needed
	if (ei->params.auth_type == EIGRP_AUTH_TYPE_MD5
	    && (ei->params.auth_keychain != NULL)) {
		length += eigrp_add_authTLV_MD5_encode(packet->s, ei);
	}

	// grab the route from the prefix so we can get the metrics we need
	successors = eigrp_topology_get_successor(prefix);
	assert(successors); // If this is NULL somebody poked us in the eye.
	route = listnode_head(successors);
	length += (nbr->tlv_encoder)(eigrp, nbr, packet->s, route);

	if ((ei->params.auth_type == EIGRP_AUTH_TYPE_MD5) &&
	    (ei->params.auth_keychain != NULL)) {
		eigrp_make_md5_digest(ei, packet->s, EIGRP_AUTH_UPDATE_FLAG);
	}

	/* EIGRP Checksum */
	eigrp_packet_checksum(ei, packet->s, length);

	packet->length = length;
	eigrp_addr_copy(&packet->dst, &nbr->src);

	/*This ack number we await from neighbor*/
	packet->sequence_number = eigrp->sequence_number;

	/*Put packet to retransmission queue*/
	eigrp_packet_enqueue(nbr->retrans_queue, packet);

	if (nbr->retrans_queue->count == 1) {
		eigrp_packet_send_reliably(eigrp, nbr);
	}
}

/*EIGRP REPLY read function*/
void eigrp_reply_receive(struct eigrp *eigrp, eigrp_neighbor_t *nbr,
			 struct eigrp_header *eigrph, struct stream *pkt,
			 eigrp_interface_t *ei, int length)
{
	struct eigrp_fsm_action_message msg;
	eigrp_prefix_descriptor_t *prefix;
	eigrp_route_descriptor_t *route;

	/* increment statistics. */
	ei->stats.rcvd.reply++;

	// record neighbor seq were processing
	nbr->recv_sequence_number = ntohl(eigrph->sequence);

	while (pkt->endp > pkt->getp) {
		route = (nbr->tlv_decoder)(eigrp, nbr, pkt, length);
		prefix = route->prefix;

		// should hsve got route off the packet, but one never knowd
		if (route) {
			msg.packet_type = EIGRP_OPC_REPLY;
			msg.eigrp = eigrp;
			msg.data_type = EIGRP_INT;
			msg.adv_router = nbr;
			msg.route = route;
			msg.metrics = route->metric;
			msg.prefix = prefix;
			eigrp_fsm_event(&msg);

		} else {
			// Destination must exists
			char buf[PREFIX_STRLEN];
			zlog_err(
				"%s: Received prefix %s which we do not know about",
				__PRETTY_FUNCTION__,
				prefix2str(prefix->destination, buf,
					   sizeof(buf)));
			continue;
		}
	}
	eigrp_hello_send_ack(nbr);
}
