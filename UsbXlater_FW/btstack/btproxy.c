#include "btproxy.h"
#include "stm32fx/peripherals.h"
#include <ds4_emu.h>
#include <btstack/btstack.h>
#include <btstack/hci_transports_btproxy.h>
#include <btstack/hci_cereal.h>
#include <btstack/src/l2cap.h>
#include <btstack/src/sdp.h>
#include <btstack/src/hci.h>
#include <btstack/src/btstack_memory.h>
#include <btstack/memory_pool.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <btstack/sdp_util.h>

hci_transport_t * btproxy_transport;

#define BTPROXY_BUFF_SZ 1024
uint8_t btproxy_d2p_buff[BTPROXY_BUFF_SZ];
uint8_t btproxy_p2d_buff[BTPROXY_BUFF_SZ];

int btproxy_init_statemachine;
int btproxy_bccmd_seq;

void btproxy_hw_error();
int btproxy_baudrate_cmd(void * config, uint32_t baudrate, uint8_t *hci_cmd_buffer);
void btproxy_svc_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size);
void btproxy_l2cap_packet_handler(void * connection, uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size);
void btproxy_sdp_packet_handler(void * connection, uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size);
void btproxy_packet_handler(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
CMD_ACTION_t btproxy_init_cmd(void * obj, uint8_t * hci_cmd_buffer);
extern const remote_device_db_t btproxy_db_memory;
uint8_t* bt_extended_inquiry_response;

void btproxy_init_RN42HCI()
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

	// use the GPIO pin to do a hardware reset
	GPIO_InitTypeDef gi;
	GPIO_StructInit(&gi);
	gi.GPIO_Pin = GPIO_Pin_5;
	gi.GPIO_Mode = GPIO_Mode_OUT;
	gi.GPIO_Speed = GPIO_Speed_50MHz;
	gi.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOA, &gi);
	//GPIO_SetBits(GPIOA, GPIO_Pin_5);
	delay_ms(1);
	GPIO_ResetBits(GPIOA, GPIO_Pin_5);
	hcicereal_init();
	delay_ms(5);
	GPIO_SetBits(GPIOA, GPIO_Pin_5);

	while ((systick_1ms_cnt - hcicereal_lastRxTimestamp) < 3000); // handle any unwanted initial error messages
	hcicereal_rx_flush();

	static hci_uart_config_t ucfg;
	ucfg.baudrate_init = 115200;
	ucfg.baudrate_main = 0;//1500000;

	static bt_control_t btctrlcfg;
	btctrlcfg.baudrate_cmd = btproxy_baudrate_cmd;

	btproxy_transport = hci_transport_h4_instance();

	btproxy_init(&ucfg, &btctrlcfg, btproxy_transport);
}

void btproxy_init_USB()
{
	static hci_uart_config_t ucfg;
	ucfg.baudrate_init = 0;
	ucfg.baudrate_main = 0;

	static bt_control_t btctrlcfg;
	btctrlcfg.baudrate_cmd = 0;

	btproxy_transport = hci_transport_stm32usbh_instance();

	btproxy_init(&ucfg, &btctrlcfg, btproxy_transport);
}

void btproxy_init(hci_uart_config_t* ucfg, bt_control_t* btctrlcfg, hci_transport_t* xport)
{
	bt_extended_inquiry_response = ds4_extended_inquiry_response;

	btctrlcfg->hw_error = btproxy_hw_error;
	btctrlcfg->next_cmd = btproxy_init_cmd;
	btproxy_init_statemachine = 0;
	btstack_memory_init();
	run_loop_init(RUN_LOOP_BTPROXY);
	hci_init(xport, ucfg, btctrlcfg, &btproxy_db_memory);
	hci_set_class_of_device(0x00002508); // pretend to be a DS4, which is HID
	hci_set_local_name("Wireless Controller");
	hci_ssp_set_enable(1);
	hci_ssp_set_io_capability(SSP_IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
	hci_ssp_set_authentication_requirement(SSP_IO_AUTHREQ_MITM_PROTECTION_NOT_REQUIRED_GENERAL_BONDING);
	hci_ssp_set_auto_accept(1);
	l2cap_init();
	l2cap_register_packet_handler(btproxy_packet_handler);
	hci_power_control(HCI_POWER_ON);

	dbg_printf(DBGMODE_TRACE, "btproxy_init finished\r\n");
}

void btproxy_task()
{
	btproxy_runloop_execute_once();
}

void btproxy_svc_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size)
{
	char handled = 0;
	dbg_printf(DBGMODE_TRACE, "btproxy_svc_packet_handler, 0x%02X, 0x%04X, %d\r\n", packet_type, channel, size);
	if (handled == 0) {
		btproxy_packet_handler(NULL, packet_type, channel, packet, size);
	}
}

