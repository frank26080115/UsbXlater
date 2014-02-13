#include "usbh_dev_hid.h"
#include "usbh_dev_cb_default.h"
#include "usbh_dev_hub.h"
#include "usbh_dev_manager.h"
#include "usbh_devio_manager.h"
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
static USBH_Status USBH_Get_HID_ReportDescriptor_Blocking (USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint16_t intf, uint16_t length);
static USBH_Status USBH_Get_HID_Descriptor_Blocking (USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t intf);
static USBH_Status USBH_Set_Idle_Blocking (USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t intf, uint8_t duration, uint8_t reportId);
static USBH_Status USBH_Set_Protocol_Blocking (USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t intf, uint8_t protocol);
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

char USBH_Dev_HID_Cnt;

void HID_Default_Data_Handler(void* p_io, uint8_t* data, uint16_t len)
{
	USBH_DevIO_t* p_in = (USBH_DevIO_t*)p_io;

	vcp_printf("%s:C%d:IntIN:", USBH_Dev_DebugPrint(p_in->pdev, p_in->ep), len);
	for (uint8_t k = 0; k < len; k++) {
		vcp_printf(" 0x%02X", data[k]);
	}
	vcp_printf("\r\n");

	dbg_printf(DBGMODE_DEBUG, "%s:C%d:IntIN:\r\n", USBH_Dev_DebugPrint(p_in->pdev, p_in->ep), len);

	HID_Rpt_Parsing_Params_t* parser = (HID_Rpt_Parsing_Params_t*)(p_in->extra);

	if (parser->mouse_exists > 0 && parser->mouse_ep == p_in->ep->bEndpointAddress && (parser->mouse_report_id <= 0 || parser->mouse_report_id == data[0]))
	{
		if (parser->mouse_report_id > 0) { data = &data[1]; len--; }
		kbm2c_handleMouseReport(data, len, parser);
	}
	if (parser->kb_exists > 0 && parser->kb_ep == p_in->ep->bEndpointAddress && (parser->kb_report_id <= 0 || parser->kb_report_id == data[0]))
	{
		if (parser->kb_report_id > 0) { data = &data[1]; len--; }
		kbm2c_handleKeyReport(data[0], &data[2], len - 2);
	}
}

void USBH_Dev_HID_DeInitDev(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev)
{
	HID_Data_t* HID_Data = pdev->Usr_Data;
	uint8_t numIntf = pdev->device_prop.Cfg_Desc.bNumInterfaces;
	if (pdev->Usr_Data != 0)
	{
		if (HID_Data->io != 0)
		{
			for (int i = 0; i < numIntf; i++)
			{
				if (HID_Data->io[i] != 0 && HID_Data->io[i]->extra != 0) {
					free(HID_Data->io[i]->extra);
				}
			}
			free(HID_Data->io);
			HID_Data->io = 0;
		}
		free(pdev->Usr_Data);
		pdev->Usr_Data = 0;
	}
	USBH_DevIO_Manager_Unplug(pdev);
}

