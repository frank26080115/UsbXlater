#include "usbh_dev_bt_hci.h"
#include "usbh_dev_cb_default.h"
#include "usbh_dev_manager.h"
#include <usbh_lib/usbh_core.h>
#include <usbh_lib/usbh_hcs.h>
#include <usbh_lib/usbh_ioreq.h>
#include <usbh_lib/usbh_stdreq.h>
#include <stm32fx/peripherals.h>
#include <stdlib.h>
#include <vcp.h>
#include <bluetooth/hci.h>

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

void USBH_Dev_BtHci_Handle_InterruptIn(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, void* data_)
{
	dbg_trace();
	UsbHci_Data_t* UsbHci_Data = pdev->Usr_Data;
	HCI_HandleEvent(&UsbHci_Data->bthci, (uint8_t*)data_);
}

void USBH_Dev_BtHci_Handle_BulkIn(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, void* data_)
{
	dbg_trace();
	UsbHci_Data_t* UsbHci_Data = pdev->Usr_Data;
	HCI_HandleData(&UsbHci_Data->bthci, (uint8_t*)data_);
}

void USBH_Dev_BtHci_DeInitDev(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	UsbHci_Data_t* UsbHci_Data = pdev->Usr_Data;
	USBH_Free_Channel(pcore, &(UsbHci_Data->hc_num_bulkout));
	USBH_Free_Channel(pcore, &(UsbHci_Data->hc_num_bulkin));
	USBH_Free_Channel(pcore, &(UsbHci_Data->hc_num_intin));
	UsbHci_Data->start_toggle = 0;
}

