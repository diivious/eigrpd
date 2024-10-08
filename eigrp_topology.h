// SPDX-License-Identifier: GPL-2.0-or-later
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
  */

#ifndef _ZEBRA_EIGRP_TOPOLOGY_H
#define _ZEBRA_EIGRP_TOPOLOGY_H

/* EIGRP Route Descriptor related functions. */
extern eigrp_route_descriptor_t *eigrp_route_descriptor_new(eigrp_interface_t *);
extern void eigrp_route_descriptor_add(eigrp_instance_t *,
				       eigrp_prefix_descriptor_t *,
				       eigrp_route_descriptor_t *);
extern void eigrp_route_descriptor_delete(eigrp_instance_t *,
					  eigrp_prefix_descriptor_t *,
					  eigrp_route_descriptor_t *);
void eigrp_route_descriptor_free(eigrp_route_descriptor_t *);

/* EIGRP Topology table related functions. */
extern struct route_table *eigrp_topology_new(void);
extern void eigrp_topology_init(struct route_table *table);

extern eigrp_prefix_descriptor_t *eigrp_prefix_descriptor_new(void);

extern void eigrp_topology_free(eigrp_instance_t *eigrp, struct route_table *table);
extern void eigrp_prefix_descriptor_add(struct route_table *table,
					eigrp_prefix_descriptor_t *pe);
extern void eigrp_prefix_descriptor_delete(eigrp_instance_t *eigrp,
					   struct route_table *table,
					   eigrp_prefix_descriptor_t *pe);
extern void eigrp_topology_delete_all(eigrp_instance_t *eigrp,
				      struct route_table *table);
extern eigrp_prefix_descriptor_t *
eigrp_topology_table_lookup_ipv4(struct route_table *table, struct prefix *p);
extern struct list *eigrp_topology_get_successor(eigrp_prefix_descriptor_t *pe);
extern struct list *
eigrp_topology_get_successor_max(eigrp_prefix_descriptor_t *pe,
				 unsigned int maxpaths);
extern eigrp_route_descriptor_t *eigrp_prefix_descriptor_lookup(
    struct list *entries, eigrp_neighbor_t *neigh);
extern struct list *eigrp_neighbor_prefixes_lookup(eigrp_instance_t *eigrp,
						   eigrp_neighbor_t *n);
extern void eigrp_topology_update_all_node_flags(eigrp_instance_t *eigrp);
extern void eigrp_topology_update_node_flags(eigrp_instance_t *eigrp,
					     eigrp_prefix_descriptor_t *pe);
extern enum metric_change eigrp_topology_update_distance(eigrp_fsm_action_message_t *msg);
extern void eigrp_update_routing_table(eigrp_instance_t *eigrp,
				       eigrp_prefix_descriptor_t *pe);
extern void eigrp_topology_neighbor_down(eigrp_instance_t *eigrp,
					 eigrp_neighbor_t *neigh);
extern void eigrp_update_topology_table_prefix(eigrp_instance_t *eigrp,
					       struct route_table *table,
					       eigrp_prefix_descriptor_t *pe);

/* Static inline functions */
/* IPv4/IPv6 prefix and address management functions
 * might move to eigrp_addr.h if this grows
 */
static inline
void eigrp_addr_copy (eigrp_addr_t *dst, eigrp_addr_t *src)
{
    memcpy(dst, src, sizeof(eigrp_addr_t));
}

static inline
int eigrp_addr_same (eigrp_addr_t *dst, eigrp_addr_t *src)
{
    if (memcmp(dst, src, sizeof(eigrp_addr_t)) == 0)
	return TRUE;
    return FALSE;
}

#endif
