/*
 * EIGRP Interface Functions.
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

#ifndef _ZEBRA_EIGRP_INTERFACE_H_
#define _ZEBRA_EIGRP_INTERFACE_H_

/*Prototypes*/
extern void eigrp_intf_init(void);
extern int eigrp_intf_new_hook(struct interface *);
extern int eigrp_intf_delete_hook(struct interface *);

extern bool eigrp_intf_is_passive(eigrp_interface_t *ei);
extern void eigrp_del_intf_params(eigrp_intf_params_t *);
extern eigrp_interface_t *eigrp_intf_new(struct eigrp *, struct interface *,
					 struct prefix *);
extern int eigrp_intf_up(struct eigrp *, eigrp_interface_t *);
extern void eigrp_intf_update(struct eigrp *, struct interface *);
extern void eigrp_intf_set_multicast(eigrp_interface_t *);
extern uint8_t eigrp_default_iftype(struct interface *);
extern void eigrp_intf_free(struct eigrp *, eigrp_interface_t *, int);
extern int eigrp_intf_down(eigrp_interface_t *);
extern const char *eigrp_intf_name_string(eigrp_interface_t *);

extern int eigrp_intf_ipmulticast(struct eigrp *, struct prefix *,
				  unsigned int);
extern void eigrp_intf_update(struct eigrp *, struct interface *);
extern int eigrp_intf_add_allspfrouters(struct eigrp *, struct prefix *,
					unsigned int);
extern int eigrp_intf_drop_allspfrouters(struct eigrp *top, struct prefix *p,
					 unsigned int ifindex);

extern eigrp_interface_t *eigrp_intf_lookup_by_local_addr(struct eigrp *,
							  struct interface *,
							  struct in_addr);
extern eigrp_interface_t *eigrp_intf_lookup_by_name(struct eigrp *,
						    const char *);

/* Simulate down/up on the interface. */
extern void eigrp_intf_reset(struct interface *);

/* Static inline functions */
static inline const char *eigrp_intf_ip_string(eigrp_interface_t *ei)
{
	return ei ? inet_ntoa(ei->address.u.prefix4) : "inactive";
}

#endif /* ZEBRA_EIGRP_INTERFACE_H_ */
