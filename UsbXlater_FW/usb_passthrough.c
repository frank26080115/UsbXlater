/*
 * UsbXlater - by Frank Zhao
 *
 * USB Passthrough Mode
 *
 * Designed to simply relay data from the host to the device and device to the host
 * The user can monitor these packets using the serial port
 * This technique can also be used for logging, data manipulation, data injection, etc
 *
 * untested right now
 *
 */


#include "usb_passthrough.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmsis/core_cm3.h>
#include <cmsis/core_cmFunc.h>

extern USB_OTG_CORE_HANDLE USB_OTG_Core_dev;
extern USB_OTG_CORE_HANDLE USB_OTG_Core_host;
USBH_DEV* USBPT_Dev;
char USBPT_Is_Active = 0;
char USBPT_Has_Dev = 0;
uint8_t USBPT_LastSetupPacket[24];
uint8_t* USBPT_CtrlData;
uint16_t USBPT_CtrlDataLen;
uint8_t* USBPT_GeneralOutData;
uint16_t USBPT_GeneralOutDataLen;
uint8_t* USBPT_GeneralInData;
uint16_t USBPT_GeneralInDataMax;
uint16_t USBPT_GeneralInDataLen;
USBPTH_HC_EP_t USBPTH_Listeners[USBPTH_MAX_LISTENERS];
USBH_EpDesc_TypeDef** USBPTH_OutEP;
uint8_t USBPTH_OutEPCnt;
static char USBPT_printf_buffer[256];

void USBPT_Init(USBH_DEV* pdev)
{
	USBPTH_OutEPCnt = 0;
	USBPT_Is_Active = 1;
	USBPT_Dev = pdev;

	for (uint8_t i = 0; i < USBPTH_MAX_LISTENERS; i++) {
		USBPTH_Listeners[i].hc = 0;
	}

	// most of the device event handlers are spawned within ISRs
	// so we must order the priorities such that the other
	// ISRs can be serviced while still inside the device event handlers
	NVIC_SetPriority(SysTick_IRQn, 0);
	NVIC_SetPriority(USART1_IRQn, 1);
	NVIC_SetPriority(OTG_FS_IRQn, 2);
	NVIC_SetPriority(OTG_HS_IRQn, 7);

	USBH_InitCore(&USB_OTG_Core_host, USB_OTG_FS_CORE_ID);
	USBH_InitDev(&USB_OTG_Core_host, USBPT_Dev, &USBPT_Host_cb);

	USBD_Init(&USB_OTG_Core_dev, USB_OTG_HS_CORE_ID, &USBPT_Dev_cb);
	DCD_DevDisconnect(&USB_OTG_Core_dev);
	USBPT_printf("\r\n USB Passthrough Mode \r\n");
	delay_ms(2000);
}

