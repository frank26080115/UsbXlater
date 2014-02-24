#include "ds4_emu.h"
#include <crc.h>
#include <ringbuffer.h>
#include <btstack/btproxy.h>
#include <btstack/src/l2cap.h>
#include <btstack/src/hci.h>
#include <btstack/src/sdp.h>
#include <usbh_lib/usbh_core.h>
#include <usbh_lib/usbh_def.h>
#include <usbotg_lib/usb_core.h>
#include <usbd_lib/usbd_core.h>
#include <usbd_lib/usbd_def.h>
#include <usbh_dev/usbh_dev_dualshock.h>
#include <usbh_dev/usbh_dev_hid.h>
#include <usbd_dev/usbd_dev_ds4.h>
#include <usbd_dev/usbd_dev_ds3.h>
#include <usbd_dev/usbd_dev_cdc.h>
#include <stdint.h>
#include <flashfile.h>

extern USB_OTG_CORE_HANDLE		USB_OTG_Core_dev;
extern USB_OTG_CORE_HANDLE		USB_OTG_Core_host;
extern USBH_DEV					USBH_Dev;

uint8_t ds4_bdaddr[BD_ADDR_LEN];
volatile uint8_t ps4_bdaddr[BD_ADDR_LEN];
volatile uint8_t ps4_link_key[LINK_KEY_LEN];
uint8_t ds4_report02[DS4_REPORT_02_LEN];
uint8_t ds4_reportA3[DS4_REPORT_A3_LEN];

uint8_t ds4_btHidReport[256 + 128];
uint16_t ds4_btHidReport_size;

volatile EMU_State_t DS4EMU_State;
volatile USBD_Host_t EMU_USBD_Host;
EMU_USBD_Mode_t EMU_USBD_Mode_Current;
EMU_USBD_Mode_t EMU_USBD_Mode_Wanted;
char USBD_Needs_Switch;
uint32_t USBD_Switch_Timer;
uint32_t USBD_PS_Check_Timer;

ptr_ringbuffer_t ds2ps_hidintr_queue;
ptr_ringbuffer_t ps2ds_hidintr_queue;
ptr_ringbuffer_t ds2ps_hidctrl_queue;
ptr_ringbuffer_t ps2ds_hidctrl_queue;

uint16_t bt2ps_hidintr_channel = 0;
uint16_t bt2ps_hidctrl_channel = 0;
uint16_t bt2ds_hidintr_channel = 0;
uint16_t bt2ds_hidctrl_channel = 0;
uint16_t bt2ps_handle = 0;
uint16_t bt2ds_handle = 0;

volatile Proxy_Statistics_t proxy_stats;

service_record_item_t * btsdp_cur_sri = 0;

void ds4emu_crc32_calc_append(uint8_t* data, uint16_t len);
void ds4emu_reroute(ptr_ringbuffer_t* queue, uint16_t local_cid);

void ds4emu_init()
{
	EMU_USBD_Mode_Current = EMUUSBDMODE_NONE;
	EMU_USBD_Mode_Wanted = EMUUSBDMODE_NONE;
	EMU_USBD_Host = USBDHOST_NONE;
	USBD_Needs_Switch = 0;
	bt_extended_inquiry_response = ds4_extended_inquiry_response;

	proxy_stats.alloc_mem = freeRam();
	ptr_ringbuffer_init(&ds2ps_hidintr_queue, 8, 512 + 64);
	ptr_ringbuffer_init(&ps2ds_hidintr_queue, 8, 512 + 64);
	ptr_ringbuffer_init(&ds2ps_hidctrl_queue, 4, 128);
	ptr_ringbuffer_init(&ps2ds_hidctrl_queue, 4, 128);
	proxy_stats.alloc_mem -= freeRam();
}

