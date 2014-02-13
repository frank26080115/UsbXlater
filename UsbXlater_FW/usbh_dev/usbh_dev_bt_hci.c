#include "usbh_dev_bt_hci.h"
#include "usbh_dev_cb_default.h"
#include "usbh_dev_manager.h"
#include "usbh_devio_manager.h"
#include <usbh_lib/usbh_core.h>
#include <usbh_lib/usbh_hcs.h>
#include <usbh_lib/usbh_ioreq.h>
#include <usbh_lib/usbh_stdreq.h>
#include <stm32fx/peripherals.h>
#include <stdlib.h>
#include <vcp.h>
#include <btstack/btproxy.h>
#include <btstack/hci_transport_stm32usbh.h>
#include <btstack/hci_cmds.h>

USBH_Device_cb_TypeDef USBH_Dev_BtHci_CB = {
	USBH_Dev_BtHci_DeInitDev,
	USBH_Dev_BtHci_Task,
	USBH_Dev_BtHci_Init,
	USBH_Dev_BtHci_DeInit,
	USBH_Dev_BtHci_DeviceAttached,
	USBH_Dev_BtHci_ResetDevice,
	USBH_Dev_BtHci_DeviceDisconnected,
	USBH_Dev_DefaultCB_OverCurrentDetected,
	USBH_Dev_BtHci_DeviceSpeedDetected,
	USBH_Dev_BtHci_DeviceDescAvailable,
	USBH_Dev_BtHci_DeviceAddressAssigned,
	USBH_Dev_BtHci_ConfigurationDescAvailable,
	USBH_Dev_DefaultCB_ManufacturerString,
	USBH_Dev_DefaultCB_ProductString,
	USBH_Dev_DefaultCB_SerialNumString,
	USBH_Dev_BtHci_EnumerationDone,
	USBH_Dev_DefaultCB_DeviceNotSupported,
	USBH_Dev_BtHci_UnrecoveredError,
};

void USBH_Dev_BtHci_DeInitDev(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	UsbHci_Data_t* UsbHci_Data = pdev->Usr_Data;
	USBH_DevIO_Manager_Unplug(pdev);
	if (pdev->Usr_Data != 0) {
		free(pdev->Usr_Data);
	}
}

void USBH_Dev_BtHci_D2HIntf_AclDataHandler(void* p_io, uint8_t* data, uint16_t len)
{
	USBH_DevIO_t* p_in = (USBH_DevIO_t*)p_io;
	UsbHci_Data_t* UsbHci_Data = (UsbHci_Data_t*)p_in->pdev->Usr_Data;
	//dbg_printf(DBGMODE_TRACE, "USB HCI ACL Handler 0x%02X%02X (len %d)\r\n", data[1], data[0], len);
	UsbHci_Data->packet_handler(HCI_ACL_DATA_PACKET, data, len);
}

void USBH_Dev_BtHci_D2HIntf_EventHandler(void* p_io, uint8_t* data, uint16_t len)
{
	USBH_DevIO_t* p_in = (USBH_DevIO_t*)p_io;
	UsbHci_Data_t* UsbHci_Data = (UsbHci_Data_t*)p_in->pdev->Usr_Data;
	dbg_printf(DBGMODE_TRACE, "USB HCI Event 0x%02X 0x%02X (len %d)\r\n", data[0], data[1], len);
	UsbHci_Data->packet_handler(HCI_EVENT_PACKET, data, len);
}

USBH_Status USBH_Dev_BtHci_Task(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	UsbHci_Data_t* UsbHci_Data = (UsbHci_Data_t*)pdev->Usr_Data;
	if (UsbHci_Data->ioIdx <= 0)
	{
		BT_USBH_CORE = pcore;
		BT_USBH_DEV = pdev;
		btproxy_init_USB();
		UsbHci_Data->ioIdx++;
	}

	btproxy_task();

	if (UsbHci_Data->dataOutIO->state != UIOSTATE_NEXT)
	{
		USBH_DevIO_Task(UsbHci_Data->dataOutIO);
	}
	else if (UsbHci_Data->ioIdx == 1)
	{
		USBH_DevIO_Task(UsbHci_Data->eventIO);
		if (UsbHci_Data->eventIO->state == UIOSTATE_NEXT) {
			UsbHci_Data->ioIdx++;
		}
	}
	else if (UsbHci_Data->ioIdx == 2)
	{
		USBH_DevIO_Task(UsbHci_Data->dataInIO);
		if (UsbHci_Data->dataInIO->state == UIOSTATE_NEXT) {
			UsbHci_Data->ioIdx++;
		}
	}
	else
	{
		UsbHci_Data->ioIdx = 1;
	}
	return USBH_OK;
}

void USBH_Dev_BtHci_Init(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	dbg_printf(DBGMODE_TRACE, "USBH_Dev_BtHci_Init \r\n");
}

void USBH_Dev_BtHci_DeInit(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	dbg_printf(DBGMODE_TRACE, "USBH_Dev_BtHci_DeInit \r\n");
}