void btproxy_l2cap_packet_handler(void * connection, uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size)
{
	char handled = 0;
	dbg_printf(DBGMODE_TRACE, "btproxy_l2cap_packet_handler, 0x%02X, 0x%04X, %d\r\n", packet_type, channel, size);
	if (handled == 0) {
		btproxy_packet_handler(connection, packet_type, channel, packet, size);
	}
}

void btproxy_sdp_packet_handler(void * connection, uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size)
{
	char handled = 0;
	dbg_printf(DBGMODE_TRACE, "btproxy_sdp_packet_handler, 0x%02X, 0x%04X, %d\r\n", packet_type, channel, size);
	if (handled == 0) {
		btproxy_packet_handler(connection, packet_type, channel, packet, size);
	}
}

void btproxy_packet_handler(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
	char handled = 0;
	uint8_t num_of_cmds;
	uint16_t cmd_opcode;
	uint8_t cmd_status;

	//dbg_printf(DBGMODE_TRACE, "btproxy_packet_handler, 0x%02X, 0x%04X, %d\r\n", packet_type, channel, size);

	switch (packet_type)
	{
		case HCI_EVENT_PACKET:
			switch (packet[0])
			{
				case BTSTACK_EVENT_STATE:
					// bt stack activated, get started - set local name
					if (packet[2] == HCI_STATE_WORKING) {
						sdp_init();
						sdp_register_packet_handler(btproxy_sdp_packet_handler);
						l2cap_register_service_internal(connection, btproxy_svc_packet_handler, PSM_HID_INTERRUPT, 250, LEVEL_0);
						l2cap_register_service_internal(connection, btproxy_svc_packet_handler, PSM_HID_CONTROL, 250, LEVEL_0);
						dbg_printf(DBGMODE_TRACE, "HCI_STATE_WORKING, services registered\r\n");
						handled = 1;
					}
					else if (packet[2] == HCI_STATE_INITIALIZING) {
						dbg_printf(DBGMODE_TRACE, "BTSTACK_EVENT_STATE HCI_STATE_INITIALIZING\r\n");
						handled = 1;
					}
					break;
				case HCI_EVENT_COMMAND_COMPLETE:
					num_of_cmds = packet[2];
					cmd_opcode = READ_BT_16(packet, 3);
					cmd_status = packet[5];
					if (COMMAND_COMPLETE_EVENT(packet, hci_read_bd_addr)){
						dbg_printf(DBGMODE_TRACE, "HCI_EVENT_COMMAND_COMPLETE, BDADDR: %s\r\n", print_bdaddr(&packet[6]));
						handled = 1;
						break;
					}
					if (COMMAND_COMPLETE_EVENT(packet, hci_read_buffer_size)){
						dbg_printf(DBGMODE_TRACE, "HCI_EVENT_COMMAND_COMPLETE, Read Buffer Size: %d\r\n", hci_max_acl_data_packet_length());
						handled = 1;
						break;
					}
					if (COMMAND_COMPLETE_EVENT(packet, hci_read_local_supported_features)){
						dbg_printf(DBGMODE_TRACE, "HCI_EVENT_COMMAND_COMPLETE, Local Supported Features: %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
								packet[6], packet[7], packet[8], packet[9],
								packet[10], packet[11], packet[12], packet[13]
						);
						handled = 1;
						break;
					}
					if (COMMAND_COMPLETE_EVENT(packet, hci_reset)){
						handled = 1;
						break;
					}
					if (COMMAND_COMPLETE_EVENT(packet, hci_read_local_supported_commands)){
						dbg_printf(DBGMODE_TRACE, "HCI_EVENT_COMMAND_COMPLETE, Local Supported Commands:");
						for (int i = 0; i < 64; i++)
							dbg_printf(DBGMODE_TRACE, " %02X", packet[6 + i]);
						dbg_printf(DBGMODE_TRACE, " \r\n");
						handled = 1;
					}
					if (COMMAND_COMPLETE_EVENT(packet, hci_read_local_version_information)){
						dbg_printf(DBGMODE_TRACE, "HCI_EVENT_COMMAND_COMPLETE, Local Version Info: status 0x%02X", cmd_status);
						dbg_printf(DBGMODE_TRACE, ", HCI version 0x%02X ", packet[5]);
						dbg_printf(DBGMODE_TRACE, ", HCI revision 0x%04X ", READ_BT_16(packet, 6));
						dbg_printf(DBGMODE_TRACE, ", LMP/PAL version 0x%02X ", packet[8]);
						dbg_printf(DBGMODE_TRACE, ", MFG 0x%04X ", READ_BT_16(packet, 9));
						dbg_printf(DBGMODE_TRACE, ", LMP/PAL subversion 0x%04X ", READ_BT_16(packet, 11));
						dbg_printf(DBGMODE_TRACE, "\r\n");
						handled = 1;
					}
					if (btproxy_init_statemachine > 0 && (btproxy_init_statemachine % 2) == 1) {
						// is initializing and is waiting
						btproxy_init_statemachine++;
						handled = 1;
					}
					break;
				case HCI_EVENT_COMMAND_STATUS:
					cmd_status = packet[2];
					num_of_cmds = packet[3];
					cmd_opcode = READ_BT_16(packet, 4);
					if (cmd_status != 0x00) {
						dbg_printf(DBGMODE_ERR, "BT cmd status failed, cmd 0x%04X status 0x%02X, btproxy_init_statemachine %d\r\n", cmd_opcode, cmd_status, btproxy_init_statemachine);
					}
					if (btproxy_init_statemachine > 0 && (btproxy_init_statemachine % 2) == 1) {
						// is initializing and is waiting
						btproxy_init_statemachine++;
						handled = 1;
					}
					break;
				case BTSTACK_EVENT_DISCOVERABLE_ENABLED:
				case L2CAP_EVENT_SERVICE_REGISTERED:
					handled = 1;
					break;
			}
			break;
		case HCI_ACL_DATA_PACKET:
			break;
	}

	if (handled == 0)
	{
		dbg_printf(DBGMODE_TRACE, "btproxy_packet_handler, unhandled, 0x%02X, 0x%04X, %d\r\n", packet_type, channel, size);
		for (int i = 0; i < size; i++) {
			dbg_printf(DBGMODE_TRACE, "0x%02X ", packet[i]);
		}
		dbg_printf(DBGMODE_TRACE, "\r\n");
	}
}