void USBPT_Work()
{
	if (USBPT_Has_Dev == 0)
	{
		if (HCD_IsDeviceConnected(&USB_OTG_Core_host) != 0)
		{
			USBPT_printf("\r\n USBPT Device Connecting \r\n");

			USBPT_Dev->Control.hc_num_out = USBH_Alloc_Channel(&USB_OTG_Core_host, 0x00);
			USBPT_Dev->Control.hc_num_in  = USBH_Alloc_Channel(&USB_OTG_Core_host, 0x80);
			if (USBPT_Dev->Control.hc_num_out >= 0 && USBPT_Dev->Control.hc_num_in >= 0)
			{
				USBH_Open_Channel(	&USB_OTG_Core_host,
					USBPT_Dev->Control.hc_num_in,
					USBPT_Dev->device_prop.address, // still 0 at this point
					USBPT_Dev->device_prop.speed,
					EP_TYPE_CTRL,
					USBPT_Dev->Control.ep0size);
				USBH_Open_Channel(	&USB_OTG_Core_host,
					USBPT_Dev->Control.hc_num_out,
					USBPT_Dev->device_prop.address, // still 0 at this point
					USBPT_Dev->device_prop.speed,
					EP_TYPE_CTRL,
					USBPT_Dev->Control.ep0size);

				DCD_DevConnect(&USB_OTG_Core_dev);
				USBPT_Has_Dev = 1;
			}
			else
			{
				dbg_printf(DBGMODE_ERR, "\r\n USBPT Unable to allocate control EP HC \r\n");
			}
		}
		else
		{
			return;
		}
	}
	else
	{
		if (HCD_IsDeviceConnected(&USB_OTG_Core_host) == 0)
		{
			USBPT_printf("\r\n USBPT Device Disconnecting \r\n");
			USBD_DeInit(&USB_OTG_Core_dev);
			DCD_DevDisconnect(&USB_OTG_Core_dev);
			USBPT_Has_Dev = 0;

			for (uint8_t i = 0; i < USBPTH_MAX_LISTENERS; i++)
			{
				if (USBPTH_Listeners[i].hc != 0 && USBPTH_Listeners[i].hc != HC_ERROR)
				{
					USBH_Free_Channel(&USB_OTG_Core_host, USBPTH_Listeners[i].hc);
					USBPTH_Listeners[i].hc = 0;
				}
			}

			if (USBPT_Dev->Control.hc_num_out >= 0 && USBPT_Dev->Control.hc_num_in >= 0)
			{
				USBH_Free_Channel(&USB_OTG_Core_host, USBPT_Dev->Control.hc_num_out);
				USBH_Free_Channel(&USB_OTG_Core_host, USBPT_Dev->Control.hc_num_in);
				USBPT_Dev->Control.hc_num_in = -1;
				USBPT_Dev->Control.hc_num_out = -1;
			}
		}
	}

	for (uint8_t i = 0; i < USBPTH_MAX_LISTENERS; i++)
	{
		USBPTH_HC_EP_t* pl = &USBPTH_Listeners[i];
		uint8_t hc = USBPTH_Listeners[i].hc;
		if (hc != 0 && hc != HC_ERROR) // if listener is actually allocated
		{
			USBH_EpDesc_TypeDef* epDesc = pl->epDesc;
			uint8_t epnum = epDesc->bEndpointAddress;
			uint8_t epType = 0;
			USBPT_GeneralInDataLen = epDesc->wMaxPacketSize;

			// try to send read tokens only on even frames
			if (USB_OTG_IsEvenFrame(&USB_OTG_Core_host) == 0) continue;

			dbg_trace();

			// attempt to start the read, check the read type first
			if ((epDesc->bmAttributes & USB_EP_TYPE_INTR) == USB_EP_TYPE_INTR) {
				epType = EP_TYPE_INTR;
				USBH_InterruptReceiveData(&USB_OTG_Core_host, USBPT_GeneralInData, USBPT_GeneralInDataLen, hc);
			}
			else if ((epDesc->bmAttributes & USB_EP_TYPE_INTR) == USB_EP_TYPE_BULK) {
				epType = EP_TYPE_BULK;
				USBH_BulkReceiveData(&USB_OTG_Core_host, USBPT_GeneralInData, USBPT_GeneralInDataLen, hc);
			}
			else if ((epDesc->bmAttributes & USB_EP_TYPE_INTR) == USB_EP_TYPE_ISOC) {
				epType = EP_TYPE_ISOC;
				USBH_IsocReceiveData(&USB_OTG_Core_host, USBPT_GeneralInData, USBPT_GeneralInDataLen, hc);
			}

			// now we wait for a reply, or maybe there isn't one
			USBH_Status status;
			char sent = 0;
			delay_1ms_cnt = 100;
			do
			{
				URB_STATE us = HCD_GetURB_State(&USB_OTG_Core_host, hc);
				if (us == URB_DONE)
				{
					// data was indeed received

					// print it to the serial port for monitoring
					USBPT_printf("\r\n USBPT:IN:EP0x%02X:", epnum);
					for (uint16_t c = 0; c < USBPT_GeneralInDataLen; c++) {
						USBPT_printf(" 0x%02X", USBPT_GeneralInData[c]);
					}
					USBPT_printf("\r\n");

					// relay the data to the host
					DCD_EP_Tx(&USB_OTG_Core_dev, epnum, USBPT_GeneralInData, USBPT_GeneralInDataLen);
					sent = 1;
					break;
				}
				else if (us == URB_ERROR) {
					dbg_printf(DBGMODE_ERR, "\r\n DataIn Error on EP 0x%02X \r\n", epnum);
					break;
				}
				else if (us == URB_STALL) {
					dbg_printf(DBGMODE_ERR, "\r\n DataIn Stalled EP 0x%02X \r\n", epnum);
					break;
				}
				else if (us == URB_NOTREADY) {
					// NAK, no data
					break;
				}
			}
			while (delay_1ms_cnt > 0);

			if (delay_1ms_cnt == 0) {
				dbg_printf(DBGMODE_ERR, "\r\n DataIn Read Timed Out EP 0x%02X \r\n", epnum);
			}
		}
	}
}

