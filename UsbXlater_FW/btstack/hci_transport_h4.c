/*
 * Copyright (C) 2009-2012 by Matthias Ringwald
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at btstack@ringwald.ch
 *
 */

/*
 *  hci_h4_transport.c
 *
 *  HCI Transport API implementation for basic H4 protocol
 *
 *  Created by Matthias Ringwald on 4/29/09.
 */

#include "btstack-config.h"

#include <stdio.h>
#include <string.h>

#include "btstack-debug.h"
#include "btstack/src/hci.h"
#include "btstack/src/hci_transport.h"
#include "btstack/src/hci_dump.h"
#include "hci_cereal.h"

static int  h4_process(struct data_source *ds);
static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size); 
static      hci_uart_config_t *hci_uart_config;

typedef enum {
	H4_W4_PACKET_TYPE,
	H4_W4_EVENT_HEADER,
	H4_W4_ACL_HEADER,
	H4_W4_PAYLOAD,
} H4_STATE;

typedef struct hci_transport_h4 {
	hci_transport_t transport;
	data_source_t *ds;
	int uart_fd;    // different from ds->fd for HCI reader thread
	/* power management support, e.g. used by iOS */
	timer_source_t sleep_timer;
} hci_transport_h4_t;


// single instance
static hci_transport_h4_t * hci_transport_h4 = NULL;

static  void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size) = dummy_handler;

// packet reader state machine
static  H4_STATE h4_state;
static int bytes_to_read;
static int read_pos;

static uint8_t hci_packet[1+HCI_PACKET_BUFFER_SIZE]; // packet type + max(acl header + acl payload, event header + event data)

static int    h4_open(void *transport_config){
	hci_uart_config = (hci_uart_config_t*) transport_config;

	//hcicereal_init();

	// set up data_source
	hci_transport_h4->ds = (data_source_t*) malloc(sizeof(data_source_t));
	if (!hci_transport_h4->ds) return -1;
	hci_transport_h4->ds->process = h4_process;
	run_loop_add_data_source(hci_transport_h4->ds);

	// init state machine
	bytes_to_read = 1;
	h4_state = H4_W4_PACKET_TYPE;
	read_pos = 0;

	return 0;
}

static int h4_close(void *transport_config){
	// first remove run loop handler
	run_loop_remove_data_source(hci_transport_h4->ds);

	// free struct
	if (hci_transport_h4->ds != 0) {
		free(hci_transport_h4->ds);
		hci_transport_h4->ds = NULL;
	}
	return 0;
}

static int h4_send_packet(uint8_t packet_type, uint8_t * packet, int size){
	hci_dump_packet( (uint8_t) packet_type, 0, packet, size);
	unsigned char *data = (char*) packet;
	hcicereal_tx(packet_type);
	int i;
	for (i = 0; i < size; i++) {
		hcicereal_tx(data[i]);
	}
	return 0;
}

static void   h4_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
	packet_handler = handler;
}

static void   h4_deliver_packet(void){
	if (read_pos < 3) return; // sanity check
	hci_dump_packet( hci_packet[0], 1, &hci_packet[1], read_pos-1);
	packet_handler(hci_packet[0], &hci_packet[1], read_pos-1);

	h4_state = H4_W4_PACKET_TYPE;
	read_pos = 0;
	bytes_to_read = 1;
}

static void h4_statemachine(void){
	switch (h4_state) {

		case H4_W4_PACKET_TYPE:
			if (hci_packet[0] == HCI_EVENT_PACKET){
				bytes_to_read = HCI_EVENT_HEADER_SIZE;
				h4_state = H4_W4_EVENT_HEADER;
			} else if (hci_packet[0] == HCI_ACL_DATA_PACKET){
				bytes_to_read = HCI_ACL_HEADER_SIZE;
				h4_state = H4_W4_ACL_HEADER;
			} else {
				log_error("h4_process: invalid packet type 0x%02x\n", hci_packet[0]);
				read_pos = 0;
				bytes_to_read = 1;
			}
			break;

		case H4_W4_EVENT_HEADER:
			bytes_to_read = hci_packet[2];
			h4_state = H4_W4_PAYLOAD;
			break;

		case H4_W4_ACL_HEADER:
			bytes_to_read = READ_BT_16( hci_packet, 3);
			h4_state = H4_W4_PAYLOAD;
			break;

		case H4_W4_PAYLOAD:
			h4_deliver_packet();
			break;
		default:
			break;
	}
}

static int h4_process(struct data_source *ds) {

	int read_now = bytes_to_read;
	uint8_t x;
	ssize_t bytes_read = 0;
	while (read_now > 0) {
		if (hcicereal_get(&x) == 0) {
			break;
		}
		hci_packet[read_pos++] = x;
		bytes_to_read--;
		read_now--;
	}

	if (bytes_to_read > 0) {
		return 0;
	}

	h4_statemachine();
	return 0;
}

static const char * h4_get_transport_name(void){
	return "H4";
}

static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){
}

static int h4_can_send_packet_now(uint8_t packet_type)
{
	return hcicereal_tx_is_busy() == 0 ? 1 : 0;
}

// get h4 singleton
hci_transport_t * hci_transport_h4_instance() {
	if (hci_transport_h4 == NULL) {
		hci_transport_h4 = (hci_transport_h4_t*)malloc( sizeof(hci_transport_h4_t));
		hci_transport_h4->ds                                      = NULL;
		hci_transport_h4->transport.open                          = h4_open;
		hci_transport_h4->transport.close                         = h4_close;
		hci_transport_h4->transport.send_packet                   = h4_send_packet;
		hci_transport_h4->transport.register_packet_handler       = h4_register_packet_handler;
		hci_transport_h4->transport.get_transport_name            = h4_get_transport_name;
		hci_transport_h4->transport.set_baudrate                  = hcicereal_set_baudrate;
		hci_transport_h4->transport.can_send_packet_now           = h4_can_send_packet_now;
	}
	return (hci_transport_t *) hci_transport_h4;
}
