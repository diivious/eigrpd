/*
 * EIGRP Sending and Receiving EIGRP SIA-Query Packets.
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

/* EIGRP SIA-QUERY read function */
void eigrp_siaquery_receive(struct eigrp *eigrp, eigrp_neighbor_t *nbr,
			    struct eigrp_header *eigrph, struct stream *pkt,
			    eigrp_interface_t *ei, int length)
{
	eigrp_fsm_action_message_t msg;
	eigrp_route_descriptor_t *route;

	/* increment statistics. */
	ei->stats.rcvd.siaQuery++;

	/* get neighbor struct */
	nbr->recv_sequence_number = ntohl(eigrph->sequence);

	// process all TLVs in the packet
	while (pkt->endp > pkt->getp) {
		route = (nbr->tlv_decoder)(eigrp, nbr, pkt, length);

		// should have got route off the packet, but one never know
		if (route) {
			msg.packet_type = EIGRP_OPC_SIAQUERY;
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
}

void eigrp_siaquery_send(struct eigrp *eigrp, eigrp_neighbor_t *nbr,
			 eigrp_prefix_descriptor_t *prefix)
{
	eigrp_packet_t *packet;
	uint16_t length = EIGRP_HEADER_LEN;

	packet = eigrp_packet_new(EIGRP_PACKET_MTU(nbr->ei->ifp->mtu), nbr);

	/* Prepare EIGRP INIT UPDATE header */
	eigrp_packet_header_init(EIGRP_OPC_SIAQUERY, nbr->ei->eigrp, packet->s, 0,
				 nbr->ei->eigrp->sequence_number, 0);

	// encode Authentication TLV, if needed
	if ((nbr->ei->params.auth_type == EIGRP_AUTH_TYPE_MD5)
	    && (nbr->ei->params.auth_keychain != NULL)) {
		length += eigrp_add_authTLV_MD5_encode(packet->s, nbr->ei);
	}

	length += (nbr->tlv_encoder)(eigrp, nbr, packet->s, prefix);

	if ((nbr->ei->params.auth_type == EIGRP_AUTH_TYPE_MD5)
	    && (nbr->ei->params.auth_keychain != NULL)) {
		eigrp_make_md5_digest(nbr->ei, packet->s, EIGRP_AUTH_UPDATE_FLAG);
	}

	/* EIGRP Checksum */
	eigrp_packet_checksum(nbr->ei, packet->s, length);

	packet->length = length;
	eigrp_addr_copy(&packet->dst, &nbr->src);

	/*This ack number we await from neighbor*/
	packet->sequence_number = nbr->ei->eigrp->sequence_number;

	if (nbr->state == EIGRP_NEIGHBOR_UP) {
		/*Put packet to retransmission queue*/
		eigrp_packet_enqueue(nbr->retrans_queue, packet);

		if (nbr->retrans_queue->count == 1) {
			eigrp_packet_send_reliably(eigrp, nbr);
		}
	} else
		eigrp_packet_free(packet);
}
