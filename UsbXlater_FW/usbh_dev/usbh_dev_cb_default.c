#include "usbh_dev_cb_default.h"
#include "usbh_dev_hub.h"
#include <usbh_lib/usbh_hcs.h>

USBH_Device_cb_TypeDef USBH_Dev_CB_Default = {
	USBH_Dev_DefaultCB_DeInitDev,
	USBH_Dev_DefaultCB_Task,
	USBH_Dev_DefaultCB_Init,
	USBH_Dev_DefaultCB_DeInit,
	USBH_Dev_DefaultCB_DeviceAttached,
	USBH_Dev_DefaultCB_ResetDevice,
	USBH_Dev_DefaultCB_DeviceDisconnected,
	USBH_Dev_DefaultCB_OverCurrentDetected,
	USBH_Dev_DefaultCB_DeviceSpeedDetected,
	USBH_Dev_DefaultCB_DeviceDescAvailable,
	USBH_Dev_DefaultCB_DeviceAddressAssigned,
	USBH_Dev_DefaultCB_ConfigurationDescAvailable,
	USBH_Dev_DefaultCB_ManufacturerString,
	USBH_Dev_DefaultCB_ProductString,
	USBH_Dev_DefaultCB_SerialNumString,
	USBH_Dev_DefaultCB_EnumerationDone,
	USBH_Dev_DefaultCB_DeviceNotSupported,
	USBH_Dev_DefaultCB_UnrecoveredError,
};

extern USBH_Device_cb_TypeDef USBH_Dev_HID_CB;
extern USBH_Device_cb_TypeDef USBH_Dev_Hub_CB;

void USBH_Dev_DefaultCB_DeInitDev(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{

}

USBH_Status USBH_Dev_DefaultCB_Task(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	// this is called periodically until the device state machine changes

	char isDevRoot = pdev->Parent == 0;
	if (isDevRoot == 0) {
		// since this device now has an address
		// we can let the hub handle new devices on another port
		USBH_DEV* parentHub = (USBH_DEV*)pdev->Parent;
		Hub_Data_t* hubData = (Hub_Data_t*)parentHub->Usr_Data;
		hubData->port_busy = 0;
		// this is a useless device, free the channel so another dev can use them
		USBH_Free_Channel(pcore, &(pdev->Control.hc_num_in));
		USBH_Free_Channel(pcore, &(pdev->Control.hc_num_out));
	}

	return USBH_OK;
}

void USBH_Dev_DefaultCB_Init(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{

}

void USBH_Dev_DefaultCB_DeInit(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{

}

void USBH_Dev_DefaultCB_DeviceAttached(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	dbg_trace();
}

void USBH_Dev_DefaultCB_ResetDevice(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	dbg_trace();
}

void USBH_Dev_DefaultCB_DeviceDisconnected(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	dbg_trace();
}

void USBH_Dev_DefaultCB_OverCurrentDetected(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	dbg_trace();
}

void USBH_Dev_DefaultCB_DeviceSpeedDetected(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, uint8_t DeviceSpeed)
{
	dbg_trace();
}

void USBH_Dev_DefaultCB_DeviceDescAvailable(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, void* data_)
{
	dbg_trace();

	// we do not know what type of device this is yet
	// this is the first callback of the enumeration process
	switch (pdev->device_prop.Dev_Desc.bDeviceClass)
	{
		case 0x00:
			// check interface
			// fall through
		case 0x03:
			// HID
			pdev->cb = &USBH_Dev_HID_CB;
			dbg_printf(DBGMODE_TRACE, "USBH_Dev_DefaultCB_DeviceDescAvailable Found Possible HID 0x%02X\r\n", pdev->device_prop.Dev_Desc.bDeviceClass);
			break;
		case 0x09:
			// hub
			pdev->cb = &USBH_Dev_Hub_CB;
			dbg_printf(DBGMODE_TRACE, "USBH_Dev_DefaultCB_DeviceDescAvailable Found Hub 0x%02X\r\n", pdev->device_prop.Dev_Desc.bDeviceClass);
			break;
		case 0x08:
			// mass storage
			// fall through
		default:
			// unknown
			dbg_printf(DBGMODE_ERR, "USBH_Dev_DefaultCB_DeviceDescAvailable unknown bDeviceClass 0x%02X\r\n", pdev->device_prop.Dev_Desc.bDeviceClass);
		;
	}
}

void USBH_Dev_DefaultCB_DeviceAddressAssigned(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	dbg_trace();
}

void USBH_Dev_DefaultCB_ConfigurationDescAvailable(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, USBH_CfgDesc_TypeDef * cfg, USBH_InterfaceDesc_TypeDef * intf, USBH_EpDesc_TypeDef ** epdesc)
{
	dbg_trace();
}

void USBH_Dev_DefaultCB_ManufacturerString(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, void * data_)
{
	dbg_printf(DBGMODE_DEBUG, "USBH_Dev_DefaultCB_ManufacturerString: %s\r\n", data_);
}

void USBH_Dev_DefaultCB_ProductString(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, void * data_)
{
	dbg_printf(DBGMODE_DEBUG, "USBH_Dev_DefaultCB_ProductString: %s\r\n", data_);
}

void USBH_Dev_DefaultCB_SerialNumString(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, void * data_)
{
	dbg_printf(DBGMODE_DEBUG, "USBH_Dev_DefaultCB_SerialNumString: %s\r\n", data_);
}

void USBH_Dev_DefaultCB_EnumerationDone(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	vcp_printf("Unknown Class Device V%04XP%04XA%d Enumerated\r\n", pdev->device_prop.Dev_Desc.idVendor, pdev->device_prop.Dev_Desc.idProduct, pdev->device_prop.address);
}

void USBH_Dev_DefaultCB_DeviceNotSupported(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	dbg_trace();
}

void USBH_Dev_DefaultCB_UnrecoveredError(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	dbg_trace();
}
