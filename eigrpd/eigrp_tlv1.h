/*
 * EIGRP TLV1 codec API.
 * Copyright (C) 2026 Donnie V. Savage
 */
#ifndef _ZEBRA_EIGRP_TLV1_H
#define _ZEBRA_EIGRP_TLV1_H

extern void eigrp_tlv1_init(eigrp_tlv_codec_t *);
extern void eigrp_tlv1_neighbor_bind(eigrp_neighbor_t *, eigrp_tlv_codec_t *);
extern void eigrp_tlv1_interface_bind(eigrp_interface_t *, eigrp_tlv_codec_t *);

#endif /* _ZEBRA_EIGRP_TLV1_H */
