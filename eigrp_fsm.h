// SPDX-License-Identifier: GPL-2.0-or-later
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
	uint8_t packet_type;			// UPDATE, QUERY, SIAQUERY, SIAREPLY
	eigrp_instance_t *eigrp;		// which event sent mesg
	eigrp_neighbor_t *adv_router;		// advertising neighbor
	eigrp_route_descriptor_t *route;	//
	eigrp_prefix_descriptor_t *prefix;	//
	msg_data_t data_type;			// internal or external tlv type
	eigrp_metrics_t metrics;		//
	enum metric_change change;		//
} eigrp_fsm_action_message_t;

extern int eigrp_fsm_event(eigrp_fsm_action_message_t *msg);


#endif /* _ZEBRA_EIGRP_DUAL_H */