void ds4emu_task()
{
	if (bt2ps_handle == 0 && bt2ds_handle != 0 && bt2ds_hidintr_channel != 0 && bt2ds_hidctrl_channel != 0
			&& (DS4EMU_State & EMUSTATE_HAS_PS4_BDADDR) != 0 && (DS4EMU_State & EMUSTATE_PENDING_PS4_BT_CONN) == 0
			&& hci_can_send_packet_now(HCI_COMMAND_DATA_PACKET))
	{
		int8_t v;
		if ((v = ds4emu_isPs4ConnValid()) == 1)
		{
			DS4EMU_State |= EMUSTATE_PENDING_PS4_BT_CONN;
			//hci_send_cmd(&hci_create_connection, ps4_bdaddr, hci_usable_acl_packet_types(), 0, 0, 0, 1); // this one should only be used if SDP is needed

			dbg_printf(DBGMODE_TRACE, "Attempting BT connection with PlayStation %s\r\n", print_bdaddr(ps4_bdaddr));
			// l2cap_create_channel_internal should trigger an entire chain of BTstack actions to create the connection, thus we avoid using hci_send_cmd
			l2cap_create_channel_internal(NULL, ds4emu_sub_handler, ps4_bdaddr, PSM_HID_CONTROL, l2cap_max_mtu());
			// once the CTRL channel is established, then the second INTR channel will be created in the packet handler
		}
	}

	ds4emu_reroute(&ds2ps_hidintr_queue, bt2ps_hidintr_channel);
	ds4emu_reroute(&ds2ps_hidctrl_queue, bt2ps_hidctrl_channel);
	ds4emu_reroute(&ps2ds_hidintr_queue, bt2ds_hidintr_channel);
	ds4emu_reroute(&ps2ds_hidctrl_queue, bt2ds_hidctrl_channel);

	if (USBD_Dev_DS4_NeedsWriteFlash != 0 && (DS4EMU_State & EMUSTATE_HAS_BT_BDADDR) != 0) {
		dbg_printf(DBGMODE_TRACE, "PS4 BDADDR and link key obtained %s", print_bdaddr(ps4_bdaddr));
		dbg_printf(DBGMODE_TRACE, ", %s\r\n", print_linkkey(ps4_link_key));
		flashfile_updateEntry(ffsys.nvm_file->d.fmt.bt_bdaddr, hci_local_bd_addr(), BD_ADDR_LEN, 0);
		flashfile_updateEntry(ffsys.nvm_file->d.fmt.ps4_bdaddr, ps4_bdaddr, BD_ADDR_LEN, 0);
		flashfile_updateEntry(ffsys.nvm_file->d.fmt.ps4_link_key, ps4_link_key, LINK_KEY_LEN, 0);
		nvm_file_t* cachedFile = (nvm_file_t*)ffsys.cache;
		uint32_t ps4_conn_crc = crc32_calc(cachedFile->d.fmt.bt_bdaddr, BD_ADDR_LEN + BD_ADDR_LEN + LINK_KEY_LEN);
		flashfile_updateEntry(&(ffsys.nvm_file->d.fmt.ps4_conn_crc), &ps4_conn_crc, sizeof(uint32_t), 1);
		USBD_Dev_DS4_NeedsWriteFlash = 0;
	}

	static USBD_Device_cb_TypeDef* cb = 0;

	if (EMU_USBD_Mode_Wanted != EMU_USBD_Mode_Current)
	{
		if ((systick_1ms_cnt - USBD_Switch_Timer) > 100 && USBD_Needs_Switch != 0)
		{
			switch (EMU_USBD_Mode_Wanted)
			{
				case EMUUSBDMODE_DS4:
					cb = &USBD_Dev_DS4_cb;
					break;
				case EMUUSBDMODE_VCP:
					cb = &USBD_Dev_CDC_cb;
					break;
				case EMUUSBDMODE_DS3:
					cb = &USBD_Dev_DS3_cb;
					break;
				case EMUUSBDMODE_KBM:
					cb = 0;
					break;
				default:
					cb = 0;
					break;
			}

			if (cb != 0)
			{
				dbg_printf(DBGMODE_TRACE, "USBD connecting as mode %d\r\n", EMU_USBD_Mode_Wanted);
				USBD_Init(&USB_OTG_Core_dev, USB_OTG_HS_CORE_ID, cb);
				DCD_DevConnect(&USB_OTG_Core_dev);
				EMU_USBD_Mode_Current = EMU_USBD_Mode_Wanted;

				if (EMU_USBD_Mode_Current == EMUUSBDMODE_DS4 || EMU_USBD_Mode_Current == EMUUSBDMODE_DS3) {
					USBD_PS_Check_Timer = systick_1ms_cnt;
				}
			}
			USBD_Needs_Switch = 0;
		}
		else if (USBD_Needs_Switch == 0)
		{
			USBD_Needs_Switch = 1;
			USBD_Switch_Timer = systick_1ms_cnt;
			if (cb != 0)
			{
				DCD_DevDisconnect(&USB_OTG_Core_dev);
				USBD_DeInit(&USB_OTG_Core_dev);
			}
			EMU_USBD_Mode_Current = EMUUSBDMODE_NONE;
			dbg_printf(DBGMODE_TRACE, "USBD disconnected, switching from mode %d to mode %d\r\n", EMU_USBD_Mode_Current, EMU_USBD_Mode_Wanted);
		}
	}

	if (EMU_USBD_Mode_Wanted != EMUUSBDMODE_DS4 && (DS4EMU_State & EMUSTATE_HAS_BT_BDADDR) != 0 && ((DS4EMU_State & EMUSTATE_HAS_PS4_BDADDR) == 0 || EMU_USBD_Mode_Wanted == EMUUSBDMODE_NONE) && (DS4EMU_State & EMUSTATE_HAS_MFG_DATE) != 0)
	{
		EMU_USBD_Mode_Wanted = EMUUSBDMODE_DS4;
		dbg_printf(DBGMODE_TRACE, "USBD switching to DS4 mode\r\n");
	}

	return;

	// if in 5 seconds, the host doesn't behave as if it is a playstation
	// then we assume it's a computer
	if ((EMU_USBD_Mode_Current == EMUUSBDMODE_DS4 || EMU_USBD_Mode_Current == EMUUSBDMODE_DS3)
		&& (systick_1ms_cnt - USBD_PS_Check_Timer) > 5000)
	{
		if (EMU_USBD_Host == USBDHOST_UNKNOWN)
		{
			if (EMU_USBD_Mode_Wanted != EMUUSBDMODE_VCP) {
				dbg_printf(DBGMODE_TRACE, "USBD should become VCP because of unknown host\r\n");
			}
			EMU_USBD_Mode_Wanted = EMUUSBDMODE_VCP;
		}
	}

	// we have obtained the playstation's bdaddr, so now we can become a keyboard+mouse for chat
	if (EMU_USBD_Mode_Current == EMUUSBDMODE_DS4 && (DS4EMU_State & EMUSTATE_HAS_PS4_BDADDR) != 0 && USBH_Dev_HID_Cnt > 0)
	{
		if (EMU_USBD_Mode_Wanted != EMUUSBDMODE_KBM) {
			dbg_printf(DBGMODE_TRACE, "USBD should become KB+M\r\n");
		}
		EMU_USBD_Mode_Wanted = EMUUSBDMODE_KBM;
	}

	// no connected HID devices means it needs to be in VCP mode to communicate
	if (USBH_Dev_HID_Cnt == 0 && systick_1ms_cnt > 5000)
	{
		if (EMU_USBD_Mode_Wanted != EMUUSBDMODE_VCP) {
			dbg_printf(DBGMODE_TRACE, "USBD should become VCP because no HID is connected\r\n");
		}
		EMU_USBD_Mode_Wanted = EMUUSBDMODE_VCP;
	}
}

