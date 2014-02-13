#include "usbh_dev_manager.h"
#include <usbh_lib/usbh_core.h>
#include <usbh_lib/usbh_def.h>
#include <usbh_lib/usbh_hcs.h>
#include <usbotg_lib/usb_core.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

volatile uint32_t USBH_Dev_Reset_Timer = 0;

#define USBH_DEV_MAX_ADDR 31
#define USBH_DEV_MAX_ADDR_DIV8 ((USBH_DEV_MAX_ADDR + 1) / 8)
uint8_t USBH_Dev_UsedAddr[USBH_DEV_MAX_ADDR_DIV8 + 1];

int8_t USBH_Dev_GetAvailAddress()
{
	// 0 is only used for unenumerated devices, never assign 0
	// go through all of them
	for (int i = 1; i < USBH_DEV_MAX_ADDR; i++)
	{
		uint8_t bitIdx = i % 8;
		uint8_t byteIdx = i / 8;
		if ((USBH_Dev_UsedAddr[byteIdx] & (1 << bitIdx)) == 0) { // it is unused
			USBH_Dev_UsedAddr[byteIdx] |= (1 << bitIdx); // mark as used
			return i; // found
		}
	}
	return -1;
}

void USBH_Dev_FreeAddress(uint8_t addr)
{
	uint8_t bitIdx = addr % 8;
	uint8_t byteIdx = addr / 8;
	USBH_Dev_UsedAddr[byteIdx] &= ~(1 << bitIdx); // mark as used
}

void USBH_Dev_AddrManager_Reset()
{
	// mark all addresses as free
	for (int i = 0; i < USBH_DEV_MAX_ADDR_DIV8; i++) {
		USBH_Dev_UsedAddr[i] = 0;
	}
}

void USBH_Dev_FreeControl(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev)
{
	if (pdev->Control.hc_num_in >= 0 || pdev->Control.hc_num_in >= 0)
	{
		#ifdef ENABLE_HC_DEBUG
		dbg_printf(DBGMODE_ERR, "Closing CTRL EP HCs for %s (in %d out %d)\r\n", USBH_Dev_DebugPrint(pdev, 0), pdev->Control.hc_num_in, pdev->Control.hc_num_out);
		#endif

		// remember the toggle states so PID's DATA0 and DATA1 are correct for packet sync
		pdev->Control.hc_in_tgl_in = pcore->host.hc[pdev->Control.hc_num_in].toggle_in;
		pdev->Control.hc_in_tgl_out = pcore->host.hc[pdev->Control.hc_num_in].toggle_out;
		pdev->Control.hc_out_tgl_in = pcore->host.hc[pdev->Control.hc_num_out].toggle_in;
		pdev->Control.hc_out_tgl_out = pcore->host.hc[pdev->Control.hc_num_out].toggle_out;
	}

	USBH_Free_Channel(pcore, &(pdev->Control.hc_num_in));
	USBH_Free_Channel(pcore, &(pdev->Control.hc_num_out));
}

int8_t USBH_Dev_AllocControl(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev)
{
	char ret = 0;
	if (pdev->Control.hc_num_in < 0)
	{
		ret |= 1;
		if (USBH_Open_Channel(	pcore,
								&(pdev->Control.hc_num_in),
								0x80,
								pdev->device_prop.address,
								pdev->device_prop.speed,
								EP_TYPE_CTRL,
								pdev->Control.ep0size) != HC_OK)
		{
			ret |= 4;
			dbg_printf(DBGMODE_ERR, "%s unable to open CTRL IN EP\r\n", USBH_Dev_DebugPrint(pdev, 0));
		}
		else
		{
			// restore the toggle states so PID's DATA0 and DATA1 are correct for packet sync
			if (pdev->Control.hc_in_tgl_in >= 0) pcore->host.hc[pdev->Control.hc_num_in].toggle_in = pdev->Control.hc_in_tgl_in;
			if (pdev->Control.hc_in_tgl_out >= 0) pcore->host.hc[pdev->Control.hc_num_in].toggle_out = pdev->Control.hc_in_tgl_out;
		}
	}
	if (pdev->Control.hc_num_out < 0)
	{
		ret |= 2;
		if (USBH_Open_Channel(	pcore,
								&(pdev->Control.hc_num_out),
								0x00,
								pdev->device_prop.address,
								pdev->device_prop.speed,
								EP_TYPE_CTRL,
								pdev->Control.ep0size) != HC_OK)
		{
			ret |= 8;
			dbg_printf(DBGMODE_ERR, "%s unable to open CTRL OUT EP\r\n", USBH_Dev_DebugPrint(pdev, 0));
		}
		else
		{
			// restore the toggle states so PID's DATA0 and DATA1 are correct for packet sync
			if (pdev->Control.hc_out_tgl_in >= 0) pcore->host.hc[pdev->Control.hc_num_out].toggle_in = pdev->Control.hc_out_tgl_in;
			if (pdev->Control.hc_out_tgl_out >= 0) pcore->host.hc[pdev->Control.hc_num_out].toggle_out = pdev->Control.hc_out_tgl_out;
		}
	}

	if ((ret & 0x03) != 0) {
		#ifdef ENABLE_HC_DEBUG
		dbg_printf(DBGMODE_ERR, "Opened CTRL EPs for %s, in:%d out:%d\r\n", USBH_Dev_DebugPrint(pdev, 0), pdev->Control.hc_num_in, pdev->Control.hc_num_out);
		#endif
		return 1;
	}
	else if (ret == 0x00) {
		return 0;
	}
	else {
		return -1;
	}
}

char* USBH_Dev_DebugPrint(USBH_DEV *pdev, USBH_EpDesc_TypeDef* ep)
{
	int idx = GLOBAL_TEMP_BUFF_SIZE - 32;
	if (pdev != 0) {
		idx += sprintf(&global_temp_buff[idx], "V%04XP%04X", pdev->device_prop.Dev_Desc.idVendor, pdev->device_prop.Dev_Desc.idProduct);
		if (pdev->Parent == 0) {
			idx += sprintf(&global_temp_buff[idx], "R");
		}
		idx += sprintf(&global_temp_buff[idx], "A%d", pdev->device_prop.address);
	}
	if (ep != 0) {
		idx += sprintf(&global_temp_buff[idx], "EP%02X", ep->bEndpointAddress);
	}
	return &global_temp_buff[GLOBAL_TEMP_BUFF_SIZE - 32];
}
