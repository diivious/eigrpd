/*
 * EIGRP Filter Functions.
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

#ifndef EIGRPD_EIGRP_FILTER_H_
#define EIGRPD_EIGRP_FILTER_H_

extern void eigrp_filter_distlist_install(eigrp_t *eigrp);
extern void eigrp_filter_distlist_update_wrapper(struct distribute_ctx *ctx, struct distribute *dist);
extern void eigrp_filter_access_update_wrapper(struct access_list *);
extern void eigrp_filter_prefix_update_wrapper(struct prefix_list *);

extern int eigrp_filter_timer_process(struct thread *);
extern int eigrp_filter_timer_interface(struct thread *);

#endif /* EIGRPD_EIGRP_FILTER_H_ */
