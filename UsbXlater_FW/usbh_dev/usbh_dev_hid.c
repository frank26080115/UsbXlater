#include "usbh_dev_hid.h"
#include "usbh_dev_cb_default.h"
#include "usbh_dev_hub.h"
#include "usbh_dev_manager.h"
#include <usbh_lib/usbh_core.h>
#include <usbh_lib/usbh_hcs.h>
#include <usbh_lib/usbh_ioreq.h>
#include <usbh_lib/usbh_stdreq.h>
#include <stdlib.h>
#include <core_cmInstr.h>
#include <vcp.h>
#include <hidrpt.h>
#include <kbm2c/kbm2ctrl.h>
#include "usbh_dev_dualshock.h"

static void USBH_ParseHIDDesc (USBH_HIDDesc_TypeDef *desc, uint8_t *buf);
static USBH_Status USBH_HID_Handle(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *phost, uint8_t intf);
static USBH_Status USBH_HID_ClassRequest(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *phost, uint8_t intf);
static USBH_Status USBH_Get_HID_ReportDescriptor (USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint16_t intf, uint16_t length);
static USBH_Status USBH_Get_HID_Descriptor (USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t intf);
static USBH_Status USBH_Set_Idle (USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t intf, uint8_t duration, uint8_t reportId);
static USBH_Status USBH_Set_Protocol (USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t intf, uint8_t protocol);

USBH_Device_cb_TypeDef USBH_Dev_HID_CB = {
	USBH_Dev_HID_DeInitDev,
	USBH_Dev_HID_Task,
	USBH_Dev_HID_Init,
	USBH_Dev_HID_DeInit,
	USBH_Dev_HID_DeviceAttached,
	USBH_Dev_HID_ResetDevice,
	USBH_Dev_HID_DeviceDisconnected,
	USBH_Dev_HID_OverCurrentDetected,
	USBH_Dev_HID_DeviceSpeedDetected,
	USBH_Dev_HID_DeviceDescAvailable,
	USBH_Dev_HID_DeviceAddressAssigned,
	USBH_Dev_HID_ConfigurationDescAvailable,
	USBH_Dev_DefaultCB_ManufacturerString,
	USBH_Dev_DefaultCB_ProductString,
	USBH_Dev_DefaultCB_SerialNumString,
	USBH_Dev_HID_EnumerationDone,
	USBH_Dev_HID_DeviceNotSupported,
	USBH_Dev_HID_UnrecoveredError,
};

char USBH_Dev_Has_HID;

void HID_Init_Default_Handler(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t intf)
{
}

void HID_Decode_Default_Handler(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t intf, uint8_t ep, uint8_t* data, uint8_t len)
{
	HID_Data_t* HID_Data = &(((HID_Data_t*)(pdev->Usr_Data))[intf]);
	HID_Rpt_Parsing_Params_t* parser = &HID_Data->parserParams;
	if (parser->mouse_exists > 0 && parser->mouse_intf == intf && (parser->mouse_report_id <= 0 || parser->mouse_report_id == data[0]))
	{
		if (parser->mouse_report_id > 0) { data = &data[1]; len--; }
		kbm2c_handleMouseReport(data, len, parser);
	}
	if (parser->kb_exists > 0 && parser->kb_intf == intf && (parser->kb_report_id <= 0 || parser->kb_report_id == data[0]))
	{
		if (parser->kb_report_id > 0) { data = &data[1]; len--; }
		kbm2c_handleKeyReport(data[0], &data[2], len - 2);
	}
}

void HID_DeInit_Default_Handler(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t intf)
{
}

void USBH_Dev_HID_FreeData(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev)
{
	uint8_t numIntf = pdev->device_prop.Cfg_Desc.bNumInterfaces;
	if (pdev->Usr_Data != 0)
	{
		for (int i = 0; i < numIntf; i++)
		{
			HID_Data_t* HID_Data = &(((HID_Data_t*)(pdev->Usr_Data))[i]);
			if (HID_Data != 0)
			{
				if (HID_Data->cb != 0) {
					HID_Data->cb->DeInit(pcore, pdev, i);
					free(HID_Data->cb);
					HID_Data->cb = 0;
				}
			}
		}
		free(pdev->Usr_Data);
		pdev->Usr_Data = 0;
	}
}

