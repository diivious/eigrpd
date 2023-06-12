/*
 * EIGRP Authnication functions to support sending and receiving of EIGRP Packets.
 * Copyright (C) 2023
 * Authors:
 *   Donnie Savage
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

#ifndef _ZEBRA_EIGRP_AUTH_H
#define _ZEBRA_EIGRP_AUTH_H

#include "lib/keychain.h"
#include "lib/checksum.h"
#include "lib/md5.h"
#include "lib/sha256.h"

/*
 * These externs need to cleaned up
 */
extern int eigrp_make_md5_digest(eigrp_interface_t *, struct stream *, uint8_t);
extern int eigrp_check_md5_digest(struct stream *,
				  struct TLV_MD5_Authentication_Type *,
				  eigrp_neighbor_t *, uint8_t);
extern int eigrp_make_sha256_digest(eigrp_interface_t *, struct stream *,
				    uint8_t);
extern int eigrp_check_sha256_digest(struct stream *,
				     struct TLV_SHA256_Authentication_Type *,
				     eigrp_neighbor_t *, uint8_t);


extern struct TLV_SHA256_Authentication_Type *eigrp_authTLV_SHA256_new(void);
extern void eigrp_authTLV_SHA256_free(struct TLV_SHA256_Authentication_Type *);

extern struct TLV_MD5_Authentication_Type *eigrp_authTLV_MD5_new(void);
extern void eigrp_authTLV_MD5_free(struct TLV_MD5_Authentication_Type *);

extern uint16_t eigrp_add_authTLV_MD5_encode(struct stream *,
					     eigrp_interface_t *);
extern uint16_t eigrp_add_authTLV_SHA256_encode(struct stream *,
						eigrp_interface_t *);

#endif /* _ZEBRA_EIGRP_AUTH_H */


