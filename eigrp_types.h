/*
 * EIGRP Definition of Data Types
 * Copyright (C) 2018
 * Authors:
 *   Donnie Savage
 */
#ifndef _ZEBRA_EIGRP_TYPES_H_
#define _ZEBRA_EIGRP_TYPES_H_

#include "eigrpd/eigrp_const.h"
#include "eigrpd/eigrp_macros.h"

/**
 * Nice type modifers to make code more readable (and maybe portable)
 */
typedef struct stream eigrp_stream_t;

typedef uint64_t eigrp_bandwidth_t;
typedef uint64_t eigrp_delay_t;
typedef uint64_t eigrp_metric_t;
typedef uint32_t eigrp_scaled_t;

typedef uint32_t eigrp_system_metric_t;
typedef uint32_t eigrp_system_delay_t;
typedef uint32_t eigrp_system_bandwidth_t;

/**
 * define some primitive types for use in pointer passing. This will allow for
 * better type  checking, especially when dealing with classic metrics (32bit)
 * and wide metrics (64bit).
 *
 * If you need structure details, include the appropriate header file
 */
typedef struct eigrp_instance eigrp_instance_t;
typedef struct eigrp_interface eigrp_interface_t;
typedef struct eigrp_neighbor eigrp_neighbor_t;
typedef struct eigrp_metrics eigrp_metrics_t;
typedef struct eigrp_prefix_descriptor eigrp_prefix_descriptor_t;
typedef struct eigrp_route_descriptor eigrp_route_descriptor_t;
typedef struct eigrp_fsm_action_message eigrp_fsm_action_message_t;

// basic packet processor definitions
typedef struct eigrp_packet eigrp_packet_t;
typedef struct eigrp_tlv_header eigrp_tlv_header_t;

typedef eigrp_route_descriptor_t *(*eigrp_tlv_decoder_t)(eigrp_instance_t *eigrp,
							 eigrp_neighbor_t *nbr,
							 eigrp_stream_t *pkt,
							 uint16_t pktlen);
typedef uint16_t (*eigrp_tlv_encoder_t)(eigrp_instance_t *eigrp,
					eigrp_neighbor_t *nbr,
					eigrp_stream_t *pkt,
					eigrp_route_descriptor_t *route);

#endif /* _ZEBRA_EIGRP_TYPES_H_ */
