/*
 * EIGRP Packet Processor for EIGRP TLV version 1 processing
 * Copyright (C) 2018
 * Authors:
 *   Donnie Savage
 *
 * The intent of this file is to define a self contained TLV processor which
 * can be agnostic to the main EIGRP routing process.  This will eliminate
 * special processing to handle the narrow versus wide metrics and the TLV
 *packet format differences.   All data sent to, or returned from, this TLV
 *processor will be normalized to the system (meaning a delay is a delay and you
 *dont have to worry about conversion)
 *
 *
 **
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
#include "eigrpd/eigrpd.h"
#include "eigrpd/eigrp_types.h"
#include "eigrpd/eigrp_structs.h"

#include "eigrpd/eigrp_interface.h"
#include "eigrpd/eigrp_neighbor.h"
#include "eigrpd/eigrp_packet.h"
#include "eigrpd/eigrp_topology.h"
#include "eigrpd/eigrp_metric.h"
#include "eigrpd/eigrp_dump.h"

/**
 * Number of useful macros which map to the packet
 */

// Vector Metrics
#define EIGRP_TLV1_METRIC                                                      \
	uint32_t delay;                                                        \
	uint32_t bandwidth;                                                    \
	uint8_t mtu[3];                                                        \
	uint8_t hop_count;                                                     \
	uint8_t reliability;                                                   \
	uint8_t load;                                                          \
	uint8_t tag;                                                           \
	uint8_t flags;
#define EIGRP_TLV1_METRIC_SIZE 16

// Nexthop (0 if this router)
#define EIGRP_TLV1_IPV4_NEXTHOP uint32_t nexthop;
#define EIGRP_TLV1_IPV4_NEXTHOP_SIZE 4

// External Data for redistributed route
#define EIGRP_TLV1_EXTDATA                                                     \
	uint32_t orig;                                                         \
	uint32_t as;                                                           \
	uint32_t tag;                                                          \
	uint32_t metric;                                                       \
	uint16_t reserved;                                                     \
	uint8_t protocol;                                                      \
	uint8_t flags;
#define EIGRP_TLV1_EXTDATA_SIZE 20

/**
 * structure to lay over a packet for encoding/decoding
 */
typedef struct eigrp_tlv1_header {
	EIGRP_TLV_HDR
	EIGRP_TLV1_IPV4_NEXTHOP
} __attribute__((packed)) eigrp_tlv1_header_t;

typedef struct eigrp_tlv1_internal_ipv4 {
	EIGRP_TLV_HDR
	EIGRP_TLV1_IPV4_NEXTHOP
	EIGRP_TLV1_METRIC

	// destination address
	uint8_t prefix_length;
	unsigned char destination_part[4];
} __attribute__((packed)) eigrp_tlv1_internal_ipv4_t;

typedef struct eigrp_tlv1_external_ipv4 {
	EIGRP_TLV_HDR
	EIGRP_TLV1_IPV4_NEXTHOP
	EIGRP_TLV1_METRIC

	// destination address
	uint8_t prefix_length;
	unsigned char destination_part[4];
} __attribute__((packed)) eigrp_tlv1_external_ipv4_t;

typedef struct eigrp_tlv1_extdata {
	EIGRP_TLV1_EXTDATA
} __attribute__((packed)) eigrp_tlv1_extdata_t;

#define EIGRP_IPV4_MIN_TLV                                                     \
	(EIGRP_TLV_HDR_SIZE + EIGRP_TLV1_IPV4_NEXTHOP_SIZE                     \
	 + EIGRP_TLV1_METRIC_SIZE)

/**
 * extract the external route information provide by the
 * router redistributing the original route
 */
static uint16_t eigrp_tlv1_external_decode(eigrp_stream_t *pkt,
					   eigrp_extdata_t *extdata)
{
	extdata->orig = stream_getl(pkt);
	extdata->as = stream_getl(pkt);
	extdata->tag = stream_getl(pkt);
	extdata->metric = stream_getl(pkt);
	extdata->reserved = stream_getw(pkt);
	extdata->protocol = stream_getc(pkt);
	extdata->flags = stream_getc(pkt);

	return (EIGRP_TLV1_EXTDATA_SIZE);
}

