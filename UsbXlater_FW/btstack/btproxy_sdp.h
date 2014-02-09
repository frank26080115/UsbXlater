#ifndef BTPROXY_SDP_H
#define BTPROXY_SDP_H

void bpsdp_init();

#ifdef INCLUDING_FROM_SDP_C

static void bpsdp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
	// TODO: filter based on channel, prepare the proper service record according to channel
	sdp_packet_handler(packet_type, channel, packet, size);
}

void bpsdp_init()
{
	l2cap_register_service_internal(NULL, bpsdp_packet_handler, PSM_SDP, 0xffff, LEVEL_0);
}

#endif

#endif
