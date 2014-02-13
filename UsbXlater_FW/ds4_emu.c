#include "ds4_emu.h"
#include <crc.h>
#include <ringbuffer.h>
#include <btstack/btproxy.h>
#include <btstack/src/l2cap.h>
#include <btstack/src/hci.h>
#include <usbh_lib/usbh_core.h>
#include <usbh_lib/usbh_def.h>
#include <usbotg_lib/usb_core.h>
#include <usbd_lib/usbd_core.h>
#include <usbd_lib/usbd_def.h>
#include <usbh_dev/usbh_dev_dualshock.h>
#include <usbh_dev/usbh_dev_hid.h>
#include <usbd_dev/usbd_dev_ds4.h>
#include <stdint.h>

extern USB_OTG_CORE_HANDLE		USB_OTG_Core_dev;
extern USB_OTG_CORE_HANDLE		USB_OTG_Core_host;
extern USBH_DEV					USBH_Dev;

uint8_t ds4_bdaddr[BD_ADDR_LEN];
uint8_t ps4_bdaddr[BD_ADDR_LEN];
uint8_t ds4_report02[DS4_REPORT_02_LEN];
uint8_t ds4_mfg_date[DS4_MFG_DATE_LEN];

volatile EMU_State_t DS4EMU_State;
USBD_Host_t     EMU_USBD_Host;
EMU_USBD_Mode_t EMU_USBD_Mode_Current;
EMU_USBD_Mode_t EMU_USBD_Mode_Wanted;
char USBD_Needs_Switch;
uint32_t USBD_Switch_Timer;
uint32_t USBD_PS_Check_Timer;

ptr_ringbuffer_t ds2ps_hidintr_queue;
ptr_ringbuffer_t ds2ps_hidintr_data_queue;
ptr_ringbuffer_t ps2ds_hidintr_queue;
ptr_ringbuffer_t ds2ps_hidctrl_queue;
ptr_ringbuffer_t ps2ds_hidctrl_queue;

uint16_t bt2ps_hidintr_channel;
uint16_t bt2ps_hidctrl_channel;
uint16_t ps2bt_hidintr_channel;
uint16_t ps2bt_hidctrl_channel;
uint16_t bt2ds_hidintr_channel;
uint16_t bt2ds_hidctrl_channel;
uint16_t ds2bt_hidintr_channel;
uint16_t ds2bt_hidctrl_channel;
uint16_t ds2bt_sdp_channel;
uint16_t ps2bt_sdp_channel;
uint16_t bt2ps_sdp_channel;
uint16_t bt2ds_sdp_channel;
uint16_t bt2ps_handle;
uint16_t bt2ds_handle;

void DS4_USBD_Init()
{
	EMU_USBD_Mode_Current = EMUUSBDMODE_NONE;
	EMU_USBD_Mode_Wanted = EMUUSBDMODE_NONE;
	EMU_USBD_Host = USBDHOST_NONE;
	USBD_Needs_Switch = 0;
}