static uint16_t eigrp_tlv1_external_encode(eigrp_stream_t *pkt,
					   eigrp_extdata_t *extdata)
{
	stream_putl(pkt, extdata->orig);
	stream_putl(pkt, extdata->as);
	stream_putl(pkt, extdata->tag);
	stream_putl(pkt, extdata->metric);
	stream_putw(pkt, extdata->reserved);
	stream_putc(pkt, extdata->protocol);
	stream_putc(pkt, extdata->flags);

	return (EIGRP_TLV1_EXTDATA_SIZE);
}

/**
 * extract the vector metric from the TLV and put it into a usable form
 */
static uint16_t eigrp_tlv1_metric_decode(eigrp_stream_t *pkt,
					 eigrp_metrics_t *metric)
{

	/* TLV1.2 provides metric in 32bit form, need to scale  */
	metric->delay = eigrp_scaled_to_delay(stream_getl(pkt));
	metric->bandwidth = stream_getl(pkt);
	metric->mtu[0] = stream_getc(pkt);
	metric->mtu[1] = stream_getc(pkt);
	metric->mtu[2] = stream_getc(pkt);
	metric->hop_count = stream_getc(pkt);
	metric->reliability = stream_getc(pkt);
	metric->load = stream_getc(pkt);
	metric->tag = stream_getc(pkt);
	metric->flags = stream_getc(pkt);

	return (EIGRP_TLV1_METRIC_SIZE);
}

static uint16_t eigrp_tlv1_metric_encode(eigrp_stream_t *pkt,
					 eigrp_metrics_t *metric)
{
	/*
	 * TLV1.2 supports classic metrics, need to scale it down to 32 bits
	 */
	stream_putl(pkt, eigrp_delay_to_scaled(metric->delay));

	stream_putl(pkt, metric->bandwidth);
	stream_putc(pkt, metric->mtu[2]);
	stream_putc(pkt, metric->mtu[1]);
	stream_putc(pkt, metric->mtu[0]);
	stream_putc(pkt, metric->hop_count);
	stream_putc(pkt, metric->reliability);
	stream_putc(pkt, metric->load);
	stream_putc(pkt, metric->tag);
	stream_putc(pkt, metric->flags);

	return (EIGRP_TLV1_METRIC_SIZE);
}

static uint16_t eigrp_tlv1_addr_decode(eigrp_stream_t *pkt,
				       eigrp_route_descriptor_t *route)
{
	eigrp_prefix_descriptor_t *prefix = route->prefix;
	struct prefix *dest = prefix->destination;
	uint16_t prefixlen;
	unsigned char prefixpart[4];

	dest->prefixlen = stream_getc(pkt);
	prefixlen = (dest->prefixlen + 7) / 8;
	switch (prefixlen) {
	case 1:
		prefixpart[0] = stream_getc(pkt);
		prefixpart[1] = 0;
		prefixpart[2] = 0;
		prefixpart[3] = 0;
		break;

	case 2:
		prefixpart[0] = stream_getc(pkt);
		prefixpart[1] = stream_getc(pkt);
		prefixpart[2] = 0;
		prefixpart[3] = 0;
		break;

	case 3:
		prefixpart[0] = stream_getc(pkt);
		prefixpart[1] = stream_getc(pkt);
		prefixpart[2] = stream_getc(pkt);
		prefixpart[3] = 0;
		break;

	case 4:
		prefixpart[0] = stream_getc(pkt);
		prefixpart[1] = stream_getc(pkt);
		prefixpart[2] = stream_getc(pkt);
		prefixpart[3] = stream_getc(pkt);
		break;

	default:
		zlog_err("%s: Unexpected prefix length: %d",
			 __PRETTY_FUNCTION__, prefixlen);
		return 0;
	}

	dest->u.prefix4.s_addr = ((prefixpart[3] << 24) | (prefixpart[2] << 16)
				  | (prefixpart[1] << 8) | (prefixpart[0]));

	return (prefixlen + 1);
}

