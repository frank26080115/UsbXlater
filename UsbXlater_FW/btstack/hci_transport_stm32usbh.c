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
static  STM32USBH_STATE stm32usbh_event_state;
static  STM32USBH_STATE stm32usbh_acl_state;
static int bytes_to_read_event;
static int read_pos_event;
static int bytes_to_read_acl;
static int read_pos_acl;
static uint8_t hci_event_packet[1+HCI_PACKET_BUFFER_SIZE]; // packet type + max(acl header + acl payload, event header + event data)
static uint8_t hci_acl_packet[1+HCI_PACKET_BUFFER_SIZE]; // packet type + max(acl header + acl payload, event header + event data)
static uint32_t last_read_timestamp;

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

	stm32usbh_acl_state = STM32USBH_PACKET_TYPE;
	read_pos_acl = 0;
	bytes_to_read_acl = 1;

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
			USBH_Dev_BtHci_TxData(BT_USBH_CORE, BT_USBH_DEV, packet, size);
			break;
		default:
			return 1;
	}

	return 0;
}

static void   stm32usbh_deliver_acl_packet(void){
	if (read_pos_acl < 3) return; // sanity check
	int len = read_pos_acl - 1;
	hci_dump_packet( hci_acl_packet[0], 1, &hci_acl_packet[1], len);

	packet_handler(hci_acl_packet[0], &hci_acl_packet[1], len);

	stm32usbh_acl_state = STM32USBH_PACKET_TYPE;
	read_pos_acl = 0;
	bytes_to_read_acl = 1;
}

static void   stm32usbh_deliver_event_packet(void){
	if (read_pos_event < 3) return; // sanity check
	int len = read_pos_event - 1;
	hci_dump_packet( hci_event_packet[0], 1, &hci_event_packet[1], len);

	packet_handler(hci_event_packet[0], &hci_event_packet[1], len);

	stm32usbh_event_state = STM32USBH_PACKET_TYPE;
	read_pos_event = 0;
	bytes_to_read_event = 1;
}

static void stm32usbh_timeout_check(void)
{
	if ((systick_1ms_cnt - last_read_timestamp) > 1000)
	{
		if (stm32usbh_event_state != STM32USBH_PACKET_TYPE && hci_event_packet[0] == HCI_EVENT_PACKET) {
			read_pos_event = hci_event_packet[2] + 3;
			stm32usbh_deliver_event_packet();
		}
		else if (stm32usbh_acl_state != STM32USBH_PACKET_TYPE && hci_acl_packet[0] == HCI_ACL_DATA_PACKET) {
			read_pos_acl = READ_BT_16( hci_acl_packet, 3) + 5;
			stm32usbh_deliver_acl_packet();
		}
		last_read_timestamp = systick_1ms_cnt;
	}
}

static void stm32usbh_acl_statemachine(void)
{
	switch (stm32usbh_acl_state) {

		case STM32USBH_PACKET_TYPE:
			if (hci_acl_packet[0] == HCI_EVENT_PACKET){
				bytes_to_read_acl = HCI_EVENT_HEADER_SIZE;
				stm32usbh_acl_state = STM32USBH_EVENT_HEADER;
			} else if (hci_acl_packet[0] == HCI_ACL_DATA_PACKET){
				bytes_to_read_acl = HCI_ACL_HEADER_SIZE;
				stm32usbh_acl_state = STM32USBH_ACL_HEADER;
			} else {
				log_error("stm32usbh_acl_statemachine: invalid packet type 0x%02x\n", hci_acl_packet[0]);
				read_pos_acl = 0;
				bytes_to_read_acl = 1;
			}
			break;

		case STM32USBH_EVENT_HEADER:
			bytes_to_read_acl = hci_acl_packet[2];
			stm32usbh_acl_state = STM32USBH_PAYLOAD;
			break;

		case STM32USBH_ACL_HEADER:
			bytes_to_read_acl = READ_BT_16( hci_acl_packet, 3);
			stm32usbh_acl_state = STM32USBH_PAYLOAD;
			break;

		case STM32USBH_PAYLOAD:
			stm32usbh_deliver_acl_packet();
			break;
		default:
			break;
	}
}

