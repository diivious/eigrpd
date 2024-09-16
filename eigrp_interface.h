// SPDX-License-Identifier: GPL-2.0-or-later
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
 */

#ifndef _ZEBRA_EIGRP_INTERFACE_H_
#define _ZEBRA_EIGRP_INTERFACE_H_

/*Prototypes*/
extern void eigrp_intf_init(void);
extern int eigrp_intf_new_hook(struct interface *);
extern int eigrp_intf_delete_hook(struct interface *);

extern bool eigrp_intf_is_passive(eigrp_interface_t *ei);
extern void eigrp_del_intf_params(eigrp_intf_params_t *);
extern eigrp_interface_t *eigrp_intf_new(eigrp_instance_t *, struct interface *,
					 struct prefix *);
extern int eigrp_intf_up(eigrp_instance_t *, eigrp_interface_t *);
extern void eigrp_intf_update(eigrp_instance_t *, struct interface *);
extern void eigrp_intf_set_multicast(eigrp_interface_t *);
extern uint8_t eigrp_default_iftype(struct interface *);
extern void eigrp_intf_free(eigrp_instance_t *, eigrp_interface_t *, int);
extern int eigrp_intf_down(eigrp_interface_t *);
extern const char *eigrp_intf_name_string(eigrp_interface_t *);

extern int eigrp_intf_ipmulticast(eigrp_instance_t *, struct prefix *,
				  unsigned int);
extern int eigrp_intf_add_allspfrouters(eigrp_instance_t *, struct prefix *,
					unsigned int);
extern int eigrp_intf_drop_allspfrouters(eigrp_instance_t *top, struct prefix *p,
					 unsigned int ifindex);

extern eigrp_interface_t *eigrp_intf_lookup_by_local_addr(eigrp_instance_t *,
							  struct interface *,
							  struct in_addr);
extern eigrp_interface_t *eigrp_intf_lookup_by_name(eigrp_instance_t *,
						    const char *);

/* Simulate down/up on the interface. */
extern void eigrp_intf_reset(struct interface *);

#endif /* ZEBRA_EIGRP_INTERFACE_H_ */