void ds4emu_loadState()
{
	char has = 1;
	if (ffsys.nvm_file->d.fmt.ds4_vid == 0x0000 || ffsys.nvm_file->d.fmt.ds4_vid == 0xFFFF
		|| ffsys.nvm_file->d.fmt.ds4_pid == 0x0000 || ffsys.nvm_file->d.fmt.ds4_pid == 0xFFFF)
	{
		has = 0;
	}
	if (is_array_valid(ffsys.nvm_file->d.fmt.ds4_bdaddr, BD_ADDR_LEN) == 0) {
		has = 0;
	}
	if (is_array_valid(ffsys.nvm_file->d.fmt.report_A3, DS4_REPORT_A3_LEN) == 0) {
		has = 0;
	}
	if (is_array_valid(ffsys.nvm_file->d.fmt.report_02, DS4_REPORT_02_LEN) == 0) {
		has = 0;
	}
	if (has != 0) {
		memcpy(ds4_bdaddr, ffsys.nvm_file->d.fmt.ds4_bdaddr, BD_ADDR_LEN);
		memcpy(ds4_reportA3, ffsys.nvm_file->d.fmt.report_A3, DS4_REPORT_A3_LEN);
		memcpy(ds4_report02, ffsys.nvm_file->d.fmt.report_02, DS4_REPORT_02_LEN);
		DS4EMU_State |= EMUSTATE_HAS_DS4_BDADDR;
		DS4EMU_State |= EMUSTATE_HAS_MFG_DATE;
		DS4EMU_State |= EMUSTATE_HAS_REPORT_02;
		dbg_printf(DBGMODE_TRACE, "Preloaded DualShock data\r\n");
	}

	has = 1;
	if (is_array_valid(ffsys.nvm_file->d.fmt.ps4_bdaddr, BD_ADDR_LEN) == 0) {
		has = 0;
	}
	if (is_array_valid(ffsys.nvm_file->d.fmt.ps4_link_key, LINK_KEY_LEN) == 0) {
		has = 0;
	}
	if (has != 0) {
		memcpy(ps4_bdaddr, ffsys.nvm_file->d.fmt.ps4_bdaddr, BD_ADDR_LEN);
		memcpy(ps4_link_key, ffsys.nvm_file->d.fmt.ps4_link_key, LINK_KEY_LEN);
		DS4EMU_State |= EMUSTATE_HAS_PS4_BDADDR;
		dbg_printf(DBGMODE_TRACE, "Preloaded PlayStation data\r\n");
	}
}