void USBH_Dev_BtHci_DeviceAttached(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	dbg_printf(DBGMODE_TRACE, "USBH_Dev_BtHci_DeviceAttached \r\n");
}

void USBH_Dev_BtHci_ResetDevice(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	dbg_printf(DBGMODE_TRACE, "USBH_Dev_BtHci_ResetDevice\r\n");
}

void USBH_Dev_BtHci_DeviceDisconnected(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	dbg_printf(DBGMODE_TRACE, "USBH_Dev_BtHci_DeviceDisconnected\r\n");

	UsbHci_Data_t* UsbHci_Data = pdev->Usr_Data;
}

void USBH_Dev_BtHci_OverCurrentDetected(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	dbg_printf(DBGMODE_ERR, "USBH_Dev_BtHci_OverCurrentDetected \r\n");
}

void USBH_Dev_BtHci_DeviceSpeedDetected(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, uint8_t DeviceSpeed)
{
	dbg_printf(DBGMODE_DEBUG, "USBH_Dev_BtHci_DeviceSpeedDetected: %d\r\n", DeviceSpeed);
}

void USBH_Dev_BtHci_DeviceDescAvailable(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, void* data_)
{
	dbg_printf(DBGMODE_TRACE, "UsbHci_DeviceDescAvailable (should not be called) \r\n");
	// should not be callled
}

void USBH_Dev_BtHci_DeviceAddressAssigned(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	dbg_printf(DBGMODE_TRACE, "UsbHci_DeviceAddressAssigned \r\n");

	if (pdev->device_prop.Dev_Desc.bNumConfigurations > 1) {
		dbg_printf(DBGMODE_TRACE, "USB BT dongle, more than 1 configs available!\r\n");
	}
}

void USBH_Dev_BtHci_ConfigurationDescAvailable(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, USBH_CfgDesc_TypeDef * cfg, USBH_InterfaceDesc_TypeDef * intf, USBH_EpDesc_TypeDef ** ep)
{
	dbg_printf(DBGMODE_TRACE, "UsbHci_ConfigurationDescAvailable \r\n");
}

void USBH_Dev_BtHci_EnumerationDone(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	dbg_printf(DBGMODE_TRACE, "UsbHci_EnumerationDone %s\r\n", USBH_Dev_DebugPrint(pdev, 0));

	if (pdev->Usr_Data != 0) {
		free(pdev->Usr_Data);
		pdev->Usr_Data = 0;
	}

	pdev->Usr_Data = calloc(1, sizeof(UsbHci_Data_t));
	UsbHci_Data_t* UsbHci_Data = pdev->Usr_Data;
	UsbHci_Data->ioIdx = 0;
	UsbHci_Data->eventIO = 0;
	UsbHci_Data->dataInIO = 0;
	UsbHci_Data->dataOutIO = 0;

	char dynamicHc = 0;
	#ifdef USBHCI_ENABLE_DYNAMIC_HC_ALLOC
	dynamicHc = 1;
	#endif

	for (int intf = 0; intf < pdev->device_prop.Cfg_Desc.bNumInterfaces; intf++)
	{
		USBH_InterfaceDesc_TypeDef* pintf = &pdev->device_prop.Itf_Desc[intf];

		for (int ep = 0; ep < pintf->bNumEndpoints; ep++)
		{
			USBH_EpDesc_TypeDef* pep = &pdev->device_prop.Ep_Desc[intf][ep];

			if (UsbHci_Data->eventIO == 0 && pep->bEndpointAddress == (0x01 | USB_D2H) && (pep->bmAttributes & EP_TYPE_MSK) == EP_TYPE_INTR)
			{
				UsbHci_Data->eventIO = USBH_DevIO_Manager_New(pcore, pdev, pep, dynamicHc, USBH_Dev_BtHci_D2HIntf_EventHandler, 0);
				dbg_printf(DBGMODE_DEBUG, "USBHCI %s found event EP\r\n", USBH_Dev_DebugPrint(pdev, pep));
			}
			else if (UsbHci_Data->dataInIO == 0 && (pep->bEndpointAddress & USB_EP_DIR_MSK) == USB_D2H && (pep->bmAttributes & EP_TYPE_MSK) == EP_TYPE_BULK)
			{
				UsbHci_Data->dataInIO = USBH_DevIO_Manager_New(pcore, pdev, pep, dynamicHc, USBH_Dev_BtHci_D2HIntf_AclDataHandler, 0);
				dbg_printf(DBGMODE_DEBUG, "USBHCI %s found data in EP\r\n", USBH_Dev_DebugPrint(pdev, pep));
			}
			else if (UsbHci_Data->dataOutIO == 0 && (pep->bEndpointAddress & USB_EP_DIR_MSK) == USB_H2D && (pep->bmAttributes & EP_TYPE_MSK) == EP_TYPE_BULK)
			{
				UsbHci_Data->dataOutIO = USBH_DevIO_Manager_New(pcore, pdev, pep, dynamicHc, 0, 0);
				dbg_printf(DBGMODE_DEBUG, "USBHCI %s found data out EP\r\n", USBH_Dev_DebugPrint(pdev, pep));
			}
		}
	}

	if (UsbHci_Data->eventIO == 0)
	{
		dbg_printf(DBGMODE_DEBUG, "USBHCI %s did not find data in EP, creating manually...\r\n", USBH_Dev_DebugPrint(pdev, 0));
		USBH_EpDesc_TypeDef* pep = &pdev->device_prop.Ep_Desc[pdev->device_prop.Cfg_Desc.bNumInterfaces][1];
		pep->bDescriptorType = USB_DESC_TYPE_ENDPOINT;
		pep->bEndpointAddress = 0x01 | USB_D2H;
		pep->bInterval = 10;
		pep->bLength = USB_ENDPOINT_DESC_SIZE;
		pep->bmAttributes = EP_TYPE_INTR;
		pep->wMaxPacketSize = 16;
		UsbHci_Data->eventIO = USBH_DevIO_Manager_New(pcore, pdev, pep, dynamicHc, USBH_Dev_BtHci_D2HIntf_EventHandler, 0);
	}

	if (UsbHci_Data->dataInIO == 0)
	{
		dbg_printf(DBGMODE_DEBUG, "USBHCI %s did not find data in EP, creating manually...\r\n", USBH_Dev_DebugPrint(pdev, 0));
		USBH_EpDesc_TypeDef* pep = &pdev->device_prop.Ep_Desc[pdev->device_prop.Cfg_Desc.bNumInterfaces][1];
		pep->bDescriptorType = USB_DESC_TYPE_ENDPOINT;
		pep->bEndpointAddress = 0x02 | USB_D2H;
		pep->bInterval = 10;
		pep->bLength = USB_ENDPOINT_DESC_SIZE;
		pep->bmAttributes = EP_TYPE_BULK;
		pep->wMaxPacketSize = 64;
		UsbHci_Data->dataInIO = USBH_DevIO_Manager_New(pcore, pdev, pep, dynamicHc, USBH_Dev_BtHci_D2HIntf_AclDataHandler, 0);
	}

	if (UsbHci_Data->dataOutIO == 0)
	{
		dbg_printf(DBGMODE_DEBUG, "USBHCI %s did not find data out EP, creating manually...\r\n", USBH_Dev_DebugPrint(pdev, 0));
		USBH_EpDesc_TypeDef* pep = &pdev->device_prop.Ep_Desc[pdev->device_prop.Cfg_Desc.bNumInterfaces][2];
		pep->bDescriptorType = USB_DESC_TYPE_ENDPOINT;
		pep->bEndpointAddress = 0x02 | USB_H2D;
		pep->bInterval = 10;
		pep->bLength = USB_ENDPOINT_DESC_SIZE;
		pep->bmAttributes = EP_TYPE_BULK;
		pep->wMaxPacketSize = 64;
		UsbHci_Data->dataOutIO = USBH_DevIO_Manager_New(pcore, pdev, pep, dynamicHc, 0, 0);
	}

	UsbHci_Data->packet_handler = stm32usbh_handle_raw;

	dbg_printf(DBGMODE_TRACE, "USB BT HCI finished all enumeration steps\r\n");
}