USBH_Status USBH_Dev_BtHci_Task(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
  UsbHci_Data_t* UsbHci_Data = pdev->Usr_Data;

  HCI_Task(&UsbHci_Data->bthci);

  USBH_Status status = USBH_OK;

  int hc_num_in;
  char toDeallocate = 0;

  switch (UsbHci_Data->state)
  {

  case UsbHciState_IDLE:

    UsbHci_Data->state = UsbHciState_SYNC;
    //USBH_Free_Channel(pcore, &(pdev->Control.hc_num_in));
    //USBH_Free_Channel(pcore, &(pdev->Control.hc_num_out));

  case UsbHciState_SYNC:

    /* Sync with start of Even Frame */
    if(USB_OTG_IsEvenFrame(pcore) != FALSE)
    {
      UsbHci_Data->state = UsbHciState_GET_DATA;
    }
    else
    {
      break;
    }

  case UsbHciState_GET_DATA:

	#ifdef USBHCI_ENABLE_DYNAMIC_HC_ALLOC
	if (UsbHci_Data->check_bulk == 0)
	{
		if (UsbHci_Data->hc_num_intin < 0) {
			USBH_Open_Channel  (pcore,
								&(UsbHci_Data->hc_num_intin),
								UsbHci_Data->intInEp,
								pdev->device_prop.address,
								pdev->device_prop.speed,
								EP_TYPE_INTR,
								UsbHci_Data->length);
		}
	}
	else
	{
		if (UsbHci_Data->hc_num_bulkin < 0) {
			USBH_Open_Channel  (pcore,
								&(UsbHci_Data->hc_num_bulkin),
								UsbHci_Data->bulkInEp,
								pdev->device_prop.address,
								pdev->device_prop.speed,
								EP_TYPE_BULK,
								UsbHci_Data->length);
		}
	}

	if ((UsbHci_Data->hc_num_intin >= 0 && UsbHci_Data->check_bulk == 0) || (UsbHci_Data->hc_num_bulkin >= 0 && UsbHci_Data->check_bulk != 0)) {
	#endif
	if(USB_OTG_IsEvenFrame(pcore) != FALSE)
	{
		if (UsbHci_Data->check_bulk == 0)
		{
			USBH_InterruptReceiveData(pcore,
									  UsbHci_Data->buff,
									  16,
									  UsbHci_Data->hc_num_intin);
		}
		else
		{
			USBH_BulkReceiveData(pcore,
									  UsbHci_Data->buff,
									  64,
									  UsbHci_Data->hc_num_bulkin);
		}
		UsbHci_Data->start_toggle = 1;

		UsbHci_Data->state = UsbHciState_POLL;
		UsbHci_Data->timer = HCD_GetCurrentFrame(pcore);

		dbgwdg_feed();
	}
	#ifdef USBHCI_ENABLE_DYNAMIC_HC_ALLOC
    }
    else {
      dbg_printf(DBGMODE_ERR, "Unable to allocate channel for USB HCI D2H\r\n");
      UsbHci_Data->hc_num_intin = -1;
    }
    #endif
    break;

  case UsbHciState_POLL:
    hc_num_in = (UsbHci_Data->check_bulk == 0) ? UsbHci_Data->hc_num_intin : UsbHci_Data->hc_num_bulkin;

    if(( HCD_GetCurrentFrame(pcore) - UsbHci_Data->timer) >= UsbHci_Data->poll)
    {
      UsbHci_Data->state = UsbHciState_GET_DATA;
      UsbHci_Data->check_bulk = !UsbHci_Data->check_bulk;
      toDeallocate = 1;
    }
    else if(HCD_GetURB_State(pcore , hc_num_in) == URB_DONE)
    {
      if(UsbHci_Data->start_toggle != 0) /* handle data once */
      {
        UsbHci_Data->start_toggle = 0;
        if (UsbHci_Data->check_bulk == 0) {
          USBH_Dev_BtHci_Handle_InterruptIn(pcore, pdev, UsbHci_Data->buff);
        }
        else {
          USBH_Dev_BtHci_Handle_BulkIn(pcore, pdev, UsbHci_Data->buff);
        }
        UsbHci_Data->state = UsbHciState_GET_DATA;
        toDeallocate = 1;
      }
    }
    else if(HCD_GetURB_State(pcore, hc_num_in) == URB_NOTREADY) // NAK
    {
      toDeallocate = 1;
    }
    else if(HCD_GetURB_State(pcore, hc_num_in) == URB_STALL) // Stalled
    {
      UsbHci_Data->state = UsbHciState_GET_DATA;
      UsbHci_Data->check_bulk = !UsbHci_Data->check_bulk;
      toDeallocate = 1;
    }

    #ifdef USBHCI_ENABLE_DYNAMIC_HC_ALLOC
    if (toDeallocate != 0) {
      if (UsbHci_Data->hc_num_bulkin >= 0) {
        USBH_Free_Channel(pcore, &(UsbHci_Data->hc_num_bulkin));
      }
      if (UsbHci_Data->hc_num_intin >= 0) {
        USBH_Free_Channel(pcore, &(UsbHci_Data->hc_num_intin));
      }
    }
    #endif
    break;

  default:
    dbg_printf(DBGMODE_ERR, "UsbHci_Task unknown UsbHci_Data->state 0x%02X \r\n", UsbHci_Data->state);
    break;
  }
	return status;
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
	dbg_printf(DBGMODE_TRACE, "UsbHci_EnumerationDone \r\n");

	if (pdev->Usr_Data != 0) {
		free(pdev->Usr_Data);
		pdev->Usr_Data = 0;
	}
	pdev->Usr_Data = calloc(1, sizeof(UsbHci_Data_t));
	UsbHci_Data_t* UsbHci_Data = pdev->Usr_Data;
	UsbHci_Data->bthci.usb_core = pcore;
	UsbHci_Data->bthci.usbh_dev = pdev;

	UsbHci_Data->state     = UsbHciState_IDLE;
	//UsbHci_Data->ctl_state = UsbHciState_REQ_IDLE;

	UsbHci_Data->poll = 10;

	UsbHci_Data->intInEp = 0x01 | USB_D2H;
	UsbHci_Data->bulkInEp = 0x03 | USB_D2H;
	UsbHci_Data->bulkOutEp = 0x03 | USB_H2D;

	UsbHci_Data->hc_num_bulkin = -1;
	UsbHci_Data->hc_num_bulkout = -1;
	UsbHci_Data->hc_num_intin = -1;

#ifndef USBHCI_ENABLE_DYNAMIC_HC_ALLOC
	if (USBH_Open_Channel  (pcore,
							&(UsbHci_Data->hc_num_intin),
							UsbHci_Data->intInEp,
							pdev->device_prop.address,
							pdev->device_prop.speed,
							EP_TYPE_INTR,
							UsbHci_Data->length) != HC_OK)
	{
		dbg_printf(DBGMODE_ERR, "Unable to open inter-in-EP for USB BT HCI\r\n");
	}
	if (USBH_Open_Channel  (pcore,
							&(UsbHci_Data->hc_num_bulkin),
							UsbHci_Data->bulkInEp,
							pdev->device_prop.address,
							pdev->device_prop.speed,
							EP_TYPE_BULK,
							UsbHci_Data->length) != HC_OK)
	{
		dbg_printf(DBGMODE_ERR, "Unable to open bulk-in-EP for USB BT HCI\r\n");
	}
#endif
	if (USBH_Open_Channel  (pcore,
							&(UsbHci_Data->hc_num_bulkout),
							UsbHci_Data->bulkOutEp,
							pdev->device_prop.address,
							pdev->device_prop.speed,
							EP_TYPE_BULK,
							UsbHci_Data->length) != HC_OK)
	{
		dbg_printf(DBGMODE_ERR, "Unable to open bulk-out-EP for USB BT HCI\r\n");
	}

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

USBH_Status USBH_Dev_BtHci_Command(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, uint8_t* data, uint8_t len)
{
	USBH_Status status;

	dbg_printf(DBGMODE_TRACE, "USBH_Dev_BtHci_Command \r\n");

	char reqUnalloc = 0;
	if (pdev->Control.hc_num_in < 0)
	{
		reqUnalloc = 1;
		if (USBH_Open_Channel(	pcore,
								&(pdev->Control.hc_num_in),
								0x80,
								pdev->device_prop.address,
								pdev->device_prop.speed,
								EP_TYPE_CTRL,
								pdev->Control.ep0size) != HC_OK)
		{
			dbg_printf(DBGMODE_ERR, "Unable to open ctrl-in-EP for USB BT HCI\r\n");
		}
	}
	if (pdev->Control.hc_num_out < 0)
	{
		reqUnalloc = 1;
		if (USBH_Open_Channel(	pcore,
								&(pdev->Control.hc_num_out),
								0x00,
								pdev->device_prop.address,
								pdev->device_prop.speed,
								EP_TYPE_CTRL,
								pdev->Control.ep0size) != HC_OK)
		{
			dbg_printf(DBGMODE_ERR, "Unable to open ctrl-in-EP for USB BT HCI\r\n");
		}
	}

	pdev->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_RECIPIENT_DEVICE | USB_REQ_TYPE_CLASS;
	pdev->Control.setup.b.bRequest = 0;
	pdev->Control.setup.b.wValue.w = 0;
	pdev->Control.setup.b.wIndex.w = 0;
	pdev->Control.setup.b.wLength.w = len;

	status = USBH_CtlReq_Blocking(pcore, pdev, data , len , 500);

	if (reqUnalloc != 0) {
		USBH_Free_Channel(pcore, &(pdev->Control.hc_num_in));
		USBH_Free_Channel(pcore, &(pdev->Control.hc_num_out));
	}

	return status;
}

USBH_Status USBH_Dev_BtHci_TxData(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, uint8_t* data, uint8_t len)
{
	USBH_Status status;
	UsbHci_Data_t* UsbHci_Data = pdev->Usr_Data;

	char reqUnalloc = 0;
	if (UsbHci_Data->hc_num_bulkout < 0)
	{
		reqUnalloc = 1;
		if (USBH_Open_Channel  (pcore,
								&(UsbHci_Data->hc_num_bulkout),
								UsbHci_Data->bulkOutEp,
								pdev->device_prop.address,
								pdev->device_prop.speed,
								EP_TYPE_BULK,
								UsbHci_Data->length) != HC_OK)
		{
			dbg_printf(DBGMODE_ERR, "Unable to open bulk-out-EP for USB BT HCI\r\n");
		}
	}

	status = USBH_BulkSendData(pcore, data, len, UsbHci_Data->hc_num_bulkout);

	if (reqUnalloc != 0) {
		USBH_Free_Channel(pcore, &(UsbHci_Data->hc_num_bulkout));
	}

	return status;
}
