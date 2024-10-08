// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * EIGRP Filter Functions.
 * Copyright (C) 2013-2015
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
 */
#include "eigrpd/eigrpd.h"
#include "eigrpd/eigrp_structs.h"
#include "eigrpd/eigrp_const.h"
#include "eigrpd/eigrp_filter.h"
#include "eigrpd/eigrp_packet.h"

#include "plist.h"
#include "privs.h"
#include "vrf.h"

/*
 * Distribute-list update functions.
 */
void eigrp_distribute_update(struct distribute_ctx *ctx,
			     struct distribute *dist)
{
	eigrp_instance_t *eigrp = eigrp_lookup(ctx->vrf->vrf_id);
	struct interface *ifp;
	eigrp_interface_t *ei = NULL;
	struct access_list *alist;
	struct prefix_list *plist;
	// struct route_map *routemap;

	/* if no interface address is present, set list to eigrp process struct
	 */

	/* Check if distribute-list was set for process or interface */
	if (!dist->ifname) {
		/* access list IN for whole process */
		if (dist->list[DISTRIBUTE_V4_IN]) {
			alist = access_list_lookup(
				AFI_IP, dist->list[DISTRIBUTE_V4_IN]);
			if (alist)
				eigrp->list[EIGRP_FILTER_IN] = alist;
			else
				eigrp->list[EIGRP_FILTER_IN] = NULL;
		} else {
			eigrp->list[EIGRP_FILTER_IN] = NULL;
		}

		/* access list OUT for whole process */
		if (dist->list[DISTRIBUTE_V4_OUT]) {
			alist = access_list_lookup(
				AFI_IP, dist->list[DISTRIBUTE_V4_OUT]);
			if (alist)
				eigrp->list[EIGRP_FILTER_OUT] = alist;
			else
				eigrp->list[EIGRP_FILTER_OUT] = NULL;
		} else {
			eigrp->list[EIGRP_FILTER_OUT] = NULL;
		}

		/* PREFIX_LIST IN for process */
		if (dist->prefix[DISTRIBUTE_V4_IN]) {
			plist = prefix_list_lookup(
				AFI_IP, dist->prefix[DISTRIBUTE_V4_IN]);
			if (plist) {
				eigrp->prefix[EIGRP_FILTER_IN] = plist;
			} else
				eigrp->prefix[EIGRP_FILTER_IN] = NULL;
		} else
			eigrp->prefix[EIGRP_FILTER_IN] = NULL;

		/* PREFIX_LIST OUT for process */
		if (dist->prefix[DISTRIBUTE_V4_OUT]) {
			plist = prefix_list_lookup(
				AFI_IP, dist->prefix[DISTRIBUTE_V4_OUT]);
			if (plist) {
				eigrp->prefix[EIGRP_FILTER_OUT] = plist;

			} else
				eigrp->prefix[EIGRP_FILTER_OUT] = NULL;
		} else
			eigrp->prefix[EIGRP_FILTER_OUT] = NULL;

// This is commented out, because the distribute.[ch] code
// changes looked poorly written from first glance
// commit was 133bdf2d
// TODO: DBS
#if 0
	/* route-map IN for whole process */
	if (dist->route[DISTRIBUTE_V4_IN])
        {
	    routemap = route_map_lookup_by_name (dist->route[DISTRIBUTE_V4_IN]);
	    if (routemap)
		eigrp->routemap[EIGRP_FILTER_IN] = routemap;
	    else
		eigrp->routemap[EIGRP_FILTER_IN] = NULL;
        }
	else
        {
	    eigrp->routemap[EIGRP_FILTER_IN] = NULL;
        }

	/* route-map OUT for whole process */
	if (dist->route[DISTRIBUTE_V4_OUT])
        {
	    routemap = route_map_lookup_by_name (dist->route[DISTRIBUTE_V4_OUT]);
	    if (routemap)
		eigrp->routemap[EIGRP_FILTER_OUT] = routemap;
	    else
		eigrp->routemap[EIGRP_FILTER_OUT] = NULL;
        }
	else
        {
	    eigrp->routemap[EIGRP_FILTER_OUT] = NULL;
        }
#endif
		// TODO: check Graceful restart after 10sec

		/* check if there is already GR scheduled */
		if (eigrp->t_distribute != NULL) {
			/* if is, cancel schedule */
			event_cancel(&(eigrp->t_distribute));
		}
		/* schedule Graceful restart for whole process in 10sec */
		eigrp->t_distribute = NULL;
		event_add_timer(eigrpd_event, eigrp_distribute_timer_process, eigrp,
				 (10), &eigrp->t_distribute);

		return;
	}

	ifp = if_lookup_by_name(dist->ifname, eigrp->vrf_id);
	if (ifp == NULL)
		return;