static uint16_t eigrp_tlv1_addr_encode(eigrp_stream_t *pkt,
				       eigrp_route_descriptor_t *route)
{
	eigrp_prefix_descriptor_t *prefix = route->prefix;
	struct prefix *dest = prefix->destination;
	uint16_t prefixlen;

	stream_putc(pkt, dest->prefixlen);
	prefixlen = (dest->prefixlen + 7) / 8;

	stream_putc(pkt, dest->u.prefix4.s_addr & 0xFF);

	/* OK, i could add "((fallthrough)) to each of these, but thats less
	 * readable than replicating the lines. Plus the optimize will blow all
	 * this out anywat
	 */
	switch (prefixlen) {
	case 4:
		stream_putc(pkt, (dest->u.prefix4.s_addr >> 24) & 0xFF);
		stream_putc(pkt, (dest->u.prefix4.s_addr >> 16) & 0xFF);
		stream_putc(pkt, (dest->u.prefix4.s_addr >> 8) & 0xFF);
		stream_putc(pkt, (dest->u.prefix4.s_addr) & 0xFF);
		break;
	case 3:
		stream_putc(pkt, (dest->u.prefix4.s_addr >> 16) & 0xFF);
		stream_putc(pkt, (dest->u.prefix4.s_addr >> 8) & 0xFF);
		stream_putc(pkt, (dest->u.prefix4.s_addr) & 0xFF);
		break;
	case 2:
		stream_putc(pkt, (dest->u.prefix4.s_addr >> 8) & 0xFF);
		stream_putc(pkt, (dest->u.prefix4.s_addr) & 0xFF);
		break;
	case 1:
		stream_putc(pkt, (dest->u.prefix4.s_addr) & 0xFF);
		break;
	default:
		zlog_err("%s: Unexpected prefix length: %d",
			 __PRETTY_FUNCTION__, prefixlen);
		return 0;
	}

	return (prefixlen + 1);
}

static uint16_t eigrp_tlv1_nexthop_decode(struct eigrp *eigrp,
					  eigrp_stream_t *pkt,
					  eigrp_route_descriptor_t *route)
{

	// if doing no-nexthop-self, then use the of the source peer
	if (/*eigrp->no_nextop_self == */ FALSE) {
		route->nexthop.s_addr = stream_getl(pkt);
	} else {
		route->nexthop.s_addr = 0L;
	}
	return (EIGRP_TLV1_IPV4_NEXTHOP_SIZE);
}

static uint16_t eigrp_tlv1_nexthop_encode(eigrp_stream_t *pkt,
					  eigrp_route_descriptor_t *route)
{
	stream_putl(pkt, route->nexthop.s_addr);
	return (EIGRP_TLV1_IPV4_NEXTHOP_SIZE);
}

/**
 * decode an incoming TLV into a topology route and return it for processing
 */