static void stm32usbh_event_statemachine(void)
{
	switch (stm32usbh_event_state) {

		case STM32USBH_PACKET_TYPE:
			if (hci_event_packet[0] == HCI_EVENT_PACKET){
				bytes_to_read_event = HCI_EVENT_HEADER_SIZE;
				stm32usbh_event_state = STM32USBH_EVENT_HEADER;
			} else if (hci_event_packet[0] == HCI_ACL_DATA_PACKET){
				bytes_to_read_event = HCI_ACL_HEADER_SIZE;
				stm32usbh_event_state = STM32USBH_ACL_HEADER;
			} else {
				log_error("stm32usbh_event_statemachine: invalid packet type 0x%02x\n", hci_event_packet[0]);
				read_pos_event = 0;
				bytes_to_read_event = 1;
			}
			break;

		case STM32USBH_EVENT_HEADER:
			bytes_to_read_event = hci_event_packet[2];
			stm32usbh_event_state = STM32USBH_PAYLOAD;
			break;

		case STM32USBH_ACL_HEADER:
			bytes_to_read_event = READ_BT_16( hci_event_packet, 3);
			stm32usbh_event_state = STM32USBH_PAYLOAD;
			break;

		case STM32USBH_PAYLOAD:
			stm32usbh_deliver_event_packet();
			break;
		default:
			break;
	}
}

void stm32usbh_handle_raw(uint8_t packet_type, uint8_t* data, int sz)
{
	last_read_timestamp = systick_1ms_cnt;

	//dbg_printf(DBGMODE_TRACE, "hci_handle_raw t:%d  L:%d  st:%d  btr:%d  rp:%d", packet_type, sz, stm32usbh_state, bytes_to_read, read_pos);
	//dbg_printf(DBGMODE_TRACE, "  d:{%02X %02X %02X %02X}\r\n", data[0], data[1], data[2], data[3]);

	if (packet_type == HCI_EVENT_PACKET)
	{
		if (stm32usbh_event_state == STM32USBH_PACKET_TYPE && read_pos_event == 0) {
			hci_event_packet[0] = packet_type;
			read_pos_event = 1;
			bytes_to_read_event = 0;
		}

		for (int i = 0; i < sz; ) {
			int read_now = bytes_to_read_event;
			uint8_t x;
			ssize_t bytes_read = 0;
			while (read_now > 0 && i < sz) {
				hci_event_packet[read_pos_event++] = data[i++];
				bytes_to_read_event--;
				read_now--;
			}

			if (bytes_to_read_event == 0) {
				stm32usbh_event_statemachine();
			}
		}
	}
	else if (packet_type == HCI_ACL_DATA_PACKET)
	{
		if (stm32usbh_acl_state == STM32USBH_PACKET_TYPE && read_pos_acl == 0) {
			hci_acl_packet[0] = packet_type;
			read_pos_acl = 1;
			bytes_to_read_acl = 0;
		}

		for (int i = 0; i < sz; ) {
			int read_now = bytes_to_read_acl;
			uint8_t x;
			ssize_t bytes_read = 0;
			while (read_now > 0 && i < sz) {
				hci_acl_packet[read_pos_acl++] = data[i++];
				bytes_to_read_acl--;
				read_now--;
			}

			if (bytes_to_read_acl == 0) {
				stm32usbh_acl_statemachine();
			}
		}
	}
}

static int stm32usbh_can_send_packet_now(uint8_t packet_type)
{
	if (BT_USBH_CORE == 0 || BT_USBH_DEV == 0) {
		dbg_printf(DBGMODE_ERR, "stm32usbh_can_send_packet_now, no USB device\r\n");
		return 0;
	}
	if (stm32usbh_acl_state != STM32USBH_PACKET_TYPE || read_pos_acl != 0) {
		return 0;
	}
	if (stm32usbh_event_state != STM32USBH_PACKET_TYPE || read_pos_event != 0) {
		return 0;
	}
	return USBH_Dev_BtHci_CanSendPacket(BT_USBH_CORE, BT_USBH_DEV);
}

static void stm32usbh_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, int size)){
	packet_handler = handler;
}

int stm32usbh_process(struct data_source *ds) {
	stm32usbh_timeout_check();
	return 1;
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