CMD_ACTION_t btproxy_init_cmd(void * obj, uint8_t * hci_cmd_buffer)
{
	CMD_ACTION_t act = NO_PACKET_GET_NEXT_CMD;
	hci_stack_t* hcistack = (hci_stack_t*)obj;
	if (btproxy_init_statemachine % 2) return NO_PACKET_GET_NEXT_CMD; // busy

	bd_addr_t emptyAddr; // placeholder for clearing link keys

	if (btproxy_init_statemachine == 0) {
		dbg_printf(DBGMODE_TRACE, "btproxy_init_cmd first command\r\n");
	}
	else {
		//dbg_printf(DBGMODE_DEBUG, "btproxy_init_cmd state %d\r\n", btproxy_init_statemachine);
	}

	switch(btproxy_init_statemachine >> 1)
	{
		case 0:
			hci_send_cmd(&hci_read_local_version_information);
			break;
		case 1:
			hci_send_cmd(&hci_read_local_supported_commands);
			break;
		case 2:
			hci_send_cmd(&hci_set_event_filter, 0x00);
			break;
		case 3:
			hci_send_cmd(&hci_set_event_mask, 0xFFFFFFFF, 0x1FFFFFFF);
			break;
		case 4:
			hci_send_cmd(&hci_write_class_of_device, hci_stack->class_of_device);
			break;
		case 5:
			hci_send_cmd(&hci_write_local_name, hci_stack->local_name);
			break;
		case 6:
			hci_send_cmd(&hci_write_extended_inquiry_response, 0x01, bt_extended_inquiry_response);
			// this might be unsupported, the command complete will have an error, ignore the error
			break;
		case 7:
			// delete all stored link keys
			memset(&emptyAddr, 0, sizeof(bd_addr_t));
			hci_send_cmd(&hci_delete_stored_link_key, &emptyAddr, 0x01);
			break;
		case 8:
			hci_send_cmd(&hci_write_inquiry_mode, 0x02); // results with RSSI or extended results
			break;
		case 9:
			hci_send_cmd(&hci_write_default_link_policy_settings, 0x0005); // master-slave switch and sniff enabled
			break;
		case 10:
			hci_send_cmd(&hci_write_page_timeout, 0x5DC0);
			break;
		case 11:
			hci_send_cmd(&hci_write_connection_accept_timeout, 0xB540);
			break;
		case 12:
			if (hci_local_ssp_activated()){
				hci_send_cmd(&hci_write_simple_pairing_mode, hci_stack->ssp_enable);
				break;
			}
			else {
				dbg_printf(DBGMODE_ERR, "BT SSP is not supported by chip\r\n");

				#ifdef FORCE_SSP
				hci_send_cmd(&hci_write_simple_pairing_mode, 1);
				// note: if this is unsupported, error is returned but statemachine continues
				break;
				#else
				btproxy_init_statemachine += 2; // skip to next state
				return NO_PACKET_GET_NEXT_CMD;
				#endif
			}
		case 13:
			hci_send_cmd(&hci_write_inquiry_scan_activity, 0x1000, 0x0012);
			break;
		case 14:
			hci_send_cmd(&hci_write_inquiry_scan_type, 0x01); // interlaced scan
			break;
		case 15:
			hci_send_cmd(&hci_write_page_scan_activity, 0x0168, 0x0012);
			break;
		case 16:
			hci_send_cmd(&hci_write_page_scan_type, 0x01); // interlaced scan
			break;
		case 17:
			//hci_send_cmd(&hci_write_default_erroneous_data_reporting, 0x00); // disabled
			//break;
			btproxy_init_statemachine += 2; // skip to next state
			return NO_PACKET_GET_NEXT_CMD;
		case 18:
			hcistack->connectable = 1;
			hcistack->discoverable = 1;
			hci_send_cmd(&hci_write_scan_enable, (hcistack->connectable << 1) | hcistack->discoverable);
			break;
		default:
			btproxy_init_statemachine = -1; // finished
			dbg_printf(DBGMODE_TRACE, "btproxy_init_cmd last command\r\n");
			return NO_PACKET_SKIP_INIT;
	}
	btproxy_init_statemachine++; // make odd to indicate waiting
	return SENT_PACKET_GET_NEXT_CMD;
}