void USBH_Dev_BtHci_DeviceNotSupported(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	dbg_printf(DBGMODE_ERR, "USBH_Dev_BtHci_DeviceNotSupported \r\n");
}

void USBH_Dev_BtHci_UnrecoveredError(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	dbg_printf(DBGMODE_ERR, "USBH_Dev_BtHci_UnrecoveredError \r\n");
}

USBH_Status USBH_Dev_BtHci_Command(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, uint8_t* data, int len)
{
	USBH_Status status;

	//dbg_printf(DBGMODE_TRACE, "USB_Hci_Cmd 0x%02X%02X\r\n", data[1], data[0]);

	pdev->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_RECIPIENT_DEVICE | USB_REQ_TYPE_CLASS;
	pdev->Control.setup.b.bRequest = 0;
	pdev->Control.setup.b.wValue.w = 0;
	pdev->Control.setup.b.wIndex.w = 0;
	pdev->Control.setup.b.wLength.w = len;

	status = USBH_CtlReq_Blocking(pcore, pdev, data , len );
	return status;
}

USBH_Status USBH_Dev_BtHci_TxData(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, uint8_t* data, int len)
{
	if (USBH_Dev_BtHci_CanSendPacket(pcore, pdev) == 0) {
		return USBH_BUSY;
	}

	//dbg_printf(DBGMODE_TRACE, "USB_Hci_Tx_ACL %02X%02X%02X%02X%02X%02X%02X%02X\r\n", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);

	UsbHci_Data_t* UsbHci_Data = pdev->Usr_Data;
	USBH_DevIO_t* io = UsbHci_Data->dataOutIO;

	io->out_ptr = data;
	io->remaining = len;

	return USBH_OK;
}

char USBH_Dev_BtHci_CanSendPacket(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	UsbHci_Data_t* UsbHci_Data = pdev->Usr_Data;
	USBH_DevIO_t* io = UsbHci_Data->dataOutIO;
	if (io->remaining > 0)
		return 0;
	if (io->state != UIOSTATE_NEXT)
		return 0;

	return 1;
}
