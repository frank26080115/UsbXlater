#include "usbh_dev_dualshock.h"
#include <usbh_dev/usbh_dev_hid.h>
#include <usbh_lib/usbh_core.h>
#include <usbh_lib/usbh_ioreq.h>
#include <usbh_lib/usbh_hcs.h>
#include <kbm2c/kbm2ctrl.h>
#include <flashfile.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

USB_OTG_CORE_HANDLE* USBH_DSPT_core;
USBH_DEV*            USBH_DSPT_dev;

char USBH_DS3_Available = 0;
char USBH_DS4_Available = 0;
char USBH_DS3_NewData = 0;
char USBH_DS4_NewData = 0;
uint8_t USBH_DS_Buffer[64];
USBH_DS_task_t USBH_DS_PostedTask = 0;

void USBH_DS4_Feature14_1401(void);
void USBH_DS4_Feature14_1402(void);

void USBH_DS4_Init_Handler(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t intf)
{
	USBH_DSPT_core = pcore;
	USBH_DSPT_dev  = pdev;
	USBH_DS4_Available = 1;
}

void USBH_DS4_DeInit_Handler(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t intf)
{
	USBH_DS4_Available = 0;
}

void USBH_DS4_Decode_Handler(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t intf, uint8_t ep, uint8_t* data, uint8_t len)
{
	// assume ep and intf are the only ones available
	if (len > 64) len = 64;
	memcpy(USBH_DS_Buffer, data, len);
	USBH_DS3_NewData = 1;
	if (data[0] == 0x01) {
		kbm2c_handleDs4Report(data);
	}

	if (USBH_DS_PostedTask == USBH_DSPT_TASK_FEATURE_14_1401) {
		//dbg_trace();
		USBH_DS4_Feature14_1401();
	}
	else if (USBH_DS_PostedTask == USBH_DSPT_TASK_FEATURE_14_1402) {
		//dbg_trace();
		USBH_DS4_Feature14_1402();
	}
}

void USBH_DS3_Init_Handler(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t intf)
{
	USBH_DSPT_core = pcore;
	USBH_DSPT_dev  = pdev;
	USBH_DS3_Available = 1;
}

void USBH_DS3_DeInit_Handler(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t intf)
{
	USBH_DS3_Available = 0;
}

void USBH_DS3_Decode_Handler(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t intf, uint8_t ep, uint8_t* data, uint8_t len)
{
	// assume ep and intf are the only ones available
	if (len > 64) len = 64;
	memcpy(USBH_DS_Buffer, data, len);
	USBH_DS3_NewData = 1;
	if (data[0] == 0x01) {
		kbm2c_handleDs3Report(data);
	}
}

void USBH_DS4_GetExtraReports(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev)
{
	USBH_Status status;
	int len;

	char needDeallocChan = 0;
	if (pdev->Control.hc_num_out < 0)
	{
		if (USBH_Open_Channel(	pcore,
							&(pdev->Control.hc_num_out),
							0x00,
							pdev->device_prop.address,
							pdev->device_prop.speed,
							EP_TYPE_CTRL,
							pdev->Control.ep0size) != HC_OK) {
			dbg_printf(DBGMODE_ERR, "Unable to open control-out-EP for DS4 GetExtraReports\r\n");
			return;
		}
		else {
			//dbg_printf(DBGMODE_TRACE, "Opened control-out-EP %d for DS4 GetExtraReports\r\n", pdev->Control.hc_num_out);
		}

		if (USBH_Open_Channel(	pcore,
							&(pdev->Control.hc_num_in),
							0x80,
							pdev->device_prop.address,
							pdev->device_prop.speed,
							EP_TYPE_CTRL,
							pdev->Control.ep0size) != HC_OK) {
			dbg_printf(DBGMODE_ERR, "Unable to open control-in-EP for DS4 GetExtraReports\r\n");
			return;
		}
		else {
			//dbg_printf(DBGMODE_TRACE, "Opened control-in-EP %d for DS4 GetExtraReports\r\n", pdev->Control.hc_num_in);
		}

		needDeallocChan = 1;
	}

	flashfile_cacheFlush();
	nvm_file_t* f = (nvm_file_t*)flashfilesystem.cache;

	len = 36;
	status = USBH_Get_Report(pcore, pdev, 0, 0x03, 0x02, len + 1, USBH_DS_Buffer);
	if (status == USBH_OK)
	{
		if (memcmp(f->d.fmt.report_02, &USBH_DS_Buffer[1], len) != 0) {
			// copy only if different
			memcpy(f->d.fmt.report_02, &USBH_DS_Buffer[1], len);
			flashfilesystem.cache_dirty = 1;
		}
	}
	else
	{
		dbg_printf(DBGMODE_ERR, "DS4 GetReport for 0x02 failed with 0x%08X\r\n", status);
	}

	len = 3 * 5;
	status = USBH_Get_Report(pcore, pdev, 0, 0x03, 0x12, len + 1, USBH_DS_Buffer);
	if (status == USBH_OK)
	{
		if (memcmp(f->d.fmt.report_12, &USBH_DS_Buffer[1], len - 6) != 0) {
			// the subtract 6 is the BD_ADDR that we don't care about
			memcpy(f->d.fmt.report_12, &USBH_DS_Buffer[1], len);
			flashfilesystem.cache_dirty = 1;
		}
	}
	else
	{
		dbg_printf(DBGMODE_ERR, "DS4 GetReport for 0x02 failed with 0x%08X\r\n", status);
	}

	len = 8 * 6;
	status = USBH_Get_Report(pcore, pdev, 0, 0x03, 0xA3, len + 1, USBH_DS_Buffer);
	if (status == USBH_OK)
	{
		if (memcmp(f->d.fmt.report_a3, &USBH_DS_Buffer[1], len) != 0) {
			memcpy(f->d.fmt.report_a3, &USBH_DS_Buffer[1], len);
			flashfilesystem.cache_dirty = 1;
		}
	}
	else
	{
		dbg_printf(DBGMODE_ERR, "DS4 GetReport for 0x02 failed with 0x%08X\r\n", status);
	}

	if (needDeallocChan != 0) {
		USBH_Free_Channel(pcore, &(pdev->Control.hc_num_out));
		USBH_Free_Channel(pcore, &(pdev->Control.hc_num_in));
	}

	flashfile_cacheFlush();
}

