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
#include "eigrpd/eigrp_packetizer.h"

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
		route = (nbr->decoder)(eigrp, nbr, pkt, length);
		if (!route)
			break;

		prefix = eigrp_topology_table_lookup_ipv4(eigrp->topology_table,
						       &route->dest);
		if (!prefix) {
			zlog_debug("EIGRP SIA-REPLY: Neighbor(%s) sent unknown prefix %s",
				   eigrp_print_addr(&nbr->src),
				   eigrp_print_prefix(&route->dest));
			eigrp_topology_route_free(route);
			continue;
		}
		route->prefix = prefix;

		/* If the destination exists, pass it to DUAL. */
		msg.packet_type = EIGRP_OPC_SIAREPLY;
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
}

void eigrp_siareply_send(eigrp_instance_t *eigrp, eigrp_neighbor_t *nbr,
			 eigrp_prefix_descriptor_t *prefix)
{
	eigrp_packetizer_work_t *work;

	work = eigrp_packetizer_work_new(EIGRP_OPC_SIAREPLY);
	work->nbr = nbr;
	work->prefix = prefix;
	work->owner = prefix;
	eigrp_packetizer_enqueue(eigrp, work);
}

