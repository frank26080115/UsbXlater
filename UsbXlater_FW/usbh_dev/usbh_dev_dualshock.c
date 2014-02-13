#include "usbh_dev_dualshock.h"
#include <ds4_emu.h>
#include <usbh_dev/usbh_dev_manager.h>
#include <usbh_dev/usbh_devio_manager.h>
#include <usbh_dev/usbh_dev_hid.h>
#include <usbh_lib/usbh_core.h>
#include <usbh_lib/usbh_ioreq.h>
#include <usbh_lib/usbh_hcs.h>
#include <kbm2c/kbm2ctrl.h>
#include <flashfile.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

uint8_t USBH_DS_Buffer[64];

char USBH_DS3_Available = 0;
char USBH_DS4_Available = 0;
char USBH_DS3_NewData = 0;
char USBH_DS4_NewData = 0;

void USBH_DS4_Init_Handler(USB_OTG_CORE_HANDLE* pcore, USBH_DEV* pdev)
{
	HID_Data_t* HID_Data = pdev->Usr_Data;
	uint8_t numIntf = pdev->device_prop.Cfg_Desc.bNumInterfaces;

	for (int i = 0; i < numIntf; i++) {
		if (HID_Data->io[i] != 0) {
			//HID_Data->io[i]->timeout = 100;
			HID_Data->io[i]->enabled = 0;
			//HID_Data->io[i]->priority = 10000;
		}
	}

	DS4EMU_State &= ~(
			EMUSTATE_HAS_DS4_BDADDR |
			EMUSTATE_GIVEN_PS4_BDADDR |
			EMUSTATE_HAS_MFG_DATE |
			EMUSTATE_HAS_REPORT_02 |
			EMUSTATE_GIVEN_DS4_1401 |
			EMUSTATE_GIVEN_DS4_1402 |
		0 );
	DS4EMU_State |= EMUSTATE_IS_DS4_USB_CONNECTED;
}

