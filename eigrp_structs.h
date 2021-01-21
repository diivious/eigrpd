/*
 * EIGRP Definition of Data Structures.
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

#ifndef _ZEBRA_EIGRP_STRUCTS_H_
#define _ZEBRA_EIGRP_STRUCTS_H_

#include "filter.h"

#include "eigrpd/eigrp_const.h"
#include "eigrpd/eigrp_types.h"
#include "eigrpd/eigrp_macros.h"

typedef struct eigrp_metrics {
	eigrp_delay_t delay;
	eigrp_bandwidth_t bandwidth;
	unsigned char mtu[3];
	uint8_t hop_count;
	uint8_t reliability;
	uint8_t load;
	uint8_t tag;
	uint8_t flags;
} eigrp_metrics_t;

typedef struct eigrp_extdata {
	uint32_t orig;
	uint32_t as;
	uint32_t tag;
	uint32_t metric;
	uint16_t reserved;
	uint8_t protocol;
	uint8_t flags;
} eigrp_extdata_t;

/*
 *DVS: this stuct is a mess of a dumping ground for any and everythign.
 *     will be cleaned up as it exposes to much to functions that do not need
 *it.
 *
 */
struct eigrp {
	vrf_id_t vrf_id;

	uint16_t AS;	     /* Autonomous system number */
	uint16_t vrid;	     /* Virtual Router ID */
	uint8_t k_values[6]; /*Array for K values configuration*/
	uint8_t variance;    /*Metric variance multiplier*/
	uint8_t max_paths;   /*Maximum allowed paths for 1 prefix*/

	/*Name of this EIGRP instance*/
	char *name;

	/* EIGRP Router ID. */
	struct in_addr router_id;	 /* Configured automatically. */
	struct in_addr router_id_static; /* Configured manually. */

	struct list *eiflist;		   /* eigrp interfaces */
	uint8_t passive_interface_default; /* passive-interface default */

	int fd;
	unsigned int maxsndbuflen;

	uint32_t sequence_number; /*Global EIGRP sequence number*/

	struct stream *ibuf;
	struct list *oi_write_q;

	/*Threads*/
	struct thread *t_write;
	struct thread *t_read;
	struct thread *t_distribute; /* timer for distribute list */

	struct route_table *networks; /* EIGRP config networks. */

	struct route_table *topology_table;

	uint64_t serno; /* Global serial number counter for topology entry
			   changes*/
	uint64_t serno_last_update; /* Highest serial number of information send
				       by last update*/
	struct list *topology_changes;

	/*Neighbor self*/
	eigrp_neighbor_t *neighbor_self;

	/*Configured metric for redistributed routes*/
	eigrp_metrics_t dmetric[ZEBRA_ROUTE_MAX + 1];
	int redistribute; /* Num of redistributed protocols. */

	/* Access-list. */
	struct access_list *list[EIGRP_FILTER_MAX];
	/* Prefix-list. */
	struct prefix_list *prefix[EIGRP_FILTER_MAX];
	/* Route-map. */
	struct route_map *routemap[EIGRP_FILTER_MAX];

	/* For redistribute route map. */
	struct {
		char *name;
		struct route_map *map;
		int metric_config;
		uint32_t metric;
	} route_map[ZEBRA_ROUTE_MAX];

	/* distribute_ctx */
	struct distribute_ctx *distribute_ctx;

	QOBJ_FIELDS
};
DECLARE_QOBJ_TYPE(eigrp)

typedef struct eigrp_packet_queue {
	eigrp_packet_t *head;
	eigrp_packet_t *tail;

	unsigned long count;
} eigrp_packet_queue_t;

typedef struct eigrp_intf_params {
	uint8_t passive_interface;
	uint32_t v_hello;
	uint16_t v_wait;
	uint8_t type; /* type of interface */
	uint32_t bandwidth;
	uint32_t delay;
	uint8_t reliability;
	uint8_t load;

	char *auth_keychain; /* Associated keychain with interface*/
	int auth_type;	     /* EIGRP authentication type */
} eigrp_intf_params_t;

enum { MEMBER_ALLROUTERS = 0,
       MEMBER_MAX,
};