void DS4_USBD_Task()
{
	if (EMU_USBD_Mode_Wanted != EMU_USBD_Mode_Current)
	{
		if ((systick_1ms_cnt - USBD_Switch_Timer) > 100 && USBD_Needs_Switch != 0)
		{
			USBD_Device_cb_TypeDef* cb = 0;
			// TODO: get cb according to wanted mode

			if (cb != 0)
			{
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
			DCD_DevDisconnect(&USB_OTG_Core_dev);
			USBD_DeInit(&USB_OTG_Core_dev);
			EMU_USBD_Mode_Current = EMUUSBDMODE_NONE;
		}
	}

	if (EMU_USBD_Mode_Wanted != EMUUSBDMODE_DS4 && (DS4EMU_State & EMUSTATE_HAS_PS4_BDADDR) == 0 && (DS4EMU_State & EMUSTATE_HAS_MFG_DATE) != 0)
	{
		EMU_USBD_Mode_Wanted = EMUUSBDMODE_DS4;
	}

	// if in 5 seconds, the host doesn't behave as if it is a playstation
	// then we assume it's a computer
	if ((EMU_USBD_Mode_Current == EMUUSBDMODE_DS4 || EMU_USBD_Mode_Current == EMUUSBDMODE_DS3)
		&& (systick_1ms_cnt - USBD_PS_Check_Timer) > 5000)
	{
		if (EMU_USBD_Host == USBDHOST_UNKNOWN) {
			EMU_USBD_Mode_Wanted = EMUUSBDMODE_VCP;
		}
	}

	// we have obtained the playstation's bdaddr, so now we can become a keyboard+mouse for chat
	if (EMU_USBD_Mode_Current == EMUUSBDMODE_DS4 && (DS4EMU_State & EMUSTATE_HAS_PS4_BDADDR) != 0 && USBH_Dev_HID_Cnt > 0)
	{
		EMU_USBD_Mode_Wanted = EMUUSBDMODE_KBM;
	}

	// no connected HID devices means it needs to be in VCP mode to communicate
	if (USBH_Dev_HID_Cnt == 0 && systick_1ms_cnt > 5000) {
		EMU_USBD_Mode_Wanted = EMUUSBDMODE_VCP;
	}
}

void btproxy_sdp_early_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
	switch (packet_type)
	{
		case HCI_EVENT_PACKET:
			switch (packet[0])
			{
				case L2CAP_EVENT_CHANNEL_OPENED:
					if (packet[2] == 0)
					{
					}
					else
					{
					}
					break;
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
	char ret = 0;
	switch (packet_type)
	{
		case HCI_EVENT_PACKET:
			switch (packet[0])
			{
				case BTSTACK_EVENT_STATE:
					if (packet[2] == HCI_STATE_WORKING)
					{
						l2cap_register_service_internal(connection, ds4emu_sub_handler, PSM_HID_INTERRUPT, 250, LEVEL_0);
						l2cap_register_service_internal(connection, ds4emu_sub_handler, PSM_HID_CONTROL, 250, LEVEL_0);
						DS4EMU_State |= EMUSTATE_HAS_BT_BDADDR;
						bt2ps_hidintr_channel = 0;
						bt2ps_hidctrl_channel = 0;
						ps2bt_hidintr_channel = 0;
						ps2bt_hidctrl_channel = 0;
						bt2ds_hidintr_channel = 0;
						bt2ds_hidctrl_channel = 0;
						ds2bt_hidintr_channel = 0;
						ds2bt_hidctrl_channel = 0;
						ds2bt_sdp_channel = 0;
						ps2bt_sdp_channel = 0;
						bt2ps_sdp_channel = 0;
						bt2ds_sdp_channel = 0;
						bt2ps_handle = 0;
						bt2ds_handle = 0;
						ptr_ringbuffer_init(&ds2ps_hidintr_queue, 8);
						ptr_ringbuffer_init(&ds2ps_hidintr_data_queue, 2);
						ptr_ringbuffer_init(&ps2ds_hidintr_queue, 8);
						ptr_ringbuffer_init(&ds2ps_hidctrl_queue, 8);
						ptr_ringbuffer_init(&ps2ds_hidctrl_queue, 8);
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
						dbg_printf(DBGMODE_DEBUG, "PS4 connected via BT, conn_handle 0x%04X\r\n", conn_handle);
					}
					ret = 1;
					break;
				case HCI_EVENT_DISCONNECTION_COMPLETE:
					conn_handle = READ_BT_16(packet, 3);
					if (conn_handle == bt2ds_handle) {
						bt2ds_handle = 0;
						DS4EMU_State &= ~(EMUSTATE_IS_DS4_BT_CONNECTED | EMUSTATE_GIVEN_DS4_1401 | EMUSTATE_GIVEN_DS4_1402);
						dbg_printf(DBGMODE_DEBUG, "DS4 disconnected via BT, conn_handle 0x%04X\r\n", conn_handle);
					}
					else if (conn_handle == bt2ps_handle) {
						bt2ps_handle = 0;
						DS4EMU_State &= ~EMUSTATE_IS_PS4_BT_CONNECTED;
						dbg_printf(DBGMODE_DEBUG, "PS4 disconnected via BT, conn_handle 0x%04X\r\n", conn_handle);
					}
					ret = 1;
			}
			break;
		case L2CAP_EVENT_CHANNEL_OPENED:
			break;
		case L2CAP_DATA_PACKET:
			if (channel == ps2bt_hidintr_channel && bt2ds_hidintr_channel != 0) {
				ptr_ringbuffer_push(&ps2ds_hidintr_queue, packet, size);
				ret = 1;
			}
			else if (channel == ps2bt_hidctrl_channel && bt2ds_hidctrl_channel != 0) {
				ptr_ringbuffer_push(&ps2ds_hidctrl_queue, packet, size);
				ret = 1;
			}
			else if (channel == ds2bt_hidintr_channel && bt2ps_hidintr_channel != 0) {
				if (packet[1] == 0x11 || packet[1] == 0x12 || packet[1] == 0x13) {
					ptr_ringbuffer_push(&ds2ps_hidintr_data_queue, packet, size);
				}
				else {
					ptr_ringbuffer_push(&ds2ps_hidintr_queue, packet, size);
				}
				ret = 1;
			}
			else if (channel == ds2bt_hidintr_channel && bt2ps_hidctrl_channel != 0) {
				ptr_ringbuffer_push(&ds2ps_hidctrl_queue, packet, size);
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

void ds4emu_reroute_channel(ptr_ringbuffer_t* queue, uint16_t local_cid)
{
	if (ptr_ringbuffer_isempty(queue) == 0)
	{
		if (local_cid == 0) {
			ptr_ringbuffer_pop(queue, 0);
		}
		else if (l2cap_can_send_packet_now(local_cid) != 0) {
			int length;
			if (length = ptr_ringbuffer_pop(queue, l2cap_get_outgoing_buffer()) > 0)
			{
				ds4emu_crc32_calc_append(l2cap_get_outgoing_buffer(), length);
				l2cap_send_prepared(local_cid, length);
			}
		}
	}
}

void ds4emu_reroute_all()
{
	ds4emu_reroute_channel(&ds2ps_hidintr_queue, bt2ps_hidintr_channel);
	ds4emu_reroute_channel(&ds2ps_hidctrl_queue, bt2ps_hidctrl_channel);
	ds4emu_reroute_channel(&ps2ds_hidintr_queue, bt2ds_hidintr_channel);
	ds4emu_reroute_channel(&ps2ds_hidctrl_queue, bt2ds_hidctrl_channel);
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
