#include "btstack-config.h"

#include <stdio.h>
#include <string.h>

#include "btstack-debug.h"
#include "btstack/src/hci.h"
#include "btstack/src/hci_transport.h"
#include "btstack/src/hci_dump.h"
#include "btstack/ubcsp/ubcsp.h"
#include "hci_cereal.h"

static uint8_t h5_activity;
static uint8_t h5_tx_ready;
static struct ubcsp_packet h5_outgoing_packet;
static struct ubcsp_packet h5_incoming_packet;

typedef struct hci_transport_h5 {
	hci_transport_t transport;
	data_source_t *ds;
} hci_transport_h5_t;

// single instance
static hci_transport_h5_t * hci_transport_h5 = NULL;

static int  h5_process(struct data_source *ds);
static void dummy_handler(uint8_t packet_type, uint8_t *packet, int size); 
static hci_uart_config_t *hci_uart_config;

static void (*packet_handler)(uint8_t packet_type, uint8_t *packet, int size) = dummy_handler;

// prototypes
static int h5_open(void *transport_config){
	hci_uart_config = (hci_uart_config_t*) transport_config;
	h5_tx_ready = 1;

	// TODO, HW reset, open port

	// set up data_source
	hci_transport_h5->ds = malloc(sizeof(data_source_t));
	if (!hci_transport_h5) return -1;
	hci_transport_h5->ds->process = h5_process;
	run_loop_add_data_source(hci_transport_h5->ds);

	ubcsp_initialize();
	ubcsp_receive_packet(&h5_incoming_packet);

	return 0;
}

static int h5_close(){
	// first remove run loop handler
	run_loop_remove_data_source(hci_transport_h5->ds);

	// free struct
	if (hci_transport_h5->ds != 0) {
		free(hci_transport_h5->ds);
		hci_transport_h5->ds = NULL;
	}
	return 0;
}

static int h5_send_packet(uint8_t packet_type, uint8_t *packet, int size) {
	char *data = (char*) packet;

	hci_dump_packet( (uint8_t) packet_type, 0, packet, size);

	for (int i = 0; i < size; i++) {
		h5_outgoing_packet.payload[i] = packet[i];
	}

	h5_outgoing_packet.channel = packet_type;
	h5_outgoing_packet.reliable = 0;
	h5_outgoing_packet.use_crc = 0;
	h5_tx_ready = 0;
	ubcsp_send_packet(&h5_outgoing_packet);

	while (h5_tx_ready == 0) {
		h5_process(hci_transport_h5->ds);
	}

	return 0;
}

static int h5_can_send_packet_now(uint8_t packet_type)
{
	return h5_tx_ready;
}

static void h5_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, int size)){
	packet_handler = handler;
}

int h5_process(struct data_source *ds) {
	int d;

	do
	{
		d = ubcsp_poll(&h5_activity);

		if (h5_activity & UBCSP_PACKET_RECEIVED) {
			
			hci_dump_packet( h5_incoming_packet.channel, 1, h5_incoming_packet.payload, h5_incoming_packet.length);
			uint8_t packet_type;
			if (h5_incoming_packet.channel == 4) {
				packet_type = HCI_EVENT_PACKET;
			}
			else if (h5_incoming_packet.channel == 2) {
				packet_type = HCI_ACL_DATA_PACKET;
			}
			packet_handler(packet_type, h5_incoming_packet.payload, h5_incoming_packet.length);
			ubcsp_receive_packet(&h5_incoming_packet);
		}

		if (h5_activity & UBCSP_PACKET_SENT) {
			h5_tx_ready = 1;
		}
	}
	while (d == 0);

	return 0;
}

static const char * h5_get_transport_name(){
	return "H5";
}

static void dummy_handler(uint8_t packet_type, uint8_t *packet, int size){
}

// get h5 singleton
hci_transport_t * hci_transport_h5_instance() {
	if (hci_transport_h5 == NULL) {
		hci_transport_h5 = malloc( sizeof(hci_transport_h5_t));
		hci_transport_h5->ds										= NULL;
		hci_transport_h5->transport.open							= h5_open;
		hci_transport_h5->transport.close							= h5_close;
		hci_transport_h5->transport.send_packet						= h5_send_packet;
		hci_transport_h5->transport.register_packet_handler			= h5_register_packet_handler;
		hci_transport_h5->transport.get_transport_name				= h5_get_transport_name;
		hci_transport_h5->transport.set_baudrate					= hcicereal_set_baudrate;
		hci_transport_h5->transport.can_send_packet_now				= h5_can_send_packet_now;
	}
	return (hci_transport_t *) hci_transport_h5;
}