uint8_t USBPTD_SetupStage(USB_OTG_CORE_HANDLE* pcore, USB_SETUP_REQ* req)
{
	// store for later use from another function
	memcpy(USBPT_LastSetupPacket, pcore->dev.setup_packet, 24);

	// print for monitoring
	USBPT_printf("\b\r\n USBPT:SETUP:");
	for (uint8_t i = 0; i < 8; i++) {
		USBPT_printf(" 0x%02X", USBPT_LastSetupPacket[i]);
	}
	USBPT_printf("\r\n");

	// prepare to be sent to the device
	memcpy(USBPT_Dev->Control.setup.d8, USBPT_LastSetupPacket, 8);

	// set address must be handled explicitly
	if ((req->bmRequest & 0x7F) == (USB_REQ_RECIPIENT_DEVICE | USB_REQ_TYPE_STANDARD) && req->bRequest == USB_REQ_SET_ADDRESS)
	{
		// let the internal code handle it for the device interface
		USBD_StdDevReq(pcore, req);

		// pass it to the downstream device
		USBH_CtlReq_Blocking(&USB_OTG_Core_host, USBPT_Dev, 0, 0, 100);
		USBD_CtlSendStatus(pcore);

		// modifiy our host channel to match
		USBPT_Dev->device_prop.address = (uint8_t)(req->wValue) & 0x7F;
		USBH_Modify_Channel (&USB_OTG_Core_host,
							USBPT_Dev->Control.hc_num_in,
							USBPT_Dev->device_prop.address,
							0,
							0,
							0);
		USBH_Modify_Channel (&USB_OTG_Core_host,
							USBPT_Dev->Control.hc_num_out,
							USBPT_Dev->device_prop.address,
							0,
							0,
							0);

		// modify all other channels to match
		for (uint8_t i = 0; i < USBPTH_MAX_LISTENERS; i++)
		{
			USBPTH_HC_EP_t* pl = &USBPTH_Listeners[i];
			uint8_t hc = pl->hc;
			if (hc != 0 && hc != HC_ERROR) // if listener is actually allocated
			{
				USBH_EpDesc_TypeDef* epDesc = pl->epDesc;
				uint8_t epType = 0;
				if ((epDesc->bmAttributes & USB_EP_TYPE_INTR) == USB_EP_TYPE_INTR) {
					epType = EP_TYPE_INTR;
				}
				else if ((epDesc->bmAttributes & USB_EP_TYPE_INTR) == USB_EP_TYPE_BULK) {
					epType = EP_TYPE_BULK;
				}
				else if ((epDesc->bmAttributes & USB_EP_TYPE_INTR) == USB_EP_TYPE_ISOC) {
					epType = EP_TYPE_ISOC;
				}
				else if ((epDesc->bmAttributes & USB_EP_TYPE_INTR) == USB_EP_TYPE_CTRL) {
					epType = EP_TYPE_CTRL;
				}

				USBH_Modify_Channel(	&USB_OTG_Core_host,
										USBPTH_Listeners[i].hc,
										USBPT_Dev->device_prop.address,
										USBPT_Dev->device_prop.speed,
										epType,
										USBPTH_Listeners[i].epDesc->wMaxPacketSize);
			}
		}
		// note: out direction channels are dynamically allocated only when needed
		// so we don't need to modify those channel addresses

		return USBD_OK;
	}

	// no data means we can just directly relay the data
	if (req->wLength == 0) {
		USBH_CtlReq_Blocking(&USB_OTG_Core_host, USBPT_Dev, 0, 0, 100);
		USBD_CtlSendStatus(pcore);
		return USBD_OK;
	}

	// there is extra data later
	USBPT_CtrlDataLen = req->wLength;
	if (USBPT_CtrlData != 0) free(USBPT_CtrlData);
	USBPT_CtrlData = malloc(USBPT_CtrlDataLen);

	USBH_Status status;

	// wait until previous req is finished
	delay_1ms_cnt = 100;
	while (delay_1ms_cnt > 0 &&
			USBPT_Dev->Control.state != CTRL_COMPLETE &&
			USBPT_Dev->Control.state != CTRL_IDLE &&
			USBPT_Dev->Control.state != CTRL_ERROR &&
			USBPT_Dev->Control.state != CTRL_STALLED);
	{
		status = USBH_HandleControl(&USB_OTG_Core_host, USBPT_Dev);
	}

	// finalize previous ctrl req
	if (USBPT_Dev->RequestState == CMD_WAIT) {
		USBH_CtlReq(&USB_OTG_Core_host, USBPT_Dev, 0 , 0 );
	}

	// prepare new setup
	USBH_SubmitSetupRequest(USBPT_Dev, USBPT_CtrlData, USBPT_CtrlDataLen);
	USBPT_Dev->RequestState = CMD_WAIT;
	USBH_CtlSendSetup (&USB_OTG_Core_host, USBPT_Dev->Control.setup.d8, USBPT_Dev->Control.hc_num_out);
	USBPT_Dev->Control.state = CTRL_SETUP_WAIT;
	USBPT_Dev->Control.timer = HCD_GetCurrentFrame(pcore);
	USBPT_Dev->Control.timeout = 50;

	if ((req->bmRequest & 0x80) == 0)
	{ // H2D
		// we need to obtain the data from EP0_RxReady first
		USBD_CtlPrepareRx (pcore, USBPT_CtrlData, USBPT_CtrlDataLen);
		return USBD_OK;
	}
	else
	{ // D2H

		// wait for request to finish
		delay_1ms_cnt = 100;
		do
		{
			status = USBH_CtlReq(&USB_OTG_Core_host, USBPT_Dev, USBPT_CtrlData , USBPT_CtrlDataLen );
			if (status == USBH_OK || status == USBH_FAIL || status == USBH_STALL || status == USBH_NOT_SUPPORTED) {
				break;
			}
			else
			{
				status = USBH_HandleControl(&USB_OTG_Core_host, USBPT_Dev);
				if (status == USBH_FAIL || status == USBH_STALL || status == USBH_NOT_SUPPORTED) {
					break;
				}
			}
		}
		while (delay_1ms_cnt > 0);

		if (delay_1ms_cnt == 0)
		{
			// timeout
			dbg_printf(DBGMODE_ERR, "\r\n USBPT Setup Timed Out \r\n");
			USBD_CtlSendStatus(pcore); // we reply with nothing to simulate a timeout
			return USBH_OK;
		}
		else if (status == USBH_OK)
		{
			// all good, send back the data
			USBD_CtlSendData (pcore, USBPT_CtrlData, USBPT_CtrlDataLen);

			// handle config descriptors specially, we need to know what channels to open based on endpoints
			if ((req->bmRequest & 0x7F) == (USB_REQ_RECIPIENT_DEVICE | USB_REQ_TYPE_STANDARD) &&
					req->bRequest == USB_REQ_GET_DESCRIPTOR &&
					req->wValue == USB_DESC_CONFIGURATION &&
					req->wLength > USB_CONFIGURATION_DESC_SIZE)
			{
				// this is a full length configuration descriptor
				// we need this info to open as many D2H endpoints to channels
				USBH_ParseCfgDesc(&USBPT_Dev->device_prop.Cfg_Desc,
									USBPT_Dev->device_prop.Itf_Desc,
									USBPT_Dev->device_prop.Ep_Desc,
									USBPT_CtrlData,
									USBPT_CtrlDataLen);

				USBPTH_OutEPCnt = 0;
				USBPT_GeneralInDataLen = 0;
				for (uint8_t i = 0; i < USBPT_Dev->device_prop.Cfg_Desc.bNumInterfaces && i < USBH_MAX_NUM_INTERFACES; i++)
				{
					for (uint8_t j = 0; j < USBPT_Dev->device_prop.Itf_Desc[i].bNumEndpoints && j < USBH_MAX_NUM_ENDPOINTS; j++)
					{
						USBH_EpDesc_TypeDef* epDesc = &USBPT_Dev->device_prop.Ep_Desc[i][j];
						for (uint8_t k = 0; k < USBPTH_MAX_LISTENERS; k++)
						{
							if ((epDesc->bEndpointAddress & USB_EP_DIR_MSK) == USB_D2H && USBPTH_Listeners[k].hc == 0)
							{
								USBPTH_Listeners[k].epDesc = epDesc;
								USBPTH_Listeners[k].hc = USBH_Alloc_Channel(&USB_OTG_Core_host, epDesc->bEndpointAddress);
								if (USBPTH_Listeners[k].hc == HC_ERROR)
								{
									USBPTH_Listeners[k].hc = 0;
								}
								else if (USBPTH_Listeners[k].hc != 0)
								{

									uint8_t epType = 0;
									if ((epDesc->bmAttributes & USB_EP_TYPE_INTR) == USB_EP_TYPE_INTR) {
										epType = EP_TYPE_INTR;
									}
									else if ((epDesc->bmAttributes & USB_EP_TYPE_INTR) == USB_EP_TYPE_BULK) {
										epType = EP_TYPE_BULK;
									}
									else if ((epDesc->bmAttributes & USB_EP_TYPE_INTR) == USB_EP_TYPE_ISOC) {
										epType = EP_TYPE_ISOC;
									}
									else if ((epDesc->bmAttributes & USB_EP_TYPE_INTR) == USB_EP_TYPE_CTRL) {
										epType = EP_TYPE_CTRL;
									}

									USBH_Open_Channel(	&USB_OTG_Core_host,
														USBPTH_Listeners[k].hc,
														USBPT_Dev->device_prop.address,
														USBPT_Dev->device_prop.speed,
														epType,
														USBPTH_Listeners[k].epDesc->wMaxPacketSize);

									DCD_EP_Open(&USB_OTG_Core_dev, epDesc->bEndpointAddress, epDesc->wMaxPacketSize, epType);

									if (epDesc->wMaxPacketSize > USBPT_GeneralInDataMax) {
										USBPT_GeneralInDataMax = epDesc->wMaxPacketSize;
									}
								}
							}
						}

						if ((epDesc->bEndpointAddress & 0x80) == USB_H2D)
						{
							USBPTH_OutEPCnt++;
						}
					}
				}

				if (USBPT_GeneralInData != 0) free(USBPT_GeneralInData); // release memory if previously allocated
				USBPT_GeneralInData = malloc(USBPT_GeneralInDataMax); // only allocate the memory we need

				if (USBPTH_OutEP != 0) free(USBPTH_OutEP); // release memory if previously allocated
				USBPTH_OutEP = malloc(sizeof(USBH_EpDesc_TypeDef*) * USBPTH_OutEPCnt); // only allocate the memory we need

				uint8_t ec = 0;
				for (uint8_t i = 0; i < USBPT_Dev->device_prop.Cfg_Desc.bNumInterfaces && i < USBH_MAX_NUM_INTERFACES; i++)
				{
					for (uint8_t j = 0; j < USBPT_Dev->device_prop.Itf_Desc[i].bNumEndpoints && j < USBH_MAX_NUM_ENDPOINTS; j++)
					{
						USBH_EpDesc_TypeDef* epDesc = &USBPT_Dev->device_prop.Ep_Desc[i][j];
						if ((epDesc->bEndpointAddress & 0x80) == USB_H2D) {
							// only save the H2D direction endpoints
							USBPTH_OutEP[ec] = epDesc;
							ec++;
						}
					}
				}
			}
			return USBH_OK;
		}
		else
		{
			if (status == USBH_STALL || status == USBH_NOT_SUPPORTED) {
				dbg_printf(DBGMODE_ERR, "\r\n USBPT Setup Stalled \r\n");
				USBD_CtlError(pcore , req);
				return USBH_OK;
			}
		}

		return USBD_OK;
	}

	dbg_printf(DBGMODE_ERR, "\r\n USBPT Setup Unhandled Error \r\n");
	USBD_CtlError(pcore , req);

	return USBD_OK;
}

