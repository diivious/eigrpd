/*
 * EIGRP Network Related Functions.
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

#ifndef _ZEBRA_EIGRP_NETWORK_H
#define _ZEBRA_EIGRP_NETWORK_H

#include "lib/table.h"
#include "lib/sockopt.h"

/* Static inline functions */
/* IPv4/IPv6 prefix and address management functions
 * might move to eigrp_addr.h if this grows
 */
static inline const char *
eigrp_print_prefix(struct prefix *network)
{
    return inet_ntoa(network->u.prefix4);
}

static inline const char *
eigrp_print_addr(eigrp_addr_t *addr)
{
    return inet_ntoa(addr->ip.v4);
}

static inline const char *
eigrp_print_routerid(struct in_addr ipv4)
{
    return inet_ntoa(ipv4);
}

static inline const char *
eigrp_print_ifname(struct eigrp_interface *ei)
{
	if (!ei)
		return "inactive";

	return ei->ifp->name;
}

/* Prototypes */
extern int eigrp_sock_init(struct vrf *vrf);
extern int eigrp_network_set(eigrp_instance_t *eigrp, struct prefix *p);
extern int eigrp_network_unset(eigrp_instance_t *eigrp, struct prefix *p);

extern void eigrp_adjust_sndbuflen(eigrp_instance_t *, unsigned int);

extern void eigrp_external_routes_refresh(eigrp_instance_t *, int);

#endif /* EIGRP_NETWORK_H_ */
