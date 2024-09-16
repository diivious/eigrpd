// SPDX-License-Identifier: GPL-2.0-or-later
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
 */

#ifndef EIGRPD_EIGRP_FILTER_H_
#define EIGRPD_EIGRP_FILTER_H_

#include "distribute.h"

extern void eigrp_distribute_update(struct distribute_ctx *ctx,
				    struct distribute *dist);
extern void eigrp_distribute_update_all(struct prefix_list *plist);
extern void eigrp_distribute_update_all_wrapper(struct access_list *alist);
extern void eigrp_distribute_timer_process(struct event *event);
extern void eigrp_distribute_timer_interface(struct event *event);

#endif /* EIGRPD_EIGRP_FILTER_H_ */
