/*
 * EIGRP Finite State Machine (DUAL).
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

#ifndef _ZEBRA_EIGRP_FSM_H
#define _ZEBRA_EIGRP_FSM_H

/* EIGRP Finite State Machine */
typedef enum {
	EIGRP_CONNECTED,
	EIGRP_INT,
	EIGRP_EXT,
} msg_data_t;

typedef struct eigrp_fsm_action_message {
	uint8_t packet_type;	      // UPDATE, QUERY, SIAQUERY, SIAREPLY
	struct eigrp *eigrp;	      // which thread sent mesg
	eigrp_neighbor_t *adv_router; // advertising neighbor
	eigrp_route_descriptor_t *route;
	eigrp_prefix_descriptor_t *prefix;
	msg_data_t data_type; // internal or external tlv type
	eigrp_metrics_t metrics;
	enum metric_change change;
} eigrp_fsm_action_message_t;

extern int eigrp_fsm_event(eigrp_fsm_action_message_t *msg);


#endif /* _ZEBRA_EIGRP_DUAL_H */
