/*
 * EIGRP Topology Table.
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

#ifndef _ZEBRA_EIGRP_TOPOLOGY_H
#define _ZEBRA_EIGRP_TOPOLOGY_H

/* EIGRP Topology table related functions. */
extern struct route_table *eigrp_topology_new(void);
extern void eigrp_topology_init(struct route_table *table);

extern eigrp_prefix_descriptor_t *eigrp_prefix_descriptor_new(void);
extern eigrp_route_descriptor_t *eigrp_route_descriptor_new(void);

extern void eigrp_topology_free(struct eigrp *eigrp, struct route_table *table);
extern void eigrp_prefix_descriptor_add(struct route_table *table,
					eigrp_prefix_descriptor_t *pe);
extern void eigrp_route_descriptor_add(struct eigrp *eigrp,
				       eigrp_prefix_descriptor_t *pe,
				       eigrp_route_descriptor_t *ne);
extern void eigrp_prefix_descriptor_delete(struct eigrp *eigrp,
					   struct route_table *table,
					   eigrp_prefix_descriptor_t *pe);
extern void eigrp_route_descriptor_delete(struct eigrp *eigrp,
					  eigrp_prefix_descriptor_t *pe,
					  eigrp_route_descriptor_t *ne);
extern void eigrp_topology_delete_all(struct eigrp *eigrp,
				      struct route_table *table);
extern eigrp_prefix_descriptor_t *
eigrp_topology_table_lookup_ipv4(struct route_table *table, struct prefix *p);
extern struct list *eigrp_topology_get_successor(eigrp_prefix_descriptor_t *pe);
extern struct list *
eigrp_topology_get_successor_max(eigrp_prefix_descriptor_t *pe,
				 unsigned int maxpaths);
extern eigrp_route_descriptor_t *
eigrp_prefix_descriptor_lookup(struct list *entries, eigrp_neighbor_t *neigh);
extern struct list *eigrp_neighbor_prefixes_lookup(struct eigrp *eigrp,
						   eigrp_neighbor_t *n);
extern void eigrp_topology_update_all_node_flags(struct eigrp *eigrp);
extern void eigrp_topology_update_node_flags(struct eigrp *eigrp,
					     eigrp_prefix_descriptor_t *pe);
extern enum metric_change
eigrp_topology_update_distance(eigrp_fsm_action_message_t *msg);
extern void eigrp_update_routing_table(struct eigrp *eigrp,
				       eigrp_prefix_descriptor_t *pe);
extern void eigrp_topology_neighbor_down(struct eigrp *eigrp,
					 eigrp_neighbor_t *neigh);
extern void eigrp_update_topology_table_prefix(struct eigrp *eigrp,
					       struct route_table *table,
					       eigrp_prefix_descriptor_t *pe);

/* Static inline functions */
static inline const char *
eigrp_topology_ip_string(eigrp_prefix_descriptor_t *tn)
{
	return inet_ntoa(tn->destination->u.prefix4);
}

#endif