int8_t ds4emu_isPs4ConnValid()
{
	if ((DS4EMU_State & EMUSTATE_HAS_BT_BDADDR) == 0) {
		return -1;
	}
	if (is_array_valid(ffsys.nvm_file->d.fmt.bt_bdaddr, BD_ADDR_LEN) == 0) {
		return -2;
	}
	if (is_array_valid(ffsys.nvm_file->d.fmt.ps4_bdaddr, BD_ADDR_LEN) == 0) {
		return -3;
	}
	if (is_array_valid(ffsys.nvm_file->d.fmt.ps4_link_key, LINK_KEY_LEN) == 0) {
		return -4;
	}
	uint32_t ps4_conn_crc = crc32_calc(ffsys.nvm_file->d.fmt.bt_bdaddr, BD_ADDR_LEN + BD_ADDR_LEN + LINK_KEY_LEN);
	if (ps4_conn_crc == ffsys.nvm_file->d.fmt.ps4_conn_crc) {
		return 1;
	}
	else {
		return -5;
	}
}

void btproxy_sdp_loadService(uint16_t conn_handle)
{
	if (btsdp_cur_sri != 0) {
		sdp_unregister_service_internal(btsdp_cur_sri->connection, btsdp_cur_sri->service_record_handle);
		free(btsdp_cur_sri);
	}
	if (conn_handle == 0)
		return;
	btsdp_cur_sri = (service_record_item_t*)calloc(1, sizeof(service_record_item_t));
	if (btsdp_cur_sri == 0) {
		dbg_printf(DBGMODE_ERR, "error btproxy_sdp_loadService no memory available\r\n");
		return;
	}
	if (conn_handle == bt2ds_handle) {
		btsdp_cur_sri->service_record = ps4_sdp_service_search_attribute_response;
	}
	else if (conn_handle == bt2ps_handle) {
		btsdp_cur_sri->service_record = ds4_sdp_service_search_attribute_response;
	}
	else {
		dbg_printf(DBGMODE_ERR, "error btproxy_sdp_loadService unknown handle\r\n");
		return;
	}
	dbg_printf(DBGMODE_TRACE, "SDP registering service for handle 0x%04X\r\n", conn_handle);
	sdp_register_service_internal(btsdp_cur_sri->connection, btsdp_cur_sri);
}

void btproxy_sdp_early_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
	SDP_PDU_ID_t pdu_id;
	uint16_t* conn_handle_ptr = (uint16_t*)&packet[-COMPLETE_L2CAP_HEADER];
	*conn_handle_ptr &= 0x0FFF;
	//dbg_printf(DBGMODE_TRACE, "btproxy_sdp_early_packet_handler conn_handle 0x%04X chan 0x%04X pdu %d\r\n", *conn_handle_ptr, channel, packet[0]);

	switch (packet_type)
	{
		case L2CAP_DATA_PACKET:
			pdu_id = (SDP_PDU_ID_t) packet[0];
			if (pdu_id == SDP_ServiceSearchRequest || pdu_id == SDP_ServiceAttributeRequest || pdu_id == SDP_ServiceSearchAttributeRequest) {
				btproxy_sdp_loadService(*conn_handle_ptr);
			}
			break;
		case HCI_EVENT_PACKET:
			if (packet[0] == L2CAP_EVENT_CHANNEL_CLOSED) {
				dbg_printf(DBGMODE_TRACE, "L2CAP_EVENT_CHANNEL_CLOSED on SDP\r\n");
			}
			break;
	}
}