typedef struct eigrp_intf_stats {
	struct {
		int ack;
		int hello;  /* Hello message input count. */
		int query;  /* Query message input count. */
		int reply;  /* Reply message input count. */
		int update; /* Update message input count. */
		int siaQuery;
		int siaReply;
	} rcvd;

	struct {
		int ack;
		int hello;  /* Hello message output count. */
		int query;  /* Query message output count. */
		int reply;  /* Reply message output count. */
		int update; /* Update message output count. */
		int siaQuery;
		int siaReply;
	} sent;
} eigrp_intf_stats_t;

/*EIGRP interface structure*/
typedef struct eigrp_interface {
	eigrp_intf_params_t params;

	/*multicast group refcnts */
	bool member_allrouters;

	/* This interface's parent eigrp instance. */
	struct eigrp *eigrp;

	/* Interface data from zebra. */
	struct interface *ifp;

	/* Packet send buffer. */
	eigrp_packet_queue_t *obuf; /* Output queue */

	/* To which multicast groups do we currently belong? */
	uint32_t curr_bandwidth;
	uint32_t curr_mtu;

	uint8_t multicast_memberships;
	struct {
		bool mixed;
		uint8_t v1;
		uint8_t v2;
	} version;

	/* EIGRP Network Type. */
	uint8_t type;

	/* Neighbor information. */
	struct list *nbrs; /* EIGRP Neighbor List */

	/* Threads. */
	struct thread *t_hello;	     /* timer */
	struct thread *t_distribute; /* timer for distribute list */

	int on_write_q;

	/* Statistics fields. */
	eigrp_intf_stats_t stats; // Statistics fields

	uint32_t crypt_seqnum; /* Cryptographic Sequence Number */

	struct prefix address; /* Interface prefix */

	/* Access-list. */
	struct access_list *list[EIGRP_FILTER_MAX];
	/* Prefix-list. */
	struct prefix_list *prefix[EIGRP_FILTER_MAX];
	/* Route-map. */
	struct route_map *routemap[EIGRP_FILTER_MAX];
} eigrp_interface_t;

/* Determines if it is first or last packet
 * when packet consists of multiple packet
 * chunks because of many route TLV
 * (all won't fit into one packet) */
enum Packet_part_type {
	EIGRP_PACKET_PART_NA,
	EIGRP_PACKET_PART_FIRST,
	EIGRP_PACKET_PART_LAST
};

//---------------------------------------------------------------------------------------------------------------------------------------------

typedef struct eigrp_packet {
	eigrp_packet_t *next;
	eigrp_packet_t *previous;

	/* Pointer to data stream. */
	struct stream *s;

	/* IP destination address. */
	struct in_addr dst;

	/*Packet retransmission thread*/
	struct thread *t_retrans_timer;

	/*Packet retransmission counter*/
	uint8_t retrans_counter;

	uint32_t sequence_number;

	/* EIGRP packet length. */
	uint16_t length;

	struct eigrp_neighbor *nbr;
} eigrp_packet_t;

struct eigrp_header {
	uint8_t version;
	uint8_t opcode;
	uint16_t checksum;
	uint32_t flags;
	uint32_t sequence;
	uint32_t ack;
	uint16_t vrid;
	uint16_t ASNumber;
	char *tlv[0];

} __attribute__((packed));


/**
 * Generic TLV type used for packet decoding.
 *
 *      +-----+------------------+
 *      |     |     |            |
 *      | Type| Len |    Vector  |
 *      |     |     |            |
 *      +-----+------------------+
 */
struct eigrp_tlv_hdr_type {
	uint16_t type;
	uint16_t length;
	uint8_t value[0];
} __attribute__((packed));

struct TLV_Parameter_Type {
	uint16_t type;
	uint16_t length;
	uint8_t K1;
	uint8_t K2;
	uint8_t K3;
	uint8_t K4;
	uint8_t K5;
	uint8_t K6;
	uint16_t hold_time;
} __attribute__((packed));

struct TLV_MD5_Authentication_Type {
	uint16_t type;
	uint16_t length;
	uint16_t auth_type;
	uint16_t auth_length;
	uint32_t key_id;
	uint32_t key_sequence;
	uint8_t Nullpad[8];
	uint8_t digest[EIGRP_AUTH_TYPE_MD5_LEN];

} __attribute__((packed));

struct TLV_SHA256_Authentication_Type {
	uint16_t type;
	uint16_t length;
	uint16_t auth_type;
	uint16_t auth_length;
	uint32_t key_id;
	uint32_t key_sequence;
	uint8_t Nullpad[8];
	uint8_t digest[EIGRP_AUTH_TYPE_SHA256_LEN];

} __attribute__((packed));