void USBH_Dev_HID_DeInitDev(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev)
{
	uint8_t numIntf = pdev->device_prop.Cfg_Desc.bNumInterfaces;
	for (int i = 0; i < numIntf; i++)
	{
		HID_Data_t* HID_Data = &(((HID_Data_t*)(pdev->Usr_Data))[i]);

		if (HID_Data != 0)
		{
			USBH_Free_Channel(pcore, &(HID_Data->hc_num_in));
			USBH_Free_Channel(pcore, &(HID_Data->hc_num_out));
			HID_Data->start_toggle = 0;
		}
	}

	USBH_Dev_HID_FreeData(pcore, pdev);
}

USBH_Status USBH_Dev_HID_Task(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev)
{
	uint8_t numIntf = pdev->device_prop.Cfg_Desc.bNumInterfaces;
	if (numIntf == 0) {
		dbg_printf(DBGMODE_ERR, "Warning: HID_Task device with 0 interfaces! \r\n");
	}
	for (int i = 0; i < numIntf; i++) {
		USBH_HID_Handle(pcore, pdev, i);
	}
	return USBH_OK;
}

void USBH_Dev_HID_Init(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev)
{

}

void USBH_Dev_HID_DeInit(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev)
{

}

void USBH_Dev_HID_DeviceAttached(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev)
{

}

void USBH_Dev_HID_ResetDevice(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev)
{

}

void USBH_Dev_HID_DeviceDisconnected(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev)
{

}

void USBH_Dev_HID_OverCurrentDetected(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev)
{

}

void USBH_Dev_HID_DeviceSpeedDetected(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t DeviceSpeed)
{

}

void USBH_Dev_HID_DeviceDescAvailable(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, void* data_)
{
	// should not be called
}

void USBH_Dev_HID_DeviceAddressAssigned(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev)
{

}

void USBH_Dev_HID_ConfigurationDescAvailable(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, USBH_CfgDesc_TypeDef * cfg, USBH_InterfaceDesc_TypeDef * intf, USBH_EpDesc_TypeDef ** epdesc)
{

}