void ds4emu_sub_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
	btproxy_sub_handler(NULL, packet_type, channel, packet, size);
}

char btproxy_sub_handler(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
	uint16_t dest_channel = 0;
	uint16_t conn_handle = 0;
	uint16_t psm = 0;
	char ret = 0;
	switch (packet_type)
	{
		case HCI_EVENT_PACKET:
			switch (packet[0])
			{
				case BTSTACK_EVENT_STATE:
					if (packet[2] == HCI_STATE_WORKING)
					{
						l2cap_register_service_internal(connection, ds4emu_sub_handler, PSM_HID_CONTROL, l2cap_max_mtu(), LEVEL_0);
						l2cap_register_service_internal(connection, ds4emu_sub_handler, PSM_HID_INTERRUPT, l2cap_max_mtu(), LEVEL_0);
						DS4EMU_State |= EMUSTATE_HAS_BT_BDADDR;
						bt2ps_hidintr_channel = 0;
						bt2ps_hidctrl_channel = 0;
						bt2ds_hidintr_channel = 0;
						bt2ds_hidctrl_channel = 0;
						bt2ps_handle = 0;
						bt2ds_handle = 0;
						dbg_printf(DBGMODE_TRACE, "DS4 BT engine initialized\r\n");
					}
					break;
				case HCI_EVENT_CONNECTION_COMPLETE:
					conn_handle = READ_BT_16(packet, 3);
					if (memcmp(&packet[5], ds4_bdaddr, BD_ADDR_LEN) == 0) {
						bt2ds_handle = conn_handle;
						dbg_printf(DBGMODE_DEBUG, "DS4 connected via BT, conn_handle 0x%04X\r\n", conn_handle);
					}
					if (memcmp(&packet[5], ps4_bdaddr, BD_ADDR_LEN) == 0) {
						bt2ps_handle = conn_handle;
						DS4EMU_State &= ~EMUSTATE_PENDING_PS4_BT_CONN;
						dbg_printf(DBGMODE_DEBUG, "PS4 connected via BT, conn_handle 0x%04X\r\n", conn_handle);
					}
					ret = 1;
					break;
				case HCI_EVENT_DISCONNECTION_COMPLETE:
					conn_handle = READ_BT_16(packet, 3);
					if (conn_handle == bt2ds_handle) {
						bt2ds_handle = 0;
						bt2ds_hidctrl_channel = 0;
						bt2ds_hidintr_channel = 0;
						DS4EMU_State &= ~(EMUSTATE_IS_DS4_BT_CONNECTED | EMUSTATE_GIVEN_DS4_1401 | EMUSTATE_GIVEN_DS4_1402);
						dbg_printf(DBGMODE_DEBUG, "DS4 disconnected via BT, conn_handle 0x%04X\r\n", conn_handle);
					}
					else if (conn_handle == bt2ps_handle) {
						bt2ps_handle = 0;
						bt2ps_hidctrl_channel = 0;
						bt2ps_hidintr_channel = 0;
						DS4EMU_State &= ~EMUSTATE_PENDING_PS4_BT_CONN;
						DS4EMU_State &= ~EMUSTATE_IS_PS4_BT_CONNECTED;
						dbg_printf(DBGMODE_DEBUG, "PS4 disconnected via BT, conn_handle 0x%04X\r\n", conn_handle);
					}
					ret = 1;
					break;
				case L2CAP_EVENT_CHANNEL_OPENED:
					if (packet[2] == 0) // status
					{
						psm = READ_BT_16(packet, 11);
						conn_handle = READ_BT_16(packet, 9);
						if (channel == 0) {
							channel = READ_BT_16(packet, 13); // local CID
						}
						if (psm == PSM_SDP)
						{
							if (conn_handle == bt2ds_handle) {
								dbg_printf(DBGMODE_DEBUG, "L2CAP_EVENT_CHANNEL_OPENED SDP for DualShock 0x%04X\r\n", channel);
								ret = 1;
							}
							else if (conn_handle == bt2ps_handle) {
								dbg_printf(DBGMODE_DEBUG, "L2CAP_EVENT_CHANNEL_OPENED SDP for PlayStation 0x%04X\r\n", channel);
								ret = 1;
							}
						}
						else if (psm == PSM_HID_INTERRUPT)
						{
							if (conn_handle == bt2ds_handle) {
								bt2ds_hidintr_channel = channel;
								dbg_printf(DBGMODE_DEBUG, "L2CAP_EVENT_CHANNEL_OPENED HID-INTR for DualShock 0x%04X\r\n", channel);
								ret = 1;
							}
							else if (conn_handle == bt2ps_handle) {
								bt2ps_hidintr_channel = channel;
								dbg_printf(DBGMODE_DEBUG, "L2CAP_EVENT_CHANNEL_OPENED HID-INTR for PlayStation 0x%04X\r\n", channel);
								ret = 1;
							}
						}
						else if (psm == PSM_HID_CONTROL)
						{
							if (conn_handle == bt2ds_handle) {
								bt2ds_hidctrl_channel = channel;
								dbg_printf(DBGMODE_DEBUG, "L2CAP_EVENT_CHANNEL_OPENED HID-CTRL for DualShock 0x%04X\r\n", channel);
								ret = 1;
							}
							else if (conn_handle == bt2ps_handle) {
								bt2ps_hidctrl_channel = channel;
								dbg_printf(DBGMODE_DEBUG, "L2CAP_EVENT_CHANNEL_OPENED HID-CTRL for PlayStation 0x%04X\r\n", channel);
								l2cap_create_channel_internal(NULL, ds4emu_sub_handler, ps4_bdaddr, PSM_HID_INTERRUPT, l2cap_max_mtu());
								ret = 1;
							}
						}
					}
					else
					{
						if (BD_ADDR_CMP(&packet[3], ps4_bdaddr) == 0) {
							dbg_printf(DBGMODE_ERR, "L2CAP_EVENT_CHANNEL_OPENED failed for PlayStation with bad status 0x%02X\r\n", packet[2]);
							DS4EMU_State &= ~EMUSTATE_IS_PS4_BT_CONNECTED;
						}
						else if (BD_ADDR_CMP(&packet[3], ds4_bdaddr) == 0) {
							dbg_printf(DBGMODE_ERR, "L2CAP_EVENT_CHANNEL_OPENED failed for DualShock with bad status 0x%02X\r\n", packet[2]);
						}
						ret = 1;
					}
					break;
				case L2CAP_EVENT_CHANNEL_CLOSED:
					if (channel == 0) {
						channel = READ_BT_16(packet, 2); // local CID
					}
					else if (channel == bt2ds_hidintr_channel) {
						bt2ds_hidintr_channel = 0;
					}
					else if (channel == bt2ps_hidintr_channel) {
						bt2ps_hidintr_channel = 0;
					}
					else if (channel == bt2ds_hidctrl_channel) {
						bt2ds_hidctrl_channel = 0;
					}
					else if (channel == bt2ps_hidctrl_channel) {
						bt2ps_hidctrl_channel = 0;
					}
					if (channel != 0) {
						dbg_printf(DBGMODE_DEBUG, "L2CAP_EVENT_CHANNEL_CLOSED 0x%04X\r\n", channel);
						ret = 1;
					}
					break;
				case L2CAP_EVENT_INCOMING_CONNECTION: // this event isn't routed to btproxy.c at all
					if (channel == 0) {
						channel = READ_BT_16(packet, 12);
					}
					dbg_printf(DBGMODE_DEBUG, "L2CAP_EVENT_INCOMING_CONNECTION 0x%04X\r\n", channel);
					l2cap_accept_connection_internal(channel);
					break;
			}
			break;
		case L2CAP_DATA_PACKET:
			if (channel == bt2ps_hidintr_channel) {
				proxy_stats.ps2bt_hidintr_cnt++;
				if (bt2ds_hidintr_channel != 0) {
					ptr_ringbuffer_push(&ps2ds_hidintr_queue, packet, size);
				}
				ret = 1;
			}
			else if (channel == bt2ps_hidctrl_channel) {
				proxy_stats.ps2bt_hidctrl_cnt++;
				if (bt2ds_hidctrl_channel != 0) {
					ptr_ringbuffer_push(&ps2ds_hidctrl_queue, packet, size);
				}
				ret = 1;
			}
			else if (channel == bt2ds_hidintr_channel) {
				proxy_stats.ds2bt_hidintr_cnt++;
				if (bt2ps_hidintr_channel != 0) {
					if (packet[1] >= 0x11 && packet[1] <= 0x13 && USBH_Dev_HID_Cnt > 0) {

					}
					else {
						ptr_ringbuffer_push(&ds2ps_hidintr_queue, packet, size);
					}
				}
				ret = 1;
			}
			else if (channel == bt2ds_hidintr_channel) {
				proxy_stats.ds2bt_hidintr_cnt++;
				if (bt2ps_hidctrl_channel != 0) {
					ptr_ringbuffer_push(&ds2ps_hidctrl_queue, packet, size);
				}
				ret = 1;
			}
			break;
	}
	return ret;
}

