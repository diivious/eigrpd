// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * EIGRP Sending and Receiving EIGRP Query Packets.
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
#include "eigrpd/eigrp_topology.h"
#include "eigrpd/eigrp_interface.h"
#include "eigrpd/eigrp_neighbor.h"
#include "eigrpd/eigrp_packet.h"
#include "eigrpd/eigrp_auth.h"
#include "eigrpd/eigrp_fsm.h"

uint32_t eigrp_query_send_all(eigrp_instance_t *eigrp)
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
void eigrp_query_receive(eigrp_instance_t *eigrp, eigrp_neighbor_t *nbr,
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

void eigrp_query_send(eigrp_instance_t *eigrp, eigrp_interface_t *ei)
{
	eigrp_packet_t *packet = NULL;
	eigrp_neighbor_t *nbr;
	eigrp_prefix_descriptor_t *prefix;
	eigrp_route_descriptor_t *route;

	uint16_t length = EIGRP_HEADER_LEN;
	struct listnode *node, *nnode, *node2, *nnode2;
	struct list *successors;

	bool has_tlv = false;
	bool new_packet = true;
	uint16_t eigrp_mtu = EIGRP_PACKET_MTU(ei->ifp->mtu);

	for (ALL_LIST_ELEMENTS(ei->eigrp->topology_changes, node, nnode, prefix)) {
		if (!(prefix->req_action & EIGRP_FSM_NEED_QUERY))
			continue;

		if (new_packet) {
			packet = eigrp_packet_new(eigrp_mtu, NULL);

			/* Prepare EIGRP INIT UPDATE header */
			eigrp_packet_header_init(EIGRP_OPC_QUERY, ei->eigrp,
						 packet->s, 0,
						 ei->eigrp->sequence_number, 0);

			// encode Authentication TLV, if needed
			if ((ei->params.auth_type == EIGRP_AUTH_TYPE_MD5)
			    && (ei->params.auth_keychain != NULL)) {
				length +=
					eigrp_add_authTLV_MD5_encode(packet->s, ei);
			}
			new_packet = false;
		}

		// grab the route from the prefix so we can get the metrics we need
		successors = eigrp_topology_get_successor(prefix);
		assert(successors); // If this is NULL somebody poked us in the eye.
		route = listnode_head(successors);
		length += (nbr->tlv_encoder)(eigrp, nbr, packet->s, route);

		has_tlv = true;
		for (ALL_LIST_ELEMENTS(ei->nbrs, node2, nnode2, nbr)) {
			if (nbr->state == EIGRP_NEIGHBOR_UP)
				listnode_add(prefix->rij, nbr);
		}

		if (length + EIGRP_TLV_MAX_IPV4_BYTE > eigrp_mtu) {
			if ((ei->params.auth_type == EIGRP_AUTH_TYPE_MD5)
			    && ei->params.auth_keychain != NULL) {
				eigrp_make_md5_digest(ei, packet->s, EIGRP_AUTH_UPDATE_FLAG);
			}

			eigrp_packet_checksum(ei, packet->s, length);
			packet->length = length;

			packet->dst.ip.v4.s_addr = htonl(EIGRP_MULTICAST_ADDRESS);

			packet->sequence_number = ei->eigrp->sequence_number;
			ei->eigrp->sequence_number++;

			for (ALL_LIST_ELEMENTS(ei->nbrs, node2, nnode2, nbr)) {
				eigrp_packet_t *dup;

				if (nbr->state != EIGRP_NEIGHBOR_UP)
					continue;

				dup = eigrp_packet_duplicate(packet, nbr);
				/*Put packet to retransmission queue*/
				eigrp_packet_enqueue(nbr->retrans_queue, dup);

				if (nbr->retrans_queue->count == 1)
					eigrp_packet_send_reliably(eigrp, nbr);
			}

			has_tlv = false;
			length = 0;
			eigrp_packet_free(packet);
			packet = NULL;
			new_packet = true;
		}
	}

	if (!has_tlv)
		return;

	if ((ei->params.auth_type == EIGRP_AUTH_TYPE_MD5)
	    && ei->params.auth_keychain != NULL)
		eigrp_make_md5_digest(ei, packet->s, EIGRP_AUTH_UPDATE_FLAG);


	/* EIGRP Checksum */
	eigrp_packet_checksum(ei, packet->s, length);

	packet->length = length;
	packet->dst.ip.v4.s_addr = htonl(EIGRP_MULTICAST_ADDRESS);

	/*This ack number we await from neighbor*/
	packet->sequence_number = ei->eigrp->sequence_number;
	ei->eigrp->sequence_number++;

	for (ALL_LIST_ELEMENTS(ei->nbrs, node2, nnode2, nbr)) {
		eigrp_packet_t *dup;

		if (nbr->state != EIGRP_NEIGHBOR_UP)
			continue;

		dup = eigrp_packet_duplicate(packet, nbr);
		/*Put packet to retransmission queue*/
		eigrp_packet_enqueue(nbr->retrans_queue, dup);

		if (nbr->retrans_queue->count == 1)
			eigrp_packet_send_reliably(eigrp, nbr);
	}

	eigrp_packet_free(packet);
}