USBH_Status USBH_Dev_HID_Task(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev)
{
	HID_Data_t* HID_Data = pdev->Usr_Data;
	if (HID_Data->custom_task != 0) {
		HID_Data->custom_task(pcore, pdev);
	}
	uint8_t numIntf = pdev->device_prop.Cfg_Desc.bNumInterfaces;
	if (numIntf == 0) {
		dbg_printf(DBGMODE_ERR, "Warning: HID_Task device with 0 interfaces! \r\n");
	}
	if (HID_Data->ioIdx >= numIntf) {
		HID_Data->ioIdx = 0;
	}
	USBH_DevIO_t* p_in = HID_Data->io[HID_Data->ioIdx];
	if (p_in == 0) {
		HID_Data->ioIdx++;
		return USBH_OK;
	}
	USBH_DevIO_Task(p_in);
	if (p_in->state == UIOSTATE_NEXT) {
		HID_Data->ioIdx++;
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
	HID_Data_t* HID_Data = pdev->Usr_Data;
	if (HID_Data->deinit_handler != 0) {
		HID_Data->deinit_handler(pcore, pdev);
	}
	USBH_Dev_HID_Cnt--;
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
		free(pdev->Usr_Data);
		pdev->Usr_Data = 0;
	}

	uint8_t numIntf = pdev->device_prop.Cfg_Desc.bNumInterfaces;
	char isHid = 0;
	pdev->Usr_Data = calloc(1, sizeof(HID_Data_t));
	HID_Data_t* HID_Data = pdev->Usr_Data;
	HID_Data->io = calloc(numIntf, sizeof(USBH_DevIO_t*));
	HID_Data->ioIdx = 0;

	dbg_printf(DBGMODE_DEBUG, "USBH_Dev_HID_EnumerationDone, bNumInterfaces: %d\r\n", numIntf);

	for (int i = 0; i < numIntf; i++)
	{
		// filter out non-HID class interfaces
		if (pdev->device_prop.Itf_Desc[i].bInterfaceClass != 0x03) {
			continue;
		}

		isHid = 1;

		USBH_DevD2H_DataHandler_t dataHandler = 0;
		HID_Rpt_Parsing_Params_t* parser = 0;
		uint8_t parserEp = 0;

		if (pdev->device_prop.Dev_Desc.idVendor == SONY_VID && pdev->device_prop.Dev_Desc.idProduct == DUALSHOCK3_PID) {
			// DUALSHOCK3
			dataHandler = USBH_DS3_Data_Handler;
			dbg_printf(DBGMODE_TRACE, "Dualshock 3 Detected\r\n");
		}
		else if (pdev->device_prop.Dev_Desc.idVendor == SONY_VID && pdev->device_prop.Dev_Desc.idProduct == DUALSHOCK4_PID) {
			// DUALSHOCK4
			dataHandler = USBH_DS4_Data_Handler;
			HID_Data->init_handler = USBH_DS4_Init_Handler;
			HID_Data->deinit_handler = USBH_DS4_DeInit_Handler;
			HID_Data->custom_task = USBH_DS4_Task;
			dbg_printf(DBGMODE_TRACE, "Dualshock 4 Detected\r\n");
		}
		// TODO handle others, like Xbox controllers
		else {
			// other, probably keyboard or mouse
			parser = malloc(sizeof(HID_Rpt_Parsing_Params_t));
			dataHandler = HID_Default_Data_Handler;
		}

		uint8_t maxEP = ( (pdev->device_prop.Itf_Desc[i].bNumEndpoints <= USBH_MAX_NUM_ENDPOINTS) ?
						pdev->device_prop.Itf_Desc[i].bNumEndpoints :
						USBH_MAX_NUM_ENDPOINTS);

		// assign interrupt endpoints to channels, and polling parameters for the D2H endpoint
		// warning: assume 1 interrupt endpoint per direction
		for (int j = 0; j < maxEP; j++)
		{
			USBH_EpDesc_TypeDef* epDesc = &(pdev->device_prop.Ep_Desc[i][j]);
			if ((epDesc->bmAttributes & EP_TYPE_MSK) == USB_EP_TYPE_INTR)
			{
				if ((epDesc->bEndpointAddress & USB_EP_DIR_MSK) == USB_EP_DIR_IN)
				{
					HID_Data->io[i] = USBH_DevIO_Manager_New(pcore, pdev, epDesc,
#ifdef USBH_HID_ENABLE_DYNAMIC_HC_ALLOC
							1
#else
							0
#endif
							,
							dataHandler, parser
					);
					parserEp = epDesc->bEndpointAddress;
				}
			}
		}

		USBH_Status status;

		if (parser != 0)
		{
			USBH_HIDDesc_TypeDef hidDesc;

			status = USBH_Get_HID_Descriptor_Blocking (pcore, pdev, i);
			if (status == USBH_OK) {
				USBH_ParseHIDDesc(&hidDesc, pcore->host.Rx_Buffer);
				vcp_printf("%sI%d:C%d:HIDDesc:\r\n", USBH_Dev_DebugPrint(pdev, 0), i, USB_HID_DESC_SIZE);
				for (uint8_t k = 0; k < USB_HID_DESC_SIZE; k++) {
					vcp_printf(" 0x%02X", pcore->host.Rx_Buffer[k]);
				}
				vcp_printf("\r\n");
			}
			else {
				dbg_printf(DBGMODE_ERR, "USBH_Get_HID_Descriptor_Blocking failed status: 0x%04X\r\n", status);
				USBH_ErrorHandle(pcore, pdev, status);
				continue;
			}

			uint8_t repIdList[8] = { 0, 0, 0, 0, 0, 0, 0, 0};

			status = USBH_Get_HID_ReportDescriptor_Blocking(pcore , pdev, i, hidDesc.wItemLength);
			if (status == USBH_OK) {
				dbg_printf(DBGMODE_DEBUG, "USBH_Get_HID_ReportDescriptor_Blocking OK: 0x%04X\r\n", status);

				vcp_printf("%sI%d:C%d:ReptDesc:", USBH_Dev_DebugPrint(pdev, 0), i, hidDesc.wItemLength);
				for (uint8_t k = 0; k < hidDesc.wItemLength; k++) {
					vcp_printf(" 0x%02X", pcore->host.Rx_Buffer[k]);
				}
				vcp_printf("\r\n");

				HID_Rpt_Parsing_Params_Reset(parser);
				HID_Rpt_Desc_Parse(pcore->host.Rx_Buffer, hidDesc.wItemLength, parser, parserEp, repIdList);
				//dbg_printf(DBGMODE_DEBUG, "HID_Rpt_Desc_Parse Complete \r\n");
				HID_Rpt_Parsing_Params_Debug_Dump(parser);
			}
			else {
				dbg_printf(DBGMODE_ERR, "USBH_Get_HID_ReportDescriptor_Blocking failed status: 0x%04X\r\n", status);
				USBH_ErrorHandle(pcore, pdev, status);
				continue;
			}

			char hasSetIdle = 0;
			for (int ridIdx = 0; ridIdx < 8; ridIdx++)
			{
				uint8_t rid = repIdList[ridIdx];
				if (rid > 0)
				{
					hasSetIdle = 1;
					status = USBH_Set_Idle_Blocking (pcore, pdev, i, 0, rid);
					if (status == USBH_OK) {
						dbg_printf(DBGMODE_DEBUG, "USBH_Set_Idle_Blocking[%d] OK: 0x%04X\r\n", rid, status);
					}
					else if(status == USBH_NOT_SUPPORTED || status == USBH_STALL) {
						dbg_printf(DBGMODE_DEBUG, "USBH_Set_Idle_Blocking[%d] NOT SUPPORTED\r\n", rid);
					}
					else {
						dbg_printf(DBGMODE_ERR, "USBH_Set_Idle_Blocking[%d] failed status: 0x%04X\r\n", rid, status);
						USBH_ErrorHandle(pcore, pdev, status);
					}
				}

				// end of list
				if (rid == 0) {
					break;
				}
			}

			if (hasSetIdle == 0)
			{
				status = USBH_Set_Idle_Blocking (pcore, pdev, i, 0, 0);
				if (status == USBH_OK) {
					dbg_printf(DBGMODE_DEBUG, "USBH_Set_Idle_Blocking[only %d] OK: 0x%04X\r\n", 0, status);
				}
				else if(status == USBH_NOT_SUPPORTED || status == USBH_STALL) {
					dbg_printf(DBGMODE_DEBUG, "USBH_Set_Idle_Blocking[only %d] NOT SUPPORTED\r\n", 0);
				}
				else {
					dbg_printf(DBGMODE_ERR, "USBH_Set_Idle_Blocking[only %d] failed status: 0x%04X\r\n", 0, status);
					USBH_ErrorHandle(pcore, pdev, status);
				}
			}
		}

		if (pdev->device_prop.Dev_Desc.idVendor == SONY_VID && pdev->device_prop.Dev_Desc.idProduct == DUALSHOCK4_PID)
		{
			// TODO
		}
		else
		{
			status = USBH_Set_Protocol_Blocking(pcore, pdev, i, 0); // this sets the protocol = "report"
			if (status == USBH_OK) {
				dbg_printf(DBGMODE_DEBUG, "USBH_Set_Protocol_Blocking OK: 0x%04X\r\n", status);
			}
			else {
				dbg_printf(DBGMODE_ERR, "USBH_Set_Protocol_Blocking failed status: 0x%04X\r\n", status);
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

	if (HID_Data->init_handler != 0) {
		HID_Data->init_handler(pcore, pdev);
	}

	USBH_Dev_HID_Cnt++;
}

void USBH_Dev_HID_DeviceNotSupported(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev)
{

}

void USBH_Dev_HID_UnrecoveredError(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev)
{

}

static USBH_Status USBH_Get_HID_ReportDescriptor_Blocking (USB_OTG_CORE_HANDLE *pcore,
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
                                length
                                );

  /* HID report descriptor is available in pcore->host.Rx_Buffer.
  In case of USB Boot Mode devices for In report handling ,
  HID report descriptor parsing is not required.
  In case, for supporting Non-Boot Protocol devices and output reports,
  user may parse the report descriptor*/

  return status;
}

static USBH_Status USBH_Get_HID_ReportDescriptor (USB_OTG_CORE_HANDLE *pcore,
                                                  USBH_DEV *pdev,
                                                  uint16_t intf,
                                                  uint16_t length)
{
  USBH_Status status;

  status = USBH_GetDescriptor(pcore,
                              pdev,
                              USB_REQ_RECIPIENT_INTERFACE | USB_REQ_TYPE_STANDARD,
                              USB_DESC_HID_REPORT,
                              intf,
                              pcore->host.Rx_Buffer,
                              length
                              );

  /* HID report descriptor is available in pcore->host.Rx_Buffer.
  In case of USB Boot Mode devices for In report handling ,
  HID report descriptor parsing is not required.
  In case, for supporting Non-Boot Protocol devices and output reports,
  user may parse the report descriptor*/

  return status;
}

static USBH_Status USBH_Get_HID_Descriptor_Blocking (USB_OTG_CORE_HANDLE *pcore,
                                                     USBH_DEV *pdev,
                                                     uint8_t intf)
{

  USBH_Status status;

  status = USBH_GetDescriptor_Blocking(pcore,
                              pdev,
                              USB_REQ_RECIPIENT_INTERFACE | USB_REQ_TYPE_STANDARD,
                              USB_DESC_HID,
                              intf,
                              pcore->host.Rx_Buffer,
                              USB_HID_DESC_SIZE
                              );

  return status;
}

static USBH_Status USBH_Get_HID_Descriptor (USB_OTG_CORE_HANDLE *pcore,
                                            USBH_DEV *pdev,
                                            uint8_t intf)
{

  USBH_Status status;

  status = USBH_GetDescriptor(pcore,
                              pdev,
                              USB_REQ_RECIPIENT_INTERFACE | USB_REQ_TYPE_STANDARD,
                              USB_DESC_HID,
                              intf,
                              pcore->host.Rx_Buffer,
                              USB_HID_DESC_SIZE
                              );

  return status;
}

static USBH_Status USBH_Set_Idle_Blocking (USB_OTG_CORE_HANDLE *pcore,
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

  return USBH_CtlReq_Blocking(pcore, pdev, 0 , 0 );
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

  return USBH_CtlReq(pcore, pdev, 0 , 0 );
}

USBH_Status USBH_Set_Report_Blocking (USB_OTG_CORE_HANDLE *pcore,
                                      USBH_DEV *pdev,
                                      uint8_t intf,
                                      uint8_t reportType,
                                      uint8_t reportId,
                                      uint8_t reportLen,
                                      uint8_t* reportBuff)
{

  pdev->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_RECIPIENT_INTERFACE | USB_REQ_TYPE_CLASS;


  pdev->Control.setup.b.bRequest = USB_HID_SET_REPORT;
  pdev->Control.setup.b.wValue.w = (reportType << 8 ) | reportId;

  pdev->Control.setup.b.wIndex.w = intf;
  pdev->Control.setup.b.wLength.w = reportLen;

  return USBH_CtlReq_Blocking(pcore, pdev, reportBuff , reportLen );
}

USBH_Status USBH_Set_Report (USB_OTG_CORE_HANDLE *pcore,
                                 USBH_DEV *pdev,
                                    uint8_t intf,
                                    uint8_t reportType,
                                    uint8_t reportId,
                                    uint8_t reportLen,
                                    uint8_t* reportBuff)
{

  pdev->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_RECIPIENT_INTERFACE | USB_REQ_TYPE_CLASS;


  pdev->Control.setup.b.bRequest = USB_HID_SET_REPORT;
  pdev->Control.setup.b.wValue.w = (reportType << 8 ) | reportId;

  pdev->Control.setup.b.wIndex.w = intf;
  pdev->Control.setup.b.wLength.w = reportLen;

  return USBH_CtlReq(pcore, pdev, reportBuff , reportLen );
}

USBH_Status USBH_Get_Report_Blocking (USB_OTG_CORE_HANDLE *pcore,
                                      USBH_DEV *pdev,
                                      uint8_t intf,
                                      uint8_t reportType,
                                      uint8_t reportId,
                                      uint8_t reportLen,
                                      uint8_t* reportBuff)
{

  pdev->Control.setup.b.bmRequestType = USB_D2H | USB_REQ_RECIPIENT_INTERFACE | USB_REQ_TYPE_CLASS;


  pdev->Control.setup.b.bRequest = USB_HID_REQ_GET_REPORT;
  pdev->Control.setup.b.wValue.w = (reportType << 8 ) | reportId;

  pdev->Control.setup.b.wIndex.w = intf;
  pdev->Control.setup.b.wLength.w = reportLen;

  return USBH_CtlReq_Blocking(pcore, pdev, reportBuff , reportLen );
}

USBH_Status USBH_Get_Report (USB_OTG_CORE_HANDLE *pcore,
                             USBH_DEV *pdev,
                             uint8_t intf,
                             uint8_t reportType,
                             uint8_t reportId,
                             uint8_t reportLen,
                             uint8_t* reportBuff)
{

  pdev->Control.setup.b.bmRequestType = USB_D2H | USB_REQ_RECIPIENT_INTERFACE |\
    USB_REQ_TYPE_CLASS;


  pdev->Control.setup.b.bRequest = USB_HID_REQ_GET_REPORT;
  pdev->Control.setup.b.wValue.w = (reportType << 8 ) | reportId;

  pdev->Control.setup.b.wIndex.w = intf;
  pdev->Control.setup.b.wLength.w = reportLen;

  return USBH_CtlReq(pcore, pdev, reportBuff , reportLen );
}

static USBH_Status USBH_Set_Protocol_Blocking(USB_OTG_CORE_HANDLE *pcore,
                                              USBH_DEV *pdev,
                                              uint8_t intf,
                                              uint8_t protocol)
{


  pdev->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_RECIPIENT_INTERFACE | USB_REQ_TYPE_CLASS;


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

  return USBH_CtlReq_Blocking(pcore, pdev, 0 , 0 );
}

static USBH_Status USBH_Set_Protocol(USB_OTG_CORE_HANDLE *pcore,
                                     USBH_DEV *pdev,
                                     uint8_t intf,
                                     uint8_t protocol)
{


  pdev->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_RECIPIENT_INTERFACE | USB_REQ_TYPE_CLASS;


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

  return USBH_CtlReq(pcore, pdev, 0 , 0 );
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