static eigrp_route_descriptor_t *eigrp_tlv1_decoder(struct eigrp *eigrp,
						    eigrp_neighbor_t *nbr,
						    eigrp_stream_t *pkt,
						    uint16_t pktlen)
{
	eigrp_route_descriptor_t *route = NULL;
	uint16_t type, length;
	uint16_t bytes = 0;

	type = stream_getw(pkt);
	length = stream_getw(pkt);

	/* lets be sure we have at least enough info in the packet to
	 * process one TLV.  if not, then no point in even trying
	 */
	if ((length > pktlen) || (length < EIGRP_IPV4_MIN_TLV)) {
		if (IS_DEBUG_EIGRP_PACKET(0, RECV)) {
			zlog_debug("EIGRP TLV: Neighbor(%s) corrupt packet",
				   eigrp_neigh_ip_string(nbr));
		}

	} else {
		/* allocate buffer */
		route = eigrp_route_descriptor_new();
	}

	if (route) {
		route->type =
			(type == EIGRP_TLV_IPv4_EXT) ? EIGRP_EXT : EIGRP_INT;
		route->afi = AF_INET;

		/* decode nexthop */
		bytes += eigrp_tlv1_nexthop_decode(eigrp, pkt, route);

		/* figure out what type of TLV we are processing */
		switch (type) {
		case EIGRP_TLV_IPv4_EXT:
			bytes += eigrp_tlv1_external_decode(pkt,
							    &route->extdata);

			// fall though to internal processing to get metric and
			// route
			__attribute__((fallthrough));

		case EIGRP_TLV_IPv4_INT:
			bytes += eigrp_tlv1_metric_decode(pkt, &route->metric);

			/* metric and (optional) external info has been
			 * processed, now lets collect all the destination(s).
			 *
			 * NOTE:
			 *    While the RFC calls out for the ability to send
			 * multiple destinations in one TLV, its never been
			 * implemented.
			 *
			 * BOGO: we should consider adding this at some pooint,
			 * but this is classic metrics, so its likely to never
			 * get implemented. For now, I am going to ignore the
			 * additional any additional destintations afer the
			 * first one.
			 */
			bytes += eigrp_tlv1_addr_decode(pkt, route);
			break;

		default:
			if (IS_DEBUG_EIGRP_PACKET(0, RECV)) {
				zlog_debug(
					"EIGRP TLV: Neighbor(%s): invalid TLV_type(%u)",
					eigrp_neigh_ip_string(nbr), type);
			}
			break;
		}
	}

	return (route);
}

static uint16_t eigrp_tlv1_encoder(struct eigrp *eigrp, eigrp_neighbor_t *nbr,
				   eigrp_stream_t *pkt,
				   eigrp_prefix_descriptor_t *prefix)
{
	eigrp_route_descriptor_t *route;
	eigrp_interface_t *ei = nbr->ei;
	size_t tlv_start = stream_get_getp(pkt);
	size_t tlv_end;
	uint16_t type, length = 0;

	// grab the route from the prefix so we can get the metrics we need
	route = eigrp_prefix_descriptor_lookup(prefix->entries, nbr);
	type = route->type;

	// need to fix these up when we know the answer...
	stream_putw(pkt, type);
	stream_putw(pkt, length);
	length += 4;

	// encode nexthop
	length += eigrp_tlv1_nexthop_encode(pkt, route);

	switch (type) {
	case EIGRP_TLV_IPv4_EXT:
		length += eigrp_tlv1_external_encode(pkt, &route->extdata);
		length += eigrp_tlv1_metric_encode(pkt, &route->metric);
		length += eigrp_tlv1_addr_encode(pkt, route);
		break;

	case EIGRP_TLV_IPv4_INT:
		length += eigrp_tlv1_metric_encode(pkt, &route->metric);
		length += eigrp_tlv1_addr_encode(pkt, route);
		break;

	default:
		if (IS_DEBUG_EIGRP_PACKET(0, RECV)) {
			zlog_debug("Neighbor(%s): invalid TLV_type(%u)",
				   eigrp_neigh_ip_string(nbr), type);
		}
		break;
	}

	/* Filtering
	 * TODO: Work in progress
	 */
	if (ei) {
		if (eigrp_update_prefix_apply(eigrp, ei, EIGRP_FILTER_OUT,
					      prefix->destination)) {
			zlog_info(
				"Prefix Filtered:  Setting Metric to EIGRP_MAX_METRIC");
			route->metric.delay = EIGRP_MAX_METRIC;
		}
	}

	// now the fix up...
	tlv_end = stream_get_endp(pkt);

	stream_set_getp(pkt, tlv_start);
	stream_putw(pkt, type);
	stream_putw(pkt, length);
	stream_set_endp(pkt, tlv_end);

	return (length);
}

void eigrp_tlv1_init(eigrp_neighbor_t *nbr)
{
	// setup vectors for processing Version 1 TLVs
	nbr->tlv_decoder = &eigrp_tlv1_decoder;
	nbr->tlv_encoder = &eigrp_tlv1_encoder;
}