struct TLV_Sequence_Type {
	uint16_t type;
	uint16_t length;
	uint8_t addr_length;
	struct in_addr *addresses;
} __attribute__((packed));

struct TLV_Next_Multicast_Sequence {
	uint16_t type;
	uint16_t length;
	uint32_t multicast_sequence;
} __attribute__((packed));

struct TLV_Software_Type {
	uint16_t type;
	uint16_t length;
	uint8_t vender_major;
	uint8_t vender_minor;
	uint8_t eigrp_major;
	uint8_t eigrp_minor;
} __attribute__((packed));

struct TLV_IPv4_Internal_type {
	uint16_t type;
	uint16_t length;
	struct in_addr forward;

	/*Metrics*/
	struct eigrp_metrics metric;

	uint8_t prefix_length;

	struct in_addr destination;
} __attribute__((packed));

struct TLV_IPv4_External_type {
	uint16_t type;
	uint16_t length;
	struct in_addr next_hop;
	struct in_addr originating_router;
	uint32_t originating_as;
	uint32_t administrative_tag;
	uint32_t external_metric;
	uint16_t reserved;
	uint8_t external_protocol;
	uint8_t external_flags;

	/*Metrics*/
	struct eigrp_metrics metric;

	uint8_t prefix_length;
	unsigned char destination_part[4];
	struct in_addr destination;
} __attribute__((packed));

/* EIGRP Peer Termination TLV - used for hard restart */
struct TLV_Peer_Termination_type {
	uint16_t type;
	uint16_t length;
	uint8_t unknown;
	uint32_t neighbor_ip;
} __attribute__((packed));

/* Who executed Graceful restart */
enum GR_type { EIGRP_GR_MANUAL, EIGRP_GR_FILTER };

//---------------------------------------------------------------------------------------------------------------------------------------------

/* EIGRP Topology table node structure */
typedef struct eigrp_prefix_descriptor {
	struct list *entries, *rij;
	struct prefix *destination;

	eigrp_metrics_t reported_metric; // RD for sending
	uint32_t fdistance;		 // FD
	uint32_t rdistance;		 // RD
	uint32_t distance;		 // D

	uint8_t nt;	    // network type
	uint8_t state;	    // route FSM state
	uint8_t af;	    // address family
	uint8_t req_action; // required action

	// If network type is REMOTE_EXTERNAL, pointer will have reference to
	// its external TLV
	struct TLV_IPv4_External_type *extTLV;

	uint64_t serno; /*Serial number for this entry. Increased with each
			  change of entry*/
} eigrp_prefix_descriptor_t;

/* EIGRP Topology table record structure */
typedef struct eigrp_route_descriptor {
	uint16_t type;
	uint16_t afi; // ipv4 or ipv6

	eigrp_prefix_descriptor_t *prefix; // prefix this route is part of
	eigrp_neighbor_t *adv_router;	   // peer who sent me the route
	eigrp_addr_t nexthop;		   // ip address as advertised by peer

	uint32_t reported_distance; // distance reported by neighbor
	uint32_t distance;	    // reported distance + link cost to neighbor

	eigrp_metrics_t reported_metric;
	eigrp_metrics_t total_metric;

	eigrp_metrics_t metric;
	eigrp_extdata_t extdata;

	uint8_t flags; // used for marking successor and FS

	eigrp_interface_t *ei; // pointer for case of connected entry
} eigrp_route_descriptor_t;

//---------------------------------------------------------------------------------------------------------------------------------------------
typedef enum {
	EIGRP_CONNECTED,
	EIGRP_INT,
	EIGRP_EXT,
} msg_data_t;

/* EIGRP Finite State Machine */

typedef struct eigrp_fsm_action_message {
	uint8_t packet_type;	      // UPDATE, QUERY, SIAQUERY, SIAREPLY
	struct eigrp *eigrp;	      // which thread sent mesg
	eigrp_neighbor_t *adv_router; // advertising neighbor
	eigrp_route_descriptor_t *route;
	eigrp_prefix_descriptor_t *prefix;
	msg_data_t data_type; // internal or external tlv type
	eigrp_metrics_t metrics;
	enum metric_change change;
} eigrp_fsm_action_message_t;

#endif /* _ZEBRA_EIGRP_STRUCTURES_H_ */