void USBH_Dev_HID_EnumerationDone(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev)
{
	if (pdev->Usr_Data != 0) {
		USBH_Dev_HID_FreeData(pcore, pdev);
	}

	uint8_t numIntf = pdev->device_prop.Cfg_Desc.bNumInterfaces;
	char isHid = 0;
	pdev->Usr_Data = calloc(numIntf, sizeof(HID_Data_t));

	dbg_printf(DBGMODE_DEBUG, "USBH_Dev_HID_EnumerationDone, bNumInterfaces: %d\r\n", numIntf);

	for (int i = 0; i < numIntf; i++)
	{
		// filter out non-HID class interfaces
		if (pdev->device_prop.Itf_Desc[i].bInterfaceClass != 0x03) {
			continue;
		}

		isHid = 1;

		HID_Data_t* HID_Data = &(((HID_Data_t*)(pdev->Usr_Data))[i]);

		HID_Data->cb = malloc(sizeof(HID_cb_TypeDef));

		if (pdev->device_prop.Dev_Desc.idVendor == SONY_VID && pdev->device_prop.Dev_Desc.idProduct == DUALSHOCK3_PID) {
			// DUALSHOCK3
			HID_Data->cb->Init   = USBH_DS3_Init_Handler;
			HID_Data->cb->Decode = USBH_DS3_Decode_Handler;
			HID_Data->cb->DeInit = USBH_DS3_DeInit_Handler;
			dbg_printf(DBGMODE_TRACE, "Dualshock 3 Detected\r\n");
		}
		else if (pdev->device_prop.Dev_Desc.idVendor == SONY_VID && pdev->device_prop.Dev_Desc.idProduct == DUALSHOCK4_PID) {
			// DUALSHOCK4
			HID_Data->cb->Init   = USBH_DS4_Init_Handler;
			HID_Data->cb->Decode = USBH_DS4_Decode_Handler;
			HID_Data->cb->DeInit = USBH_DS4_DeInit_Handler;
			dbg_printf(DBGMODE_TRACE, "Dualshock 4 Detected\r\n");
		}
		// TODO handle others, like Xbox controllers
		else {
			// other, probably keyboard or mouse
			HID_Data->cb->Init   = HID_Init_Default_Handler;
			HID_Data->cb->Decode = HID_Decode_Default_Handler;
			HID_Data->cb->DeInit = HID_DeInit_Default_Handler;
		}

		HID_Data->state = HID_IDLE;
		HID_Rpt_Parsing_Params_Reset(&HID_Data->parserParams);

		uint8_t maxEP = ( (pdev->device_prop.Itf_Desc[i].bNumEndpoints <= USBH_MAX_NUM_ENDPOINTS) ?
						pdev->device_prop.Itf_Desc[i].bNumEndpoints :
						USBH_MAX_NUM_ENDPOINTS);

		// assign interrupt endpoints to channels, and polling parameters for the D2H endpoint
		// warning: assume 1 interrupt endpoint per direction
		for (int j = 0; j < maxEP; j++)
		{
			USBH_EpDesc_TypeDef* epDesc = &(pdev->device_prop.Ep_Desc[i][j]);
			if ((epDesc->bmAttributes & USB_EP_TYPE_INTR) == USB_EP_TYPE_INTR)
			{
				if ((epDesc->bEndpointAddress & USB_EP_DIR_MSK) == USB_EP_DIR_IN)
				{
					HID_Data->ep_addr   = epDesc->bEndpointAddress;
					HID_Data->length    = epDesc->wMaxPacketSize;
					HID_Data->poll      = epDesc->bInterval;
					if (HID_Data->poll < HID_MIN_POLL) {
						HID_Data->poll = HID_MIN_POLL;
					}
					HID_Data->HIDIntInEp = (epDesc->bEndpointAddress);
					HID_Data->hc_num_in = -1;
					#ifndef USBH_HID_ENABLE_DYNAMIC_HC_ALLOC
					if (USBH_Open_Channel  (pcore,
											&(HID_Data->hc_num_in),
											epDesc->bEndpointAddress,
											pdev->device_prop.address,
											pdev->device_prop.speed,
											EP_TYPE_INTR,
											HID_Data->length) < 0)
					{
						dbg_printf(DBGMODE_DEBUG, "Unable to allocate Inter-In EP, intf: %d, ep: 0x%02X \r\n", i, epDesc->bEndpointAddress);
					}
					else
					{
						dbg_printf(DBGMODE_DEBUG, "Inter-In EP allocated, intf: %d, ep: 0x%02X, len: %d\r\n", i, epDesc->bEndpointAddress, epDesc->wMaxPacketSize);
					}
					#endif
				}
				else
				{
					// do we really want to allocate H2D? we don't need these most of the time
					continue;

					HID_Data->HIDIntOutEp = (epDesc->bEndpointAddress);
					USBH_Open_Channel  (pcore,
										&(HID_Data->hc_num_out),
										epDesc->bEndpointAddress,
										pdev->device_prop.address,
										pdev->device_prop.speed,
										EP_TYPE_INTR,
										HID_Data->length);
				}
			}
		}

		HID_Data->start_toggle = 0;

		USBH_Status status;

		status = USBH_Get_HID_Descriptor (pcore, pdev, i);
		if (status == USBH_OK) {
			USBH_ParseHIDDesc(&(HID_Data->HID_Desc), pcore->host.Rx_Buffer);
			vcp_printf("V%04XP%04XA%dI%d:C%d:HIDDesc:\r\n", pdev->device_prop.Dev_Desc.idVendor, pdev->device_prop.Dev_Desc.idProduct, pdev->device_prop.address, i, USB_HID_DESC_SIZE);
			for (uint8_t k = 0; k < USB_HID_DESC_SIZE; k++) {
				vcp_printf(" 0x%02X", pcore->host.Rx_Buffer[k]);
			}
			vcp_printf("\r\n");
		}
		else {
			dbg_printf(DBGMODE_ERR, "USBH_Get_HID_Descriptor failed status: 0x%04X\r\n", status);
			USBH_ErrorHandle(pcore, pdev, status);
			continue;
		}

		uint8_t repIdList[8] = { 0, 0, 0, 0, 0, 0, 0, 0};

		status = USBH_Get_HID_ReportDescriptor(pcore , pdev, i, HID_Data->HID_Desc.wItemLength);
		if (status == USBH_OK) {
			dbg_printf(DBGMODE_DEBUG, "USBH_Get_HID_ReportDescriptor OK: 0x%04X\r\n", status);

			vcp_printf("V%04XP%04XA%dI%d:C%d:ReptDesc:", pdev->device_prop.Dev_Desc.idVendor, pdev->device_prop.Dev_Desc.idProduct, pdev->device_prop.address, i, HID_Data->HID_Desc.wItemLength);
			for (uint8_t k = 0; k < HID_Data->HID_Desc.wItemLength; k++) {
				vcp_printf(" 0x%02X", pcore->host.Rx_Buffer[k]);
			}
			vcp_printf("\r\n");

			HID_Rpt_Desc_Parse(pcore->host.Rx_Buffer, HID_Data->HID_Desc.wItemLength, &HID_Data->parserParams, i, repIdList);
			//dbg_printf(DBGMODE_DEBUG, "HID_Rpt_Desc_Parse Complete \r\n");
			HID_Rep_Parsing_Params_Debug_Dump(&(HID_Data->parserParams));
		}
		else {
			dbg_printf(DBGMODE_ERR, "USBH_Get_HID_ReportDescriptor failed status: 0x%04X\r\n", status);
			USBH_ErrorHandle(pcore, pdev, status);
			continue;
		}

		for (int ridIdx = 0; ridIdx < 8; ridIdx++)
		{
			uint8_t rid = repIdList[ridIdx];
			status = USBH_Set_Idle (pcore, pdev, i, 0, rid);
			if (status == USBH_OK) {
				dbg_printf(DBGMODE_DEBUG, "USBH_Set_Idle[%d] OK: 0x%04X\r\n", rid, status);
			}
			else if(status == USBH_NOT_SUPPORTED) {
				dbg_printf(DBGMODE_DEBUG, "USBH_Set_Idle[%d] NOT SUPPORTED\r\n", rid);
			}
			else {
				dbg_printf(DBGMODE_ERR, "USBH_Set_Idle[%d] failed status: 0x%04X\r\n", rid, status);
				USBH_ErrorHandle(pcore, pdev, status);
			}

			// end of list
			if (rid == 0) {
				break;
			}
		}

		if (pdev->device_prop.Dev_Desc.idVendor == SONY_VID && pdev->device_prop.Dev_Desc.idProduct == DUALSHOCK4_PID)
		{
			// TODO
		}
		else
		{
			status = USBH_Set_Protocol(pcore, pdev, i, 0); // this sets the protocol = "report"
			if (status == USBH_OK) {
				dbg_printf(DBGMODE_DEBUG, "USBH_Set_Protocol OK: 0x%04X\r\n", status);
			}
			else {
				dbg_printf(DBGMODE_ERR, "USBH_Set_Protocol failed status: 0x%04X\r\n", status);
				USBH_ErrorHandle(pcore, pdev, status);
			}
		}
	}

	if (isHid == 0) {
		// none of the interfaces are HID
		free(pdev->Usr_Data);
		pdev->Usr_Data = 0;
		pdev->cb = &USBH_Dev_CB_Default; // this will cause the device to not be serviced
	}
}