void USBPTH_DeInitDev(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev)
{
}

USBH_Status USBPTH_Task(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev)
{
	return USBH_OK;
}

void USBPTH_Init(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev)
{
}

void USBPTH_DeInit(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev)
{
}

void USBPTH_DeviceAttached(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev)
{
}

void USBPTH_ResetDevice(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev)
{
}

void USBPTH_DeviceDisconnected(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev)
{
}

void USBPTH_OverCurrentDetected(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev)
{
}

void USBPTH_DeviceSpeedDetected(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev, uint8_t DeviceSpeed)
{
}

void USBPTH_DeviceDescAvailable(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev, void * data)
{
}

void USBPTH_DeviceAddressAssigned(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev)
{
}

void USBPTH_ConfigurationDescAvailable(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev,
								 USBH_CfgDesc_TypeDef * config,
								 USBH_InterfaceDesc_TypeDef * intf,
								 USBH_EpDesc_TypeDef ** epDesc)
{
}

void USBPTH_ManufacturerString(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev, void * data)
{
}

void USBPTH_ProductString(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev, void * data)
{
}

void USBPTH_SerialNumString(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev, void * data)
{
}

void USBPTH_EnumerationDone(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev)
{
}

void USBPTH_DeviceNotSupported(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev)
{
}

