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
#include "eigrpd/eigrp_packetizer.h"

uint32_t eigrp_query_send_all(eigrp_instance_t *eigrp)
{
	eigrp_packetizer_work_t *work;
	eigrp_prefix_descriptor_t *prefix;
	struct listnode *node, *nnode;
	uint32_t counter = 0;

	if (!eigrp)
		return 0;

	for (ALL_LIST_ELEMENTS(eigrp->topology_changes, node, nnode, prefix)) {
		if (!(prefix->req_action & EIGRP_FSM_NEED_QUERY))
			continue;

		work = eigrp_packetizer_work_new(EIGRP_OPC_QUERY);
		work->prefix = prefix;
		work->owner = prefix;
		eigrp_packetizer_enqueue(eigrp, work);

		prefix->req_action &= ~EIGRP_FSM_NEED_QUERY;
		if (!prefix->req_action)
			listnode_delete(eigrp->topology_changes, prefix);

		counter++;
	}

	return counter;
}

/*EIGRP QUERY read function*/
void eigrp_query_receive(eigrp_instance_t *eigrp, eigrp_neighbor_t *nbr,
			 struct eigrp_header *eigrph, struct stream *pkt,
			 eigrp_interface_t *ei, int length)
{
	eigrp_fsm_action_message_t msg;
	eigrp_prefix_descriptor_t *prefix;
	eigrp_route_descriptor_t *route;

	/* increment statistics. */
	ei->stats.rcvd.query++;

	/* get neighbor struct */
	nbr->recv_sequence_number = ntohl(eigrph->sequence);

	// process all TLVs in the packet
	while (pkt->endp > pkt->getp) {
		route = (nbr->tlv_decoder)(eigrp, nbr, pkt, length);

		// should have got route off the packet, but one never knows
		if (!route)
			break;

		prefix = eigrp_topology_table_lookup_ipv4(eigrp->topology_table,
						       &route->dest);
		if (!prefix) {
			zlog_debug("EIGRP QUERY: Neighbor(%s) sent unknown prefix %s",
				   eigrp_print_addr(&nbr->src),
				   eigrp_print_prefix(&route->dest));
			eigrp_route_descriptor_free(route);
			continue;
		}
		route->prefix = prefix;

		msg.packet_type = EIGRP_OPC_QUERY;
		msg.eigrp = eigrp;
		msg.data_type = (route->type == EIGRP_TLV_IPv4_EXT)
					? EIGRP_EXT
					: EIGRP_INT;
		msg.adv_router = nbr;
		msg.route = route;
		msg.metrics = route->metric;
		msg.prefix = prefix;
		eigrp_fsm_event(&msg);
	}

	eigrp_hello_send_ack(nbr);
	eigrp_query_send_all(eigrp);
	eigrp_update_send_all(eigrp, nbr->ei);
}