void ds4emu_crc32_calc_append(uint8_t* data, uint16_t len)
{
	uint32_t* c = (uint32_t*)&data[len - 4];
	*c = crc32_calc(data, len - 4);
}

void ds4emu_reroute(ptr_ringbuffer_t* queue, uint16_t local_cid)
{
	if (ptr_ringbuffer_isempty(queue) == 0)
	{
		if (local_cid == 0) {
			ptr_ringbuffer_pop(queue, 0);
		}
		else if (l2cap_can_send_packet_now(local_cid) != 0) {
			uint8_t* outgoing = l2cap_get_outgoing_buffer();
			int length = ptr_ringbuffer_pop(queue, outgoing);
			if (length > 0)
			{
				if (local_cid == bt2ps_hidctrl_channel) {
					proxy_stats.bt2ps_hidctrl_cnt++;
				}
				else if (local_cid == bt2ds_hidctrl_channel) {
					proxy_stats.bt2ds_hidctrl_cnt++;
				}
				else if (local_cid == bt2ps_hidintr_channel) {
					proxy_stats.bt2ps_hidintr_cnt++;
				}
				else if (local_cid == bt2ds_hidintr_channel) {
					proxy_stats.bt2ds_hidintr_cnt++;
				}
				l2cap_send_prepared(local_cid, length);
			}
		}
	}
}

