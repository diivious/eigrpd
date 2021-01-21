/*
 * EIGRP General Sending and Receiving of EIGRP Packets.
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

#ifndef _ZEBRA_EIGRP_PACKET_H
#define _ZEBRA_EIGRP_PACKET_H

/**
 * every TLV has a header - might as well define it here
 */

#define EIGRP_TLV_HDR                                                          \
	uint16_t type;                                                         \
	uint16_t length;
#define EIGRP_TLV_HDR_SIZE 4

typedef struct eigrp_tlv_header {
	uint16_t type;
	uint16_t length;
} eigrp_tlv_header_t;

/*Prototypes*/
extern int eigrp_packet_read(struct thread *);
extern int eigrp_packet_write(struct thread *);

extern eigrp_packet_t *eigrp_packet_new(size_t, eigrp_neighbor_t *);
extern eigrp_packet_t *eigrp_packet_duplicate(eigrp_packet_t *,
					      eigrp_neighbor_t *);
extern void eigrp_packet_free(eigrp_packet_t *);
extern void eigrp_packet_delete(eigrp_interface_t *);
extern void eigrp_packet_header_init(int, struct eigrp *, struct stream *,
				     uint32_t, uint32_t, uint32_t);
extern void eigrp_packet_checksum(eigrp_interface_t *, struct stream *,
				  uint16_t);

extern eigrp_packet_queue_t *eigrp_packet_queue_new(void);
extern eigrp_packet_t *eigrp_packet_queue_next(eigrp_packet_queue_t *);
extern eigrp_packet_t *eigrp_packet_dequeue(eigrp_packet_queue_t *);
extern void eigrp_packet_enqueue(eigrp_packet_queue_t *, eigrp_packet_t *);
extern void eigrp_packet_queue_free(eigrp_packet_queue_t *);
extern void eigrp_packet_queue_reset(eigrp_packet_queue_t *);

extern void eigrp_packet_send_reliably(struct eigrp *, eigrp_neighbor_t *);

extern uint16_t eigrp_add_authTLV_MD5_encode(struct stream *,
					     eigrp_interface_t *);
extern uint16_t eigrp_add_authTLV_SHA256_encode(struct stream *,
						eigrp_interface_t *);

extern int eigrp_packet_unack_retrans(struct thread *);
extern int eigrp_packet_unack_multicast_retrans(struct thread *);

/**
 * Found in eigrp-tlv*.c for peer versioning of TLV decoding
 */
extern void eigrp_tlv1_init(eigrp_neighbor_t *);
extern void eigrp_tlv2_init(eigrp_neighbor_t *);

/*
 * untill there is reason to have their own header, these externs are found in
 * eigrp_hello.c
 */
extern void eigrp_sw_version_init(void);
extern void eigrp_hello_send(eigrp_interface_t *, uint8_t, struct in_addr *);
extern void eigrp_hello_send_ack(eigrp_neighbor_t *);
extern void eigrp_hello_receive(struct eigrp *, struct ip *,
				struct eigrp_header *, struct stream *,
				eigrp_interface_t *, int);
extern int eigrp_hello_timer(struct thread *);

/*
 * These externs are found in eigrp_update.c
 */
extern bool eigrp_update_prefix_apply(struct eigrp *eigrp,
				      eigrp_interface_t *ei, int in,
				      struct prefix *prefix);
extern void eigrp_update_send(struct eigrp *, eigrp_neighbor_t *,
			      eigrp_interface_t *);
extern void eigrp_update_receive(struct eigrp *, eigrp_neighbor_t *,
				 struct eigrp_header *, struct stream *,
				 eigrp_interface_t *, int);
extern void eigrp_update_send_all(struct eigrp *, eigrp_interface_t *);
extern void eigrp_update_send_init(struct eigrp *, eigrp_neighbor_t *);
extern void eigrp_update_send_EOT(eigrp_neighbor_t *);
extern int eigrp_update_send_GR_thread(struct thread *);
extern void eigrp_update_send_GR(eigrp_neighbor_t *, enum GR_type,
				 struct vty *);
extern void eigrp_update_send_interface_GR(eigrp_interface_t *, enum GR_type,
					   struct vty *);
extern void eigrp_update_send_process_GR(struct eigrp *, enum GR_type,
					 struct vty *);

/*
 * These externs are found in eigrp_query.c
 */
extern void eigrp_query_send(struct eigrp *, eigrp_interface_t *);
extern void eigrp_query_receive(struct eigrp *, eigrp_neighbor_t *,
				struct eigrp_header *, struct stream *,
				eigrp_interface_t *, int);
extern uint32_t eigrp_query_send_all(struct eigrp *);

/*
 * These externs are found in eigrp_reply.c
 */
extern void eigrp_reply_send(struct eigrp *, eigrp_neighbor_t *,
			     eigrp_prefix_descriptor_t *);
extern void eigrp_reply_receive(struct eigrp *, eigrp_neighbor_t *,
				struct eigrp_header *, struct stream *,
				eigrp_interface_t *, int);

/*
 * These externs are found in eigrp_siaquery.c
 */
extern void eigrp_siaquery_send(struct eigrp *, eigrp_neighbor_t *,
				eigrp_prefix_descriptor_t *);
extern void eigrp_siaquery_receive(struct eigrp *, eigrp_neighbor_t *,
				   struct eigrp_header *, struct stream *,
				   eigrp_interface_t *, int);

/*
 * These externs are found in eigrp_siareply.c
 */
extern void eigrp_siareply_send(struct eigrp *, eigrp_neighbor_t *,
				eigrp_prefix_descriptor_t *);
extern void eigrp_siareply_receive(struct eigrp *, eigrp_neighbor_t *,
				   struct eigrp_header *, struct stream *,
				   eigrp_interface_t *, int);

/*
 * These externs need to cleaned up
 */
extern struct TLV_MD5_Authentication_Type *eigrp_authTLV_MD5_new(void);
extern void eigrp_authTLV_MD5_free(struct TLV_MD5_Authentication_Type *);
extern struct TLV_SHA256_Authentication_Type *eigrp_authTLV_SHA256_new(void);
extern void eigrp_authTLV_SHA256_free(struct TLV_SHA256_Authentication_Type *);

extern int eigrp_make_md5_digest(eigrp_interface_t *, struct stream *, uint8_t);
extern int eigrp_check_md5_digest(struct stream *,
				  struct TLV_MD5_Authentication_Type *,
				  eigrp_neighbor_t *, uint8_t);
extern int eigrp_make_sha256_digest(eigrp_interface_t *, struct stream *,
				    uint8_t);
extern int eigrp_check_sha256_digest(struct stream *,
				     struct TLV_SHA256_Authentication_Type *,
				     eigrp_neighbor_t *, uint8_t);


extern void eigrp_IPv4_InternalTLV_free(struct TLV_IPv4_Internal_type *);

extern struct TLV_Sequence_Type *eigrp_SequenceTLV_new(void);

extern const struct message eigrp_packet_type_str[];
extern const size_t eigrp_packet_type_str_max;

#endif /* _ZEBRA_EIGRP_PACKET_H */
