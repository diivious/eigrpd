// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * EIGRP Macros Definition.
 * Copyright (C) 2013-2014
 * Authors:
 *   Donnie Savage
 *   Jan Janovic
 *   Matej Perina
 *   Peter Orsag
 *   Peter Paluch
 *
 */

#ifndef _ZEBRA_EIGRP_MACROS_H_
#define _ZEBRA_EIGRP_MACROS_H_

//--------------------------------------------------------------------------

#define EIGRP_INTF_STRING_MAXLEN 40
#define EIGRP_INTF_NAME(I) eigrp_intf_name_string((I))

//--------------------------------------------------------------------------

#define EIGRP_PACKET_MTU(mtu) ((mtu) - (sizeof(struct ip)))


/* Event Macros */

#define EIGRP_EVENT_ADD_WRITE(E)					\
    event_add_write(eigrpd_event, eigrp_packet_write, (E), (E)->fd, &(E)->t_write)


#endif /* _ZEBRA_EIGRP_MACROS_H_ */