void ds4emu_report(uint8_t* data, uint16_t len)
{
	if (bt2ps_hidintr_channel != 0) {
		ds4emu_crc32_calc_append(data, len);
		ptr_ringbuffer_push(&ds2ps_hidintr_queue, data, len);
	}
}

uint32_t btproxy_provide_class_of_device()
{
	if ((DS4EMU_State & EMUSTATE_IS_DS4_BT_CONNECTED) == 0)
		return 0x00002508;

	return 0x002C0100; // PS4
}

char* btproxy_provide_name_of_device()
{
	return ds4_bt_device_name;
}

char btproxy_check_bdaddr(bd_addr_t addr)
{
	if ((DS4EMU_State & EMUSTATE_HAS_DS4_BDADDR) != 0 && memcmp(addr, ds4_bdaddr, BD_ADDR_LEN) == 0) {
		return 1;
	}
	if ((DS4EMU_State & EMUSTATE_HAS_PS4_BDADDR) != 0 && memcmp(addr, ps4_bdaddr, BD_ADDR_LEN) == 0) {
		return 1;
	}
	return 0;
}

void btproxy_db_open() { }
void btproxy_db_close() { }

int btproxy_db_get_link_key(bd_addr_t *bd_addr, link_key_t *link_key, link_key_type_t * type) {
	*type = UNAUTHENTICATED_COMBINATION_KEY_GENERATED_FROM_P192;
	if (memcmp(bd_addr, ps4_bdaddr, BD_ADDR_LEN) == 0 && ds4emu_isPs4ConnValid() == 1)
	{
		memcpy(link_key, ps4_link_key, LINK_KEY_LEN);
		return 1;
	}
	else if (memcmp(bd_addr, ds4_bdaddr, BD_ADDR_LEN) == 0)
	{
		memcpy(link_key, ds4_link_key, LINK_KEY_LEN);
		return 1;
	}
	return 0;
}

void btproxy_db_put_link_key(bd_addr_t *bd_addr, link_key_t *key, link_key_type_t type) {

}

void btproxy_db_delete_link_key(bd_addr_t *bd_addr) {

}

int btproxy_db_get_name(bd_addr_t *bd_addr, device_name_t *device_name) {
	// this is not really called
	return 0;
}

void btproxy_db_put_name(bd_addr_t *bd_addr, device_name_t *device_name) {

}

void btproxy_db_delete_name(bd_addr_t *bd_addr) {

}

uint8_t btproxy_db_persistent_rfcomm_channel(char *servicename) {

}
