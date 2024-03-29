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