void USBH_DS4_SetExtraReports(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev)
{
	USBH_Status status;
	int len;

	char needDeallocChan = 0;
	if (pdev->Control.hc_num_out < 0)
	{
		if (USBH_Open_Channel(	pcore,
							&(pdev->Control.hc_num_out),
							0x00,
							pdev->device_prop.address,
							pdev->device_prop.speed,
							EP_TYPE_CTRL,
							pdev->Control.ep0size) != HC_OK) {
			dbg_printf(DBGMODE_ERR, "Unable to open control-out-EP for DS4 SetExtraReports\r\n");
			return;
		}
		else {
			//dbg_printf(DBGMODE_TRACE, "Opened control-out-EP %d for DS4 SetExtraReports\r\n", pdev->Control.hc_num_out);
		}

		if (USBH_Open_Channel(	pcore,
							&(pdev->Control.hc_num_in),
							0x80,
							pdev->device_prop.address,
							pdev->device_prop.speed,
							EP_TYPE_CTRL,
							pdev->Control.ep0size) != HC_OK) {
			dbg_printf(DBGMODE_ERR, "Unable to open control-in-EP for DS4 SetExtraReports\r\n");
			return;
		}
		else {
			//dbg_printf(DBGMODE_TRACE, "Opened control-in-EP %d for DS4 SetExtraReports\r\n", pdev->Control.hc_num_in);
		}

		needDeallocChan = 1;
	}

	len = 6 + 8 + 8;
	USBH_DS_Buffer[0] = 0x13;
	memcpy(&USBH_DS_Buffer[1], flashfilesystem.nvm_file->d.fmt.report_13, len);
	status = USBH_Set_Report(pcore, pdev, 0, 0x03, 0x13, len + 1, USBH_DS_Buffer);

	if (needDeallocChan != 0) {
		USBH_Free_Channel(pcore, &(pdev->Control.hc_num_out));
		USBH_Free_Channel(pcore, &(pdev->Control.hc_num_in));
	}
}

