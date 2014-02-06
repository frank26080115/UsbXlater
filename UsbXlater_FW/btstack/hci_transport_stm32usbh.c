#include <stdio.h>
#include <string.h>

#include "btstack-config.h"
#include "btstack-debug.h"
#include "btstack/src/hci.h"
#include "btstack/src/hci_transport.h"
#include "btstack/src/hci_dump.h"
#include "usbh_lib/usbh_core.h"
#include "usbh_dev/usbh_dev_bt_hci.h"

USB_OTG_CORE_HANDLE * BT_USBH_CORE;
USBH_DEV * BT_USBH_DEV;

typedef enum {
	STM32USBH_PACKET_TYPE,
	STM32USBH_EVENT_HEADER,
	STM32USBH_ACL_HEADER,
	STM32USBH_PAYLOAD,
} STM32USBH_STATE;

typedef struct hci_transport_stm32usbh {
	hci_transport_t transport;
	data_source_t *ds;
} hci_transport_stm32usbh_t;

static hci_transport_stm32usbh_t * hci_transport_stm32usbh = NULL;
static  STM32USBH_STATE stm32usbh_state;
static int bytes_to_read;
static int read_pos;
static uint8_t hci_packet[1+HCI_PACKET_BUFFER_SIZE]; // packet type + max(acl header + acl payload, event header + event data)

static int  stm32usbh_process(struct data_source *ds);
static void dummy_handler(uint8_t packet_type, uint8_t *packet, int size); 
static hci_uart_config_t *hci_uart_config;

static void (*packet_handler)(uint8_t packet_type, uint8_t *packet, int size) = dummy_handler;

static int stm32usbh_open(void *transport_config){
	hci_uart_config = (hci_uart_config_t*) transport_config;

	// set up data_source
	hci_transport_stm32usbh->ds = malloc(sizeof(data_source_t));
	if (!hci_transport_stm32usbh) return -1;
	hci_transport_stm32usbh->ds->process = stm32usbh_process;
	run_loop_add_data_source(hci_transport_stm32usbh->ds);

	return 0;
}

static int stm32usbh_close(){
	// first remove run loop handler
	run_loop_remove_data_source(hci_transport_stm32usbh->ds);

	// free struct
	if (hci_transport_stm32usbh->ds != 0) {
		free(hci_transport_stm32usbh->ds);
		hci_transport_stm32usbh->ds = NULL;
	}
	return 0;
}

static int stm32usbh_send_packet(uint8_t packet_type, uint8_t *packet, int size) {
	if (BT_USBH_CORE == 0 || BT_USBH_DEV == 0) {
		return 0;
	}

	char *data = (char*) packet;

	hci_dump_packet( (uint8_t) packet_type, 0, packet, size);

	// send header
	switch(packet_type)
	{
		case HCI_COMMAND_DATA_PACKET:
			USBH_Dev_BtHci_Command(BT_USBH_CORE, BT_USBH_DEV, packet, size);
			break;
		case HCI_ACL_DATA_PACKET:
			break;
		default:
			return 1;
	}

	return 0;
}

static void   stm32usbh_deliver_packet(void){
	if (read_pos < 3) return; // sanity check
	hci_dump_packet( hci_packet[0], 1, &hci_packet[1], read_pos-1);
	packet_handler(hci_packet[0], &hci_packet[1], read_pos-1);

	stm32usbh_state = STM32USBH_PACKET_TYPE;
	read_pos = 0;
	bytes_to_read = 1;
}

static void stm32usbh_statemachine(void){
	switch (stm32usbh_state) {

		case STM32USBH_PACKET_TYPE:
			if (hci_packet[0] == HCI_EVENT_PACKET){
				bytes_to_read = HCI_EVENT_HEADER_SIZE;
				stm32usbh_state = STM32USBH_EVENT_HEADER;
			} else if (hci_packet[0] == HCI_ACL_DATA_PACKET){
				bytes_to_read = HCI_ACL_HEADER_SIZE;
				stm32usbh_state = STM32USBH_ACL_HEADER;
			} else {
				log_error("stm32usbh_statemachine: invalid packet type 0x%02x\n", hci_packet[0]);
				read_pos = 0;
				bytes_to_read = 1;
			}
			break;

		case STM32USBH_EVENT_HEADER:
			bytes_to_read = hci_packet[2];
			stm32usbh_state = STM32USBH_PAYLOAD;
			break;

		case STM32USBH_ACL_HEADER:
			bytes_to_read = READ_BT_16( hci_packet, 3);
			stm32usbh_state = STM32USBH_PAYLOAD;
			break;

		case STM32USBH_PAYLOAD:
			stm32usbh_deliver_packet();
			break;
		default:
			break;
	}
}

void stm32usbh_handle_raw(uint8_t packet_type, uint8_t* data, int sz)
{
	if (stm32usbh_state == STM32USBH_PACKET_TYPE && read_pos == 0) {
		hci_packet[read_pos++] = packet_type;
	}

	for (int i = 0; i < sz; ) {
		int read_now = bytes_to_read;
		uint8_t x;
		ssize_t bytes_read = 0;
		while (read_now > 0 && i < sz) {
			hci_packet[read_pos++] = data[i++];
			bytes_to_read--;
			read_now--;
		}

		if (bytes_to_read <= 0) {
			stm32usbh_statemachine();
		}
	}
}

static int stm32usbh_can_send_packet_now(uint8_t packet_type)
{
	if (BT_USBH_CORE == 0 || BT_USBH_DEV == 0) {
		return 0;
	}
	return 1;
}

static void stm32usbh_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, int size)){
	packet_handler = handler;
}

int stm32usbh_process(struct data_source *ds) {
	if (BT_USBH_CORE == 0 || BT_USBH_DEV == 0) {
		return 0;
	}

	UsbHci_Data_t* uhd = (UsbHci_Data_t*)BT_USBH_DEV->Usr_Data;
	uhd->packet_handler = stm32usbh_handle_raw;

	USBH_Process(BT_USBH_CORE, BT_USBH_DEV);

	return 0;
}

static const char * stm32usbh_get_transport_name(){
	return "STM32 USBH";
}

static void dummy_handler(uint8_t packet_type, uint8_t *packet, int size){
}

// get h5 singleton
hci_transport_t * hci_transport_stm32usbh_instance() {
	if (hci_transport_stm32usbh == NULL) {
		hci_transport_stm32usbh = malloc( sizeof(hci_transport_stm32usbh_t));
		hci_transport_stm32usbh->ds										= NULL;
		hci_transport_stm32usbh->transport.open							= stm32usbh_open;
		hci_transport_stm32usbh->transport.close						= stm32usbh_close;
		hci_transport_stm32usbh->transport.send_packet					= stm32usbh_send_packet;
		hci_transport_stm32usbh->transport.register_packet_handler		= stm32usbh_register_packet_handler;
		hci_transport_stm32usbh->transport.get_transport_name			= stm32usbh_get_transport_name;
		hci_transport_stm32usbh->transport.set_baudrate					= NULL;
		hci_transport_stm32usbh->transport.can_send_packet_now			= stm32usbh_can_send_packet_now;
	}
	return (hci_transport_t *) hci_transport_stm32usbh;
}