void USBH_Dev_HID_DeviceNotSupported(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev)
{

}

void USBH_Dev_HID_UnrecoveredError(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev)
{

}

static USBH_Status USBH_HID_Handle(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t intf)
{
  volatile unsigned int syncTries = 0xFFFFFFFF;

  if (pdev->device_prop.Itf_Desc[intf].bInterfaceClass != 0x03) {
    //dbg_printf(DBGMODE_TRACE, "USBH_HID_Handle Interface %d not HID, instead: 0x%02X\r\n", intf, pdev->device_prop.Itf_Desc[intf].bInterfaceClass);
    return USBH_OK;
  }

  HID_Data_t* HID_Data = &(((HID_Data_t*)(pdev->Usr_Data))[intf]);

  #ifndef USBH_HID_ENABLE_DYNAMIC_HC_ALLOC
  if (HID_Data->hc_num_in < 0) {
    // no possible way to do anything
    return USBH_OK;
  }
  #endif

  USBH_Status status = USBH_OK;

  char toDeallocate = 0;

  switch (HID_Data->state)
  {

  case HID_IDLE:
    HID_Data->cb->Init(pcore, pdev, intf);
    HID_Data->state = HID_SYNC;

  case HID_SYNC:

    /* Sync with start of Even Frame */
      HID_Data->state = HID_GET_DATA;
      dbg_printf(DBGMODE_TRACE, "HID state machine (dev %d intf %d) entered HID_GET_DATA \r\n", pdev->device_prop.address, intf);
      USBH_Dev_Has_HID = 1;

      char isDevRoot = pdev->Parent == 0;
		if (isDevRoot == 0) {
			// since this device now has an address
			// we can let the hub handle new devices on another port
			USBH_DEV* parentHub = (USBH_DEV*)pdev->Parent;
			Hub_Data_t* hubData = (Hub_Data_t*)parentHub->Usr_Data;
			hubData->port_busy = 0;
		}
		// we are ready to do interrupt transfers, we do not need the control EPs anymore
		// free them so other devices can use them
		USBH_Free_Channel(pcore, &(pdev->Control.hc_num_in));
		USBH_Free_Channel(pcore, &(pdev->Control.hc_num_out));
		HID_Data->state = HID_GET_DATA;
	break;

  case HID_GET_DATA:

    #ifdef USBH_HID_ENABLE_DYNAMIC_HC_ALLOC
    if (HID_Data->hc_num_in < 0)
    {
		USBH_Open_Channel (pcore,
						&(HID_Data->hc_num_in),
						HID_Data->HIDIntInEp,
						pdev->device_prop.address,
						pdev->device_prop.speed,
						EP_TYPE_INTR,
						HID_Data->length);
    }

    if (HID_Data->hc_num_in >= 0) {
    #endif


      if (USB_OTG_IsEvenFrame(pcore) != 0)
      {
		  USBH_InterruptReceiveData(pcore,
									HID_Data->buff,
									HID_Data->length,
									HID_Data->hc_num_in);

		  HID_Data->start_toggle = 1;

		  HID_Data->state = HID_POLL;
		  HID_Data->timer = HCD_GetCurrentFrame(pcore);

		  dbgwdg_feed();
      }
    #ifdef USBH_HID_ENABLE_DYNAMIC_HC_ALLOC
    }
    else {
      // failed to allocate
      dbg_printf(DBGMODE_ERR, "Unable to allocate channel for HID dev (addr %d, intf %d, ep 0x%02X) \r\n", pdev->device_prop.address, intf, HID_Data->HIDIntInEp);
      HID_Data->hc_num_in = -1;
    }
    #endif
    break;

  case HID_POLL:
    if(( HCD_GetCurrentFrame(pcore) - HID_Data->timer) >= HID_Data->poll)
    {
      HID_Data->state = HID_GET_DATA;
      toDeallocate = 1;
    }
    else if(HCD_GetURB_State(pcore , HID_Data->hc_num_in) == URB_DONE)
    {
      if(HID_Data->start_toggle != 0) /* handle data once */
      {
        HID_Data->start_toggle = 0;

        uint8_t len = pcore->host.hc[HID_Data->hc_num_in].xfer_count;

        vcp_printf("V%04XP%04XA%dEP%02X:C%d:IntIN:", pdev->device_prop.Dev_Desc.idVendor, pdev->device_prop.Dev_Desc.idProduct, pdev->device_prop.address, HID_Data->HIDIntInEp, len);
        for (uint8_t k = 0; k < len; k++) {
          vcp_printf(" 0x%02X", HID_Data->buff[k]);
        }
        vcp_printf("\r\n");

        HID_Data->cb->Decode(pcore, pdev, intf, HID_Data->HIDIntInEp, HID_Data->buff, len);
        toDeallocate = 1;
      }
    }
    else if(HCD_GetURB_State(pcore, HID_Data->hc_num_in) == URB_NOTREADY) // NAK
    {
      toDeallocate = 1;
    }
    else if(HCD_GetURB_State(pcore, HID_Data->hc_num_in) == URB_STALL) // Stalled
    {
      HID_Data->poll += 10;

      /*
      // Issue Clear Feature on interrupt IN endpoint
      if( (USBH_ClrFeature_Blocking(pcore,
                           pdev,
                           HID_Data->ep_addr,
                           HID_Data->hc_num_in)) == USBH_OK)
      //*/
      {
        // Change state to issue next IN token
        HID_Data->state = HID_GET_DATA;
        toDeallocate = 1;
      }
    }

    #ifdef USBH_HID_ENABLE_DYNAMIC_HC_ALLOC
    if (toDeallocate != 0)
    {
        USBH_Free_Channel(pcore, &(HID_Data->hc_num_in));
    }
    #endif
    break;

  default:
    dbg_printf(DBGMODE_ERR, "HID_Handle (dev_addr %d, intf %d) unknown HID_Data->state 0x%02X \r\n", pdev->device_prop.address, intf, HID_Data->state);
    break;
  }
  return status;
}