void USBPTH_UnrecoveredError(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev)
{
}

uint8_t USBPTD_ClassInit         (void *pcore , uint8_t cfgidx)
{
	return USBD_OK;
}

uint8_t USBPTD_ClassDeInit       (void *pcore , uint8_t cfgidx)
{
	return USBD_OK;
}

uint8_t USBPTD_Setup             (void *pcore , USB_SETUP_REQ  *req)
{
	return USBD_OK;
}

uint8_t USBPTD_EP0_TxSent        (void *pcore )
{
	return USBD_OK;
}

uint8_t USBPTD_DataIn            (void *pcore , uint8_t epnum)
{
	return USBD_OK;
}

uint8_t USBPTD_DataOut           (void *pcore , uint8_t epnum)
{
	__enable_irq();

	USB_OTG_CORE_HANDLE* pcore_ = (USB_OTG_CORE_HANDLE*)pcore;
	DCD_DEV* pdev = &(pcore_->dev);
	uint8_t* data;
	uint16_t wLength;

	if (epnum == 0x00)
	{ // CTRL REQ
		wLength = USBPT_CtrlDataLen;
		data = USBPT_CtrlData;
	}
	else
	{
		wLength = pdev->out_ep[epnum].xfer_count;
		data = pdev->out_ep[epnum].xfer_buff;
	}

	// print to monitor
	USBPT_printf("\r\n USBPT:OUT:EP0x%02X:", epnum);
	for (uint16_t i = 0; i < wLength; i++) {
		USBPT_printf(" 0x%02X", data[i]);
	}
	USBPT_printf("\r\n");

	if (epnum == 0x00)
	{ // CTRL REQ

		USBH_Status status;
		delay_1ms_cnt = 100;

		// wait for transfer complete
		do
		{
			status = USBH_CtlReq(&USB_OTG_Core_host, USBPT_Dev, USBPT_CtrlData , USBPT_CtrlDataLen );
			if (status == USBH_OK || status == USBH_FAIL || status == USBH_STALL || status == USBH_NOT_SUPPORTED) {
				break;
			}
			else
			{
				status = USBH_HandleControl(&USB_OTG_Core_host, USBPT_Dev);
				if (status == USBH_FAIL || status == USBH_STALL || status == USBH_NOT_SUPPORTED) {
					break;
				}
			}
		}
		while (delay_1ms_cnt > 0);

		if (delay_1ms_cnt == 0) {
			dbg_printf(DBGMODE_ERR, "\r\n USBPTD_DataOut timed out while sending to device, status: 0x%04X \r\n", status);
			USBD_CtlError(pcore , 0);
			return USBD_FAIL;
		}
		else if (status != USBH_OK) {
			dbg_printf(DBGMODE_ERR, "\r\n USBPTD_DataOut failed to send to device, status: 0x%04X \r\n", status);
			USBD_CtlError(pcore , 0);
			return USBD_FAIL;
		}
		else { // everything is OK
			USBD_CtlSendStatus(pcore);
			return USBD_OK;
		}
	}
	else
	{
		wLength = pdev->out_ep[epnum].xfer_count;
		data = pdev->out_ep[epnum].xfer_buff;

		// allocate memory needed
		if (USBPT_GeneralOutData != 0) free(USBPT_GeneralOutData);
		USBPT_GeneralOutDataLen = wLength;
		USBPT_GeneralOutData = malloc(wLength);
		memcpy(USBPT_GeneralOutData, data, USBPT_GeneralOutDataLen);

		USBH_EpDesc_TypeDef* epDesc = 0;
		for (uint8_t i = 0; i < USBPTH_OutEPCnt; i++)
		{
			// look for appropriate EP
			if (USBPTH_OutEP[i]->bEndpointAddress == epnum) {
				epDesc = USBPTH_OutEP[i];
				break;
			}
		}

		if (epDesc != 0) // EP found
		{
			uint8_t epType = 0;
			uint8_t hc = 0;
			if ((epDesc->bmAttributes & USB_EP_TYPE_INTR) == USB_EP_TYPE_INTR) {
				epType = EP_TYPE_INTR;
			}
			else if ((epDesc->bmAttributes & USB_EP_TYPE_INTR) == USB_EP_TYPE_BULK) {
				epType = EP_TYPE_BULK;
			}
			else if ((epDesc->bmAttributes & USB_EP_TYPE_INTR) == USB_EP_TYPE_ISOC) {
				epType = EP_TYPE_ISOC;
			}

			// dynamically allocate host channel for use
			hc = USBH_Alloc_Channel(&USB_OTG_Core_host, epnum);
			if (hc != 0 && hc != HC_ERROR)
			{
				USBH_Open_Channel(	&USB_OTG_Core_host,
									hc,
									USBPT_Dev->device_prop.address,
									USBPT_Dev->device_prop.speed,
									epType,
									epDesc->wMaxPacketSize);

				// try to only send on even frame
				volatile uint32_t syncTries = 0x7FFFFFFF;
				while (USB_OTG_IsEvenFrame(&USB_OTG_Core_host) == 0 && syncTries--) __NOP();

				// send using appropriate method
				switch (epType)
				{
					case EP_TYPE_INTR:
						USBH_InterruptSendData(&USB_OTG_Core_host, USBPT_GeneralOutData, USBPT_GeneralOutDataLen, hc);
						break;
					case EP_TYPE_BULK:
						USBH_BulkSendData(&USB_OTG_Core_host, USBPT_GeneralOutData, USBPT_GeneralOutDataLen, hc);
						break;
					case EP_TYPE_ISOC:
						USBH_IsocSendData(&USB_OTG_Core_host, USBPT_GeneralOutData, USBPT_GeneralOutDataLen, hc);
						break;
					default:
						break;
				}

				// wait until done sending
				USBH_Status status;
				delay_1ms_cnt = 100;
				do
				{
					URB_STATE us = HCD_GetURB_State(&USB_OTG_Core_host, hc);
					if (us == URB_DONE) {
						break;
					}
					else if (us == URB_ERROR) {
						dbg_printf(DBGMODE_ERR, "\r\n USBPTD_DataOut Send Error on EP 0x%02X \r\n", epnum);
						DCD_EP_Stall(pcore, epnum);
						break;
					}
					else if (us == URB_STALL) {
						dbg_printf(DBGMODE_ERR, "\r\n USBPTD_DataOut Stalled EP 0x%02X \r\n", epnum);
						DCD_EP_Stall(pcore, epnum);
						break;
					}
				}
				while (delay_1ms_cnt > 0);

				if (delay_1ms_cnt == 0) {
					dbg_printf(DBGMODE_ERR, "\r\n USBPTD_DataOut Send Timed Out EP 0x%02X \r\n", epnum);
				}

				// free the channel to be used by something else later
				USB_OTG_HC_Halt(&USB_OTG_Core_host, hc);
				USBH_Free_Channel(&USB_OTG_Core_host, hc);
			}
			else
			{
				dbg_printf(DBGMODE_ERR, "\r\n USBPTD_DataOut Failed to Alloc HC for EP 0x%02X \r\n", epnum);
			}

		}
		else
		{
			dbg_printf(DBGMODE_ERR, "\r\n USBPTD_DataOut No Such EP 0x%02X \r\n", epnum);
		}

		return USBD_OK;
	}

	return USBD_OK;
}

