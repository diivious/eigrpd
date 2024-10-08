// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * EIGRP Sending and Receiving EIGRP SIA-Reply Packets.
 * Copyright (C) 2013-2014
 * Authors:
 *   Donnie Savage
 *   Jan Janovic
 *   Matej Perina
 *   Peter Orsag
 *   Peter Paluch
 */
#include "eigrpd/eigrpd.h"
#include "eigrpd/eigrp_structs.h"
#include "eigrpd/eigrp_neighbor.h"
#include "eigrpd/eigrp_packet.h"
#include "eigrpd/eigrp_auth.h"
#include "eigrpd/eigrp_network.h"
#include "eigrpd/eigrp_topology.h"
#include "eigrpd/eigrp_fsm.h"

/* EIGRP SIA-REPLY read function */
void eigrp_siareply_receive(eigrp_instance_t *eigrp, eigrp_neighbor_t *nbr,
			    struct eigrp_header *eigrph, struct stream *pkt,
			    eigrp_interface_t *ei, int length)
{
	struct eigrp_fsm_action_message msg;
	eigrp_prefix_descriptor_t *prefix;
	eigrp_route_descriptor_t *route;

	/* increment statistics. */
	ei->stats.rcvd.siaReply++;
	nbr->recv_sequence_number = ntohl(eigrph->sequence);

	while (pkt->endp > pkt->getp) {
		route = (nbr->tlv_decoder)(eigrp, nbr, pkt, length);
		prefix = route->prefix;

		/* If the destination exists (it should, but one never know)*/
		if (route != NULL) {
			msg.packet_type = EIGRP_OPC_SIAREPLY;
			msg.eigrp = eigrp;
			msg.data_type = EIGRP_INT;
			msg.adv_router = nbr;
			msg.route = route;
			msg.metrics = route->metric;
			msg.prefix = prefix;
			eigrp_fsm_event(&msg);

		} else {
			// Destination must exists
			zlog_err("%s: Received prefix %s which we do not know about",
				 __PRETTY_FUNCTION__,
				 eigrp_print_prefix(prefix->destination));
			continue;
		}
	}
	eigrp_hello_send_ack(nbr);
}

void eigrp_siareply_send(eigrp_instance_t *eigrp,
			 eigrp_neighbor_t *nbr,
			 eigrp_prefix_descriptor_t *prefix)
{
	eigrp_packet_t *packet;
	eigrp_route_descriptor_t *route;
	uint16_t length = EIGRP_HEADER_LEN;
	struct list *successors;

	packet = eigrp_packet_new(EIGRP_PACKET_MTU(nbr->ei->ifp->mtu), nbr);

	/* Prepare EIGRP INIT UPDATE header */
	eigrp_packet_header_init(EIGRP_OPC_SIAREPLY, nbr->ei->eigrp, packet->s, 0,
				 nbr->ei->eigrp->sequence_number, 0);

	// encode Authentication TLV, if needed
	if (nbr->ei->params.auth_type == EIGRP_AUTH_TYPE_MD5
	    && nbr->ei->params.auth_keychain != NULL) {
		length += eigrp_add_authTLV_MD5_encode(packet->s, nbr->ei);
	}

	// grab the route from the prefix so we can get the metrics we need
	successors = eigrp_topology_get_successor(prefix);
	assert(successors); // If this is NULL somebody poked us in the eye.
	route = listnode_head(successors);
	length += (nbr->tlv_encoder)(eigrp, nbr, packet->s, route);

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