static USBH_Status USBH_Get_HID_ReportDescriptor (USB_OTG_CORE_HANDLE *pcore,
                                                  USBH_DEV *pdev,
                                                  uint16_t intf,
                                                  uint16_t length)
{
  USBH_Status status;

  status = USBH_GetDescriptor_Blocking(pcore,
                              pdev,
                              USB_REQ_RECIPIENT_INTERFACE
                                | USB_REQ_TYPE_STANDARD,
                                USB_DESC_HID_REPORT,
                                intf,
                                pcore->host.Rx_Buffer,
                                length,
                                200);

  /* HID report descriptor is available in pcore->host.Rx_Buffer.
  In case of USB Boot Mode devices for In report handling ,
  HID report descriptor parsing is not required.
  In case, for supporting Non-Boot Protocol devices and output reports,
  user may parse the report descriptor*/


  return status;
}

static USBH_Status USBH_Get_HID_Descriptor (USB_OTG_CORE_HANDLE *pcore,
                                            USBH_DEV *pdev,
                                            uint8_t intf)
{

  USBH_Status status;

  status = USBH_GetDescriptor_Blocking(pcore,
                              pdev,
                              USB_REQ_RECIPIENT_INTERFACE
                                | USB_REQ_TYPE_STANDARD,
                                USB_DESC_HID,
                                intf,
                                pcore->host.Rx_Buffer,
                                USB_HID_DESC_SIZE,
                                200);

  return status;
}