uint8_t USBPTD_EP0_RxReady       (void *pcore )
{
	return USBPTD_DataOut(pcore, 0x00);
}

uint8_t USBPTD_SOF               (void *pcore)
{
	return USBD_OK;
}

uint8_t USBPTD_IsoINIncomplete   (void *pcore)
{
	return USBD_OK;
}

uint8_t USBPTD_IsoOUTIncomplete  (void *pcore)
{
	return USBD_OK;
}

uint8_t* USBPTD_GetDeviceDescriptor( uint8_t speed , uint16_t *length)
{
	return 0;
}

uint8_t* USBPTD_GetConfigDescriptor( uint8_t speed , uint16_t *length)
{
	return 0;
}

uint8_t* USBPTD_GetOtherConfigDescriptor( uint8_t speed , uint16_t *length)
{
	return 0;
}

uint8_t* USBPTD_GetUsrStrDescriptor( uint8_t speed , uint8_t index , uint16_t *length)
{
	return 0;
}

uint8_t* USBPTD_GetLangIDStrDescriptor( uint8_t speed , uint16_t *length)
{
	return 0;
}

uint8_t* USBPTD_GetManufacturerStrDescriptor( uint8_t speed , uint16_t *length)
{
	return 0;
}

uint8_t* USBPTD_GetProductStrDescriptor( uint8_t speed , uint16_t *length)
{
	return 0;
}

