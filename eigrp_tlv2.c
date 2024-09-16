/*
 * EIGRP Packet Processor for EIGRP TLV2
 * Copyright (C) 2018
 * Authors:
 *   Donnie Savage
 */
#include "eigrpd/eigrpd.h"
#include "eigrpd/eigrp_types.h"
#include "eigrpd/eigrp_structs.h"

#include "eigrpd/eigrp_interface.h"
#include "eigrpd/eigrp_neighbor.h"
#include "eigrpd/eigrp_packet.h"
#include "eigrpd/eigrp_topology.h"
#include "eigrpd/eigrp_fsm.h"
#include "eigrpd/eigrp_metric.h"
#include "eigrpd/eigrp_dump.h"

/**
 * Number of useful macros which map to the packet
 */

// Vector Metrics



/**
 * extract the vector metric from the TLV and put it into a usable form
 */
static eigrp_route_descriptor_t *eigrp_tlv2_decoder(eigrp_instance_t *eigrp,
						    eigrp_neighbor_t *nbr,
						    eigrp_stream_t *pkt,
						    uint16_t pktlen)
{
	return 0;
}

static uint16_t eigrp_tlv2_encoder(eigrp_instance_t *eigrp, eigrp_neighbor_t *nbr,
				   eigrp_stream_t *pkt,
				   eigrp_route_descriptor_t *route)
{
	return 0;
}

void eigrp_tlv2_init(eigrp_neighbor_t *nbr)
{
	// setup vectors for processing Version 2 TLVs
	nbr->tlv_decoder = &eigrp_tlv2_decoder;
	nbr->tlv_encoder = &eigrp_tlv2_encoder;
}