void USBH_DS4_Feature14_1401(void)
{
	USBH_DS_PostedTask = 0;

	memset(USBH_DSPT_core->host.Rx_Buffer, 0, 17);
	USBH_DSPT_core->host.Rx_Buffer[0] = 0x14;
	USBH_DSPT_core->host.Rx_Buffer[1] = 0x01;

	USBH_DSPT_dev->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_RECIPIENT_INTERFACE | USB_REQ_TYPE_CLASS;
	USBH_DSPT_dev->Control.setup.b.bRequest = 0x01;
	USBH_DSPT_dev->Control.setup.b.wValue.w = 0x0314;
	USBH_DSPT_dev->Control.setup.b.wIndex.w = 0;
	USBH_DSPT_dev->Control.setup.b.wLength.w = 17;

	char needDeallocChan = 0;
	if (USBH_DSPT_dev->Control.hc_num_out < 0)
	{
		if (USBH_Open_Channel(	USBH_DSPT_core,
							&(USBH_DSPT_dev->Control.hc_num_out),
							0x00,
							USBH_DSPT_dev->device_prop.address,
							USBH_DSPT_dev->device_prop.speed,
							EP_TYPE_CTRL,
							USBH_DSPT_dev->Control.ep0size) != HC_OK) {
			dbg_printf(DBGMODE_ERR, "Unable to open control-out-EP for DS4 SetFeature\r\n");
			return;
		}
		else {
			//dbg_printf(DBGMODE_TRACE, "Opened control-out-EP %d for DS4 SetFeature\r\n", USBH_DSPT_dev->Control.hc_num_out);
		}

		if (USBH_Open_Channel(	USBH_DSPT_core,
							&(USBH_DSPT_dev->Control.hc_num_in),
							0x80,
							USBH_DSPT_dev->device_prop.address,
							USBH_DSPT_dev->device_prop.speed,
							EP_TYPE_CTRL,
							USBH_DSPT_dev->Control.ep0size) != HC_OK) {
			dbg_printf(DBGMODE_ERR, "Unable to open control-in-EP for DS4 SetFeature\r\n");
			return;
		}
		else {
			//dbg_printf(DBGMODE_TRACE, "Opened control-in-EP %d for DS4 SetFeature\r\n", USBH_DSPT_dev->Control.hc_num_in);
		}

		needDeallocChan = 1;
	}

	if (USBH_DSPT_dev->Control.hc_num_out >= 0) {
		USBH_Status status = USBH_CtlReq_Blocking(USBH_DSPT_core, USBH_DSPT_dev, USBH_DSPT_core->host.Rx_Buffer , 17 , 500);
	}

	if (needDeallocChan != 0) {
		USBH_Free_Channel(USBH_DSPT_core, &(USBH_DSPT_dev->Control.hc_num_out));
		USBH_Free_Channel(USBH_DSPT_core, &(USBH_DSPT_dev->Control.hc_num_in));
	}
}

void USBH_DS4_Feature14_1402(void)
{
	USBH_DS_PostedTask = 0;

	memset(USBH_DSPT_core->host.Rx_Buffer, 0, 17);
	USBH_DSPT_core->host.Rx_Buffer[0] = 0x14;
	USBH_DSPT_core->host.Rx_Buffer[1] = 0x02;

	USBH_DSPT_dev->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_RECIPIENT_INTERFACE | USB_REQ_TYPE_CLASS;
	USBH_DSPT_dev->Control.setup.b.bRequest = 0x01;
	USBH_DSPT_dev->Control.setup.b.wValue.w = 0x0314;
	USBH_DSPT_dev->Control.setup.b.wIndex.w = 0;
	USBH_DSPT_dev->Control.setup.b.wLength.w = 17;

	char needDeallocChan = 0;
	if (USBH_DSPT_dev->Control.hc_num_out < 0)
	{
		if (USBH_Open_Channel(	USBH_DSPT_core,
							&(USBH_DSPT_dev->Control.hc_num_out),
							0x00,
							USBH_DSPT_dev->device_prop.address,
							USBH_DSPT_dev->device_prop.speed,
							EP_TYPE_CTRL,
							USBH_DSPT_dev->Control.ep0size) != HC_OK) {
			dbg_printf(DBGMODE_ERR, "Unable to open control-out-EP for DS4 SetFeature\r\n");
			return;
		}
		else {
			//dbg_printf(DBGMODE_TRACE, "Opened control-out-EP %d for DS4 SetFeature\r\n", USBH_DSPT_dev->Control.hc_num_out);
		}

		if (USBH_Open_Channel(	USBH_DSPT_core,
							&(USBH_DSPT_dev->Control.hc_num_in),
							0x80,
							USBH_DSPT_dev->device_prop.address,
							USBH_DSPT_dev->device_prop.speed,
							EP_TYPE_CTRL,
							USBH_DSPT_dev->Control.ep0size) != HC_OK) {
			dbg_printf(DBGMODE_ERR, "Unable to open control-in-EP for DS4 SetFeature\r\n");
			return;
		}
		else {
			//dbg_printf(DBGMODE_TRACE, "Opened control-in-EP %d for DS4 SetFeature\r\n", USBH_DSPT_dev->Control.hc_num_in);
		}

		needDeallocChan = 1;
	}

	if (USBH_DSPT_dev->Control.hc_num_out >= 0) {
		USBH_Status status = USBH_CtlReq_Blocking(USBH_DSPT_core, USBH_DSPT_dev, USBH_DSPT_core->host.Rx_Buffer , 17 , 500);
	}

	if (needDeallocChan != 0) {
		USBH_Free_Channel(USBH_DSPT_core, &(USBH_DSPT_dev->Control.hc_num_out));
		USBH_Free_Channel(USBH_DSPT_core, &(USBH_DSPT_dev->Control.hc_num_in));
	}
}