uint8_t* USBPTD_GetSerialStrDescriptor( uint8_t speed , uint16_t *length)
{
	return 0;
}

uint8_t* USBPTD_GetConfigurationStrDescriptor( uint8_t speed , uint16_t *length)
{
	return 0;
}

uint8_t* USBPTD_GetInterfaceStrDescriptor( uint8_t intf , uint8_t speed , uint16_t *length)
{
	return 0;
}

void USBPTD_UsrInit(void)
{
}

void USBPTD_DeviceReset(uint8_t speed)
{
	USBPT_printf("\r\n USBPT Host Caused Reset \r\n");
}

void USBPTD_DeviceConfigured(void)
{
}

void USBPTD_DeviceSuspended(void)
{
}

void USBPTD_DeviceResumed(void)
{
}

void USBPTD_DeviceConnected(void)
{
	USBPT_printf("\r\n USBPT Host Connecting \r\n");
}

void USBPTD_DeviceDisconnected(void)
{
	USBPT_printf("\r\n USBPT Host Disconnecting \r\n");
}


USBH_Device_cb_TypeDef USBPT_Host_cb =
{
	USBPTH_DeInitDev,
	USBPTH_Task,
	USBPTH_Init,
	USBPTH_DeInit,
	USBPTH_DeviceAttached,
	USBPTH_ResetDevice,
	USBPTH_DeviceDisconnected,
	USBPTH_OverCurrentDetected,
	USBPTH_DeviceSpeedDetected,
	USBPTH_DeviceDescAvailable,
	USBPTH_DeviceAddressAssigned,
	USBPTH_ConfigurationDescAvailable,
	USBPTH_ManufacturerString,
	USBPTH_ProductString,
	USBPTH_SerialNumString,
	USBPTH_EnumerationDone,
	USBPTH_DeviceNotSupported,
	USBPTH_UnrecoveredError,
};

