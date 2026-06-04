/*
 * EIGRP TLV2 codec API.
 * Copyright (C) 2026 Donnie V. Savage
 */
#ifndef _ZEBRA_EIGRP_TLV2_H
#define _ZEBRA_EIGRP_TLV2_H

extern void eigrp_tlv2_init(eigrp_tlv_codec_t *);
extern void eigrp_tlv2_neighbor_bind(eigrp_neighbor_t *, eigrp_tlv_codec_t *);
extern void eigrp_tlv2_interface_bind(eigrp_interface_t *, eigrp_tlv_codec_t *);

#endif /* _ZEBRA_EIGRP_TLV2_H */