int btproxy_baudrate_cmd(void * config, uint32_t baudrate, uint8_t *hci_cmd_buffer)
{
	dbg_printf(DBGMODE_TRACE, "btproxy_baudrate_cmd %d\r\n", baudrate);

	// mfg specific command OGF and OCF
	hci_cmd_buffer[0] = 0x00;
	hci_cmd_buffer[1] = 0xFC;

	// index 2 is length

	int i = 3;
	hci_cmd_buffer[i++] = 2; // channel = 2 means BCCMD

	// type = SETREQ
	hci_cmd_buffer[i++] = 0x02;
	hci_cmd_buffer[i++] = 0x00;

	int j = i;

	// seq num
	hci_cmd_buffer[i++] = btproxy_bccmd_seq & 0xFF;
	hci_cmd_buffer[i++] = (btproxy_bccmd_seq >> 8) & 0xFF;
	btproxy_bccmd_seq++;

	// varid = config_uart
	hci_cmd_buffer[i++] = 0x02;
	hci_cmd_buffer[i++] = 0x68;

	// status = OK
	hci_cmd_buffer[i++] = 0x00;
	hci_cmd_buffer[i++] = 0x00;

	int k = i;

	int baudSetting;
	double divider = 244.140625; // see BCCMD Config_UART documentation
	double baudDbl = baudrate;
	baudDbl /= divider;
	baudSetting = (int)lround(baudDbl);

	// note, 3Mbit/s is baudSetting = 0x3000
	// TODO: stop bits and parity setting

	hci_cmd_buffer[i++] = baudSetting & 0xFF;
	hci_cmd_buffer[i++] = (baudSetting >> 8) & 0xFF;

	// three more 16 bit PDUs, blanks
	hci_cmd_buffer[i++] = 0;
	hci_cmd_buffer[i++] = 0;
	hci_cmd_buffer[i++] = 0;
	hci_cmd_buffer[i++] = 0;
	hci_cmd_buffer[i++] = 0;
	hci_cmd_buffer[i++] = 0;

	// set the PDU length
	int m = (i - 2) / 2;
	hci_cmd_buffer[j] = m & 0xFF;
	hci_cmd_buffer[j + 1] = (m >> 8) & 0xFF;
	i += 2;
	hci_cmd_buffer[2] = i;
}

void btproxy_hw_error()
{
}

void btproxy_db_open() { }
void btproxy_db_close() { }

int btproxy_db_get_link_key(bd_addr_t *bd_addr, link_key_t *link_key, link_key_type_t * type) {
	return 0;
}

void btproxy_db_put_link_key(bd_addr_t *bd_addr, link_key_t *key, link_key_type_t type) {

}

void btproxy_db_delete_link_key(bd_addr_t *bd_addr) {

}

int btproxy_db_get_name(bd_addr_t *bd_addr, device_name_t *device_name) {
	return 0;
}

void btproxy_db_put_name(bd_addr_t *bd_addr, device_name_t *device_name) {

}

void btproxy_db_delete_name(bd_addr_t *bd_addr) {

}

uint8_t btproxy_db_persistent_rfcomm_channel(char *servicename) {

}

const remote_device_db_t btproxy_db_memory = {
	btproxy_db_open,
	btproxy_db_close,
	btproxy_db_get_link_key,
	btproxy_db_put_link_key,
	btproxy_db_delete_link_key,
	btproxy_db_get_name,
	btproxy_db_put_name,
	btproxy_db_delete_name,
	btproxy_db_persistent_rfcomm_channel
};