USBD_Device_cb_TypeDef USBPT_Dev_cb =
{
	USBPTD_ClassInit,
	USBPTD_ClassDeInit,
	USBPTD_Setup,
	USBPTD_EP0_TxSent,
	USBPTD_EP0_RxReady,
	USBPTD_DataIn,
	USBPTD_DataOut,
	USBPTD_SOF,
	USBPTD_IsoINIncomplete,
	USBPTD_IsoOUTIncomplete,

	USBPTD_GetDeviceDescriptor,
	USBPTD_GetConfigDescriptor,
	USBPTD_GetOtherConfigDescriptor,
	USBPTD_GetUsrStrDescriptor,
	USBPTD_GetLangIDStrDescriptor,
	USBPTD_GetManufacturerStrDescriptor,
	USBPTD_GetProductStrDescriptor,
	USBPTD_GetSerialStrDescriptor,
	USBPTD_GetConfigurationStrDescriptor,
	USBPTD_GetInterfaceStrDescriptor,

	USBPTD_UsrInit,
	USBPTD_DeviceReset,
	USBPTD_DeviceConfigured,
	USBPTD_DeviceSuspended,
	USBPTD_DeviceResumed,
	USBPTD_DeviceConnected,
	USBPTD_DeviceDisconnected,
};

void USBPT_printf(char * format, ...)
{
	int i, j;
	#ifndef va_list
	#define va_list __gnuc_va_list
	#endif

	va_list args;
	va_start(args, format);
	j = vsprintf(USBPT_printf_buffer, format, args);
	va_end(args);
	for (i = 0; i < j; i++) {
		uint8_t ch = USBPT_printf_buffer[i];
		if (ch == '\b') {
			cereal_wait();
		}
		else {
			cereal_tx(ch);
		}
	}
}