static USBH_Status USBH_Set_Idle (USB_OTG_CORE_HANDLE *pcore,
                                  USBH_DEV *pdev,
                                  uint8_t intf,
                                  uint8_t duration,
                                  uint8_t reportId)
{

  pdev->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_RECIPIENT_INTERFACE |\
    USB_REQ_TYPE_CLASS;


  pdev->Control.setup.b.bRequest = USB_HID_SET_IDLE;
  pdev->Control.setup.b.wValue.w = (duration << 8 ) | reportId;

  pdev->Control.setup.b.wIndex.w = intf;
  pdev->Control.setup.b.wLength.w = 0;

  return USBH_CtlReq_Blocking(pcore, pdev, 0 , 0 , 200);
}

USBH_Status USBH_Set_Report (USB_OTG_CORE_HANDLE *pcore,
                                 USBH_DEV *pdev,
                                    uint8_t intf,
                                    uint8_t reportType,
                                    uint8_t reportId,
                                    uint8_t reportLen,
                                    uint8_t* reportBuff)
{

  pdev->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_RECIPIENT_INTERFACE |\
    USB_REQ_TYPE_CLASS;


  pdev->Control.setup.b.bRequest = USB_HID_SET_REPORT;
  pdev->Control.setup.b.wValue.w = (reportType << 8 ) | reportId;

  pdev->Control.setup.b.wIndex.w = intf;
  pdev->Control.setup.b.wLength.w = reportLen;

  return USBH_CtlReq_Blocking(pcore, pdev, reportBuff , reportLen , 200);
}

static USBH_Status USBH_Set_Protocol(USB_OTG_CORE_HANDLE *pcore,
                                     USBH_DEV *pdev,
                                     uint8_t intf,
                                     uint8_t protocol)
{


  pdev->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_RECIPIENT_INTERFACE |\
    USB_REQ_TYPE_CLASS;


  pdev->Control.setup.b.bRequest = USB_HID_SET_PROTOCOL;

  if(protocol != 0)
  {
    /* Boot Protocol */
    pdev->Control.setup.b.wValue.w = 0;
  }
  else
  {
    /*Report Protocol*/
    pdev->Control.setup.b.wValue.w = 1;
  }

  pdev->Control.setup.b.wIndex.w = intf;
  pdev->Control.setup.b.wLength.w = 0;

  return USBH_CtlReq_Blocking(pcore, pdev, 0 , 0 , 200);
}

static void  USBH_ParseHIDDesc (USBH_HIDDesc_TypeDef *desc, uint8_t *buf)
{
	desc->bLength                  = *(uint8_t  *) (buf + 0);
	desc->bDescriptorType          = *(uint8_t  *) (buf + 1);
	desc->bcdHID                   =  LE16  (buf + 2);
	desc->bCountryCode             = *(uint8_t  *) (buf + 4);
	desc->bNumDescriptors          = *(uint8_t  *) (buf + 5);
	desc->bReportDescriptorType    = *(uint8_t  *) (buf + 6);
	desc->wItemLength              =  LE16  (buf + 7);
}