void USBH_DS4_Task(USB_OTG_CORE_HANDLE* pcore, USBH_DEV* pdev)
{
	HID_Data_t* HID_Data = pdev->Usr_Data;
	USBH_Status status;
	int len;
	USBH_Dev_AllocControl(pcore, pdev);
	if ((DS4EMU_State & EMUSTATE_HAS_DS4_BDADDR) == 0)
	{
		len = 3 * 5;
		if(status = USBH_Get_Report(pcore, pdev, 0, 0x03, 0x12, len + 1, USBH_DS_Buffer) == USBH_OK)
		{
			DS4EMU_State |= EMUSTATE_HAS_DS4_BDADDR;
			memcpy(ds4_bdaddr, &USBH_DS_Buffer[1], BD_ADDR_LEN);
			dbg_printf(DBGMODE_DEBUG, "DS4 %s xfer BDADDR ", USBH_Dev_DebugPrint(pdev, 0));
			dbg_printf(DBGMODE_DEBUG, "%s\r\n", print_bdaddr(ds4_bdaddr));
		}
		return;
	}
	if ((DS4EMU_State & EMUSTATE_HAS_MFG_DATE) == 0)
	{
		len = DS4_MFG_DATE_LEN;
		if(status = USBH_Get_Report(pcore, pdev, 0, 0x03, 0xA3, len + 1, USBH_DS_Buffer) == USBH_OK)
		{
			DS4EMU_State |= EMUSTATE_HAS_MFG_DATE;
			memcpy(ds4_mfg_date, &USBH_DS_Buffer[1], len);
			dbg_printf(DBGMODE_DEBUG, "DS4 %s xfer MFG date\r\n", USBH_Dev_DebugPrint(pdev, 0));
		}
		return;
	}
	if ((DS4EMU_State & EMUSTATE_HAS_REPORT_02) == 0)
	{
		len = DS4_REPORT_02_LEN - 1;
		if(status = USBH_Get_Report(pcore, pdev, 0, 0x03, 0x02, len + 1, USBH_DS_Buffer) == USBH_OK)
		{
			DS4EMU_State |= EMUSTATE_HAS_REPORT_02;
			memcpy(ds4_report02, &USBH_DS_Buffer[1], len);
			dbg_printf(DBGMODE_DEBUG, "DS4 %s xfer report 0x02\r\n", USBH_Dev_DebugPrint(pdev, 0));
		}
		return;
	}
	if ((DS4EMU_State & EMUSTATE_GIVEN_PS4_BDADDR) == 0 && (DS4EMU_State & EMUSTATE_HAS_BT_BDADDR) != 0)
	{
		len = BD_ADDR_LEN + LINK_KEY_LEN;
		USBH_DS_Buffer[0] = 0x13;
		memcpy(&USBH_DS_Buffer[1], hci_local_bd_addr(), BD_ADDR_LEN);
		memcpy(&USBH_DS_Buffer[1 + BD_ADDR_LEN], ds4_link_key, LINK_KEY_LEN);
		if(status = USBH_Set_Report_Blocking(pcore, pdev, 0, 0x03, USBH_DS_Buffer[0], len + 1, USBH_DS_Buffer) == USBH_OK)
		{
			DS4EMU_State |= EMUSTATE_GIVEN_PS4_BDADDR;
			dbg_printf(DBGMODE_DEBUG, "DS4 %s given BT dongle's BDADDR\r\n", USBH_Dev_DebugPrint(pdev, 0));
		}
		return;
	}
	if ((DS4EMU_State & EMUSTATE_GIVEN_PS4_BDADDR) != 0 && (DS4EMU_State & EMUSTATE_HAS_BT_BDADDR) != 0 && (DS4EMU_State & EMUSTATE_GIVEN_DS4_1401) == 0)
	{
		len = 16;
		memset(USBH_DS_Buffer, 0, len + 1);
		USBH_DS_Buffer[0] = 0x14;
		USBH_DS_Buffer[1] = 0x01;
		if(status = USBH_Set_Report_Blocking(pcore, pdev, 0, 0x03, USBH_DS_Buffer[0], len + 1, USBH_DS_Buffer) == USBH_OK)
		{
			DS4EMU_State |= EMUSTATE_GIVEN_DS4_1401;
			dbg_printf(DBGMODE_DEBUG, "DS4 %s sent 0x14 0x01\r\n", USBH_Dev_DebugPrint(pdev, 0));
		}
		return;
	}
	if ((DS4EMU_State & EMUSTATE_GIVEN_PS4_BDADDR) != 0 && (DS4EMU_State & EMUSTATE_HAS_BT_BDADDR) != 0 && (DS4EMU_State & EMUSTATE_GIVEN_DS4_1401) != 0 && (DS4EMU_State & EMUSTATE_GIVEN_DS4_1402) == 0 && (DS4EMU_State & EMUSTATE_IS_DS4_BT_CONNECTED) == 0)
	{
		len = 16;
		USBH_DS_Buffer[0] = 0x14;
		USBH_DS_Buffer[1] = 0x02;
		if(status = USBH_Set_Report_Blocking(pcore, pdev, 0, 0x03, USBH_DS_Buffer[0], len + 1, USBH_DS_Buffer) == USBH_OK)
		{
			DS4EMU_State |= EMUSTATE_GIVEN_DS4_1402;
		}
		return;
	}
	if ((DS4EMU_State & EMUSTATE_GIVEN_PS4_BDADDR) != 0 && (DS4EMU_State & EMUSTATE_HAS_BT_BDADDR) != 0 && (DS4EMU_State & EMUSTATE_GIVEN_DS4_1401) != 0 && (DS4EMU_State & EMUSTATE_IS_DS4_BT_CONNECTED) != 0)
	{
		USBH_Dev_FreeControl(pcore, pdev);
		dbg_printf(DBGMODE_DEBUG, "DS4 %s all tasks completed, deallocated CTRL-EP\r\n", USBH_Dev_DebugPrint(pdev, 0));
	}
}

void USBH_DS4_DeInit_Handler(USB_OTG_CORE_HANDLE* pcore, USBH_DEV* pdev)
{
	DS4EMU_State &= ~EMUSTATE_IS_DS4_USB_CONNECTED;
}

void USBH_DS4_Data_Handler(void* p_io, uint8_t* data, uint16_t len)
{
	USBH_DevIO_t* p_in = p_io;

	if ((DS4EMU_State & EMUSTATE_IS_DS4_BT_CONNECTED) != 0)
	{
		p_in->enabled = 0;
	}
	else
	{
		p_in->enabled = 1;
	}

	if (data[0] == 0x01) {
		kbm2c_handleDs4Report(&data[1]);
	}
}

void USBH_DS3_Data_Handler(void* p_io, uint8_t* data, uint16_t len)
{
	if (data[0] == 0x01) {
		kbm2c_handleDs3Report(data);
	}
}