	/*struct eigrp_intf_info * info = ifp->info;
	  ei = info->eigrp_interface;*/
	struct listnode *node, *nnode;
	eigrp_interface_t *ei2;
	/* Find proper interface */
	for (ALL_LIST_ELEMENTS(eigrp->eiflist, node, nnode, ei2)) {
		if (strcmp(ei2->ifp->name, ifp->name) == 0) {
			ei = ei2;
			break;
		}
	}
	assert(ei != NULL);

	/* Access-list for interface in */
	if (dist->list[DISTRIBUTE_V4_IN]) {
		alist = access_list_lookup(AFI_IP,
					   dist->list[DISTRIBUTE_V4_IN]);
		if (alist) {
			ei->list[EIGRP_FILTER_IN] = alist;
		} else
			ei->list[EIGRP_FILTER_IN] = NULL;
	} else {
		ei->list[EIGRP_FILTER_IN] = NULL;
	}

	/* Access-list for interface in */
	if (dist->list[DISTRIBUTE_V4_OUT]) {
		alist = access_list_lookup(AFI_IP,
					   dist->list[DISTRIBUTE_V4_OUT]);
		if (alist)
			ei->list[EIGRP_FILTER_OUT] = alist;
		else
			ei->list[EIGRP_FILTER_OUT] = NULL;

	} else
		ei->list[EIGRP_FILTER_OUT] = NULL;

	/* Prefix-list for interface in */
	if (dist->prefix[DISTRIBUTE_V4_IN]) {
		plist = prefix_list_lookup(AFI_IP,
					   dist->prefix[DISTRIBUTE_V4_IN]);
		if (plist)
			ei->prefix[EIGRP_FILTER_IN] = plist;
		else
			ei->prefix[EIGRP_FILTER_IN] = NULL;
	} else
		ei->prefix[EIGRP_FILTER_IN] = NULL;

	/* Prefix-list for interface out */
	if (dist->prefix[DISTRIBUTE_V4_OUT]) {
		plist = prefix_list_lookup(AFI_IP,
					   dist->prefix[DISTRIBUTE_V4_OUT]);
		if (plist)
			ei->prefix[EIGRP_FILTER_OUT] = plist;
		else
			ei->prefix[EIGRP_FILTER_OUT] = NULL;
	} else
		ei->prefix[EIGRP_FILTER_OUT] = NULL;

	// TODO: check Graceful restart after 10sec

	/* Cancel GR scheduled */
	event_cancel(&(ei->t_distribute));

	/* schedule Graceful restart for interface in 10sec */
	eigrp->t_distribute = NULL;
	event_add_timer(eigrpd_event, eigrp_distribute_timer_interface, ei, 10,
			 &eigrp->t_distribute);
}

/*
 * Function called by prefix-list and access-list update
 */
static void eigrp_distribute_update_interface(eigrp_instance_t *eigrp,
					      struct interface *ifp)
{
	struct distribute *dist;

	dist = distribute_lookup(eigrp->distribute_ctx, ifp->name);
	if (dist)
		eigrp_distribute_update(eigrp->distribute_ctx, dist);
}

/* Update all interface's distribute list.
 * Function used in hook for prefix-list
 */
void eigrp_distribute_update_all(struct prefix_list *notused)
{
	eigrp_instance_t *eigrp;
	struct vrf *vrf;
	struct interface *ifp;

	RB_FOREACH (vrf, vrf_name_head, &vrfs_by_name) {
		eigrp = eigrp_lookup(vrf->vrf_id);
		if (eigrp) {
			FOR_ALL_INTERFACES (vrf, ifp) {
				eigrp_distribute_update_interface(eigrp, ifp);
			}
		}
	}
}

/*
 * Function used in hook for acces-list
 */
void eigrp_distribute_update_all_wrapper(struct access_list *notused)
{
	eigrp_distribute_update_all(NULL);
}

/*
 * @fn eigrp_distribute_timer_process
 *
 * @param[in]   event  current execution event timer is associated with
 *
 * @return int  always returns 0
 *
 * @par
 * Called when 10sec waiting time expire and
 * executes Graceful restart for whole process
 */
void eigrp_distribute_timer_process(struct event *event)
{
	eigrp_instance_t *eigrp;

	eigrp = EVENT_ARG(event);
	eigrp->t_distribute = NULL;

	/* execute GR for whole process */
	eigrp_update_send_process_GR(eigrp, EIGRP_GR_FILTER, NULL);

	return;
}

/*
 * @fn eigrp_distribute_timer_interface
 *
 * @param[in]   event  current execution event timer is associated with
 *
 * @return int  always returns 0
 *
 * @par
 * Called when 10sec waiting time expire and
 * executes Graceful restart for interface
 */
void eigrp_distribute_timer_interface(struct event *event)
{
	eigrp_interface_t *ei;

	ei = EVENT_ARG(event);
	ei->t_distribute = NULL;

	/* execute GR for interface */
	eigrp_update_send_interface_GR(ei, EIGRP_GR_FILTER, NULL);

	return;
}
