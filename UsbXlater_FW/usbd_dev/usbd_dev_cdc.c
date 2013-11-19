#include "usbd_dev_cdc.h"
#include "usbd_dev_inc_all.h"
#include <stdlib.h>
#include <string.h>

uint8_t USBD_CDC_DeviceDesc[USB_SIZ_DEVICE_DESC] = {
	0x12,                       // bLength
	USB_DEVICE_DESCRIPTOR_TYPE, // bDescriptorType
	0x00, 0x02,                 // bcdUSB
	DEVICE_CLASS_CDC,           // bDeviceClass
	DEVICE_SUBCLASS_CDC,        // bDeviceSubClass
	0x00,                       // bDeviceProtocol
	USB_OTG_MAX_EP0_SIZE,       // bMaxPacketSize
	0x83, 0x04,                 // idVendor
	0x40, 0x57,                 // idProduct
	0x00, 0x02,                 // bcdDevice rel. 2.00
	USBD_IDX_MFC_STR,           // Index of manufacturer  string
	USBD_IDX_PRODUCT_STR,       // Index of product string
	USBD_IDX_SERIAL_STR,        // Index of serial number string
	0x01,                       // bNumConfigurations
};

uint8_t USBD_CDC_ConfigDescriptor[USB_CDC_CONFIG_DESC_SIZ] = {
	// Configuration Descriptor
	0x09,   // bLength: Configuration Descriptor size
	USB_CONFIGURATION_DESCRIPTOR_TYPE,      // bDescriptorType: Configuration
	USB_CDC_CONFIG_DESC_SIZ,                // wTotalLength:no of returned bytes
	0x00,
	0x02,   // bNumInterfaces: 2 interface
	0x01,   // bConfigurationValue: Configuration value
	0x00,   // iConfiguration: Index of string descriptor describing the configuration
	0xC0,   // bmAttributes: self powered
	0x32,   // MaxPower 0 mA

	// ---------------------------------------------------------------------------

	// Interface Descriptor
	0x09,   // bLength: Interface Descriptor size
	USB_INTERFACE_DESCRIPTOR_TYPE,  // bDescriptorType: Interface
	// Interface descriptor type
	0x00,   // bInterfaceNumber: Number of Interface
	0x00,   // bAlternateSetting: Alternate setting
	0x01,   // bNumEndpoints: One endpoints used
	0x02,   // bInterfaceClass: Communication Interface Class
	0x02,   // bInterfaceSubClass: Abstract Control Model
	0x01,   // bInterfaceProtocol: Common AT commands
	0x00,   // iInterface:

	// Header Functional Descriptor
	0x05,   // bLength: Endpoint Descriptor size
	0x24,   // bDescriptorType: CS_INTERFACE
	0x00,   // bDescriptorSubtype: Header Func Desc
	0x10,   // bcdCDC: spec release number
	0x01,

	// Call Management Functional Descriptor
	0x05,   // bFunctionLength
	0x24,   // bDescriptorType: CS_INTERFACE
	0x01,   // bDescriptorSubtype: Call Management Func Desc
	0x00,   // bmCapabilities: D0+D1
	0x01,   // bDataInterface: 1

	// ACM Functional Descriptor
	0x04,   // bFunctionLength
	0x24,   // bDescriptorType: CS_INTERFACE
	0x02,   // bDescriptorSubtype: Abstract Control Management desc
	0x02,   // bmCapabilities

	// Union Functional Descriptor
	0x05,   // bFunctionLength
	0x24,   // bDescriptorType: CS_INTERFACE
	0x06,   // bDescriptorSubtype: Union func desc
	0x00,   // bMasterInterface: Communication class interface
	0x01,   // bSlaveInterface0: Data Class Interface

	// Endpoint 2 Descriptor
	0x07,                           // bLength: Endpoint Descriptor size
	USB_ENDPOINT_DESCRIPTOR_TYPE,   // bDescriptorType: Endpoint
	USBD_Dev_CDC_CMD_EP,            // bEndpointAddress
	0x03,                           // bmAttributes: Interrupt
	CDC_CMD_PACKET_SZE, 0x00,       // wMaxPacketSize:
	0x10,                           // bInterval:

	// ---------------------------------------------------------------------------

	// Data class interface descriptor
	0x09,   // bLength: Endpoint Descriptor size
	USB_INTERFACE_DESCRIPTOR_TYPE,  // bDescriptorType:
	0x01,   // bInterfaceNumber: Number of Interface
	0x00,   // bAlternateSetting: Alternate setting
	0x02,   // bNumEndpoints: Two endpoints used
	0x0A,   // bInterfaceClass: CDC
	0x00,   // bInterfaceSubClass:
	0x00,   // bInterfaceProtocol:
	0x00,   // iInterface:

	// Endpoint OUT Descriptor
	0x07,   // bLength: Endpoint Descriptor size
	USB_ENDPOINT_DESCRIPTOR_TYPE,      // bDescriptorType: Endpoint
	USBD_Dev_CDC_H2D_EP,               // bEndpointAddress
	0x02,                              // bmAttributes: Bulk
	USBD_Dev_CDC_H2D_EP_SZ, 0x00,      // wMaxPacketSize:
	0x00,                              // bInterval: ignore for Bulk transfer

	// Endpoint IN Descriptor
	0x07,   // bLength: Endpoint Descriptor size
	USB_ENDPOINT_DESCRIPTOR_TYPE,      // bDescriptorType: Endpoint
	USBD_Dev_CDC_D2H_EP,               // bEndpointAddress
	0x02,                              // bmAttributes: Bulk
	USBD_Dev_CDC_D2H_EP_SZ, 0x00,      // wMaxPacketSize:
	0x00                               // bInterval: ignore for Bulk transfer
};

uint8_t USBD_CDC_LangIDDesc[USB_SIZ_STRING_LANGID] = {
	USB_SIZ_STRING_LANGID,
	USB_DESC_TYPE_STRING,
	0x09, 0x04,
};

struct cdc_linecodeing_t {
	uint32_t bitrate;
	uint8_t  format;
	uint8_t  paritytype;
	uint8_t  datatype;
}
cdc_linecoding =
{
	115200, /* baud rate*/
	0x00,   /* stop bits-1*/
	0x00,   /* parity - none*/
	0x08    /* nb. of bits 8*/
};
#define CDC_GET_LINE_CODING 0x21
#define CDC_SET_LINE_CODING 0x20

static char USBD_CDC_InFlight = 0;
volatile ringbuffer_t USBD_CDC_H2D_FIFO;
volatile ringbuffer_t USBD_CDC_D2H_FIFO;
static uint8_t* USBD_CDC_D2H_Buff;
static uint8_t* USBD_CDC_H2D_Buff;
static uint8_t* USBD_CDC_CMD_Buff;
static uint8_t USBD_CDC_AltSet;
char USBD_CDC_IsReady = 0;

/**
 * @brief  USBD_CDC_ClassInit
 *         Initilaize the CDC interface
 * @param  pdev: device instance
 * @param  cfgidx: Configuration index
 * @retval status
 */
static uint8_t  USBD_CDC_ClassInit (void* pdev, uint8_t cfgidx)
{
	DCD_EP_Open(pdev,
				USBD_Dev_CDC_D2H_EP,
				USBD_Dev_CDC_D2H_EP_SZ,
				USB_OTG_EP_BULK);

	DCD_EP_Open(pdev,
				USBD_Dev_CDC_H2D_EP,
				USBD_Dev_CDC_H2D_EP_SZ,
				USB_OTG_EP_BULK);

	DCD_EP_Open(pdev,
				USBD_Dev_CDC_CMD_EP,
				USBD_Dev_CDC_CMD_EP_SZ,
				USB_OTG_EP_INT);

	USBD_CDC_InFlight = 0;

	ringbuffer_init(&USBD_CDC_H2D_FIFO, malloc(CDC_DATA_IN_PACKET_SIZE), CDC_DATA_IN_PACKET_SIZE);
	ringbuffer_init(&USBD_CDC_D2H_FIFO, malloc(CDC_DATA_MAX_PACKET_SIZE), CDC_DATA_MAX_PACKET_SIZE);
	USBD_CDC_D2H_Buff = malloc(CDC_DATA_IN_PACKET_SIZE);
	USBD_CDC_H2D_Buff = malloc(CDC_DATA_MAX_PACKET_SIZE);
	USBD_CDC_CMD_Buff = malloc(CDC_CMD_PACKET_SZE);

	/* Prepare Out endpoint to receive next packet */
	DCD_EP_PrepareRx(pdev,
					USBD_Dev_CDC_H2D_EP,
					(uint8_t*)(USBD_CDC_H2D_Buff),
					USBD_Dev_CDC_H2D_EP_SZ);

	return USBD_OK;
}

/**
 * @brief  USBD_CDC_ClassDeInit
 *         DeInitialize the CDC layer
 * @param  pdev: device instance
 * @param  cfgidx: Configuration index
 * @retval status
 */
static uint8_t  USBD_CDC_ClassDeInit (void  *pdev, uint8_t cfgidx)
{
	DCD_EP_Close(pdev, USBD_Dev_CDC_D2H_EP);
	DCD_EP_Close(pdev, USBD_Dev_CDC_H2D_EP);
	DCD_EP_Close(pdev, USBD_Dev_CDC_CMD_EP);

	free(USBD_CDC_H2D_Buff); USBD_CDC_H2D_Buff = 0;
	free(USBD_CDC_D2H_Buff); USBD_CDC_D2H_Buff = 0;
	free(USBD_CDC_CMD_Buff); USBD_CDC_CMD_Buff = 0;
	free(USBD_CDC_H2D_FIFO.start); USBD_CDC_H2D_FIFO.start = 0;
	free(USBD_CDC_D2H_FIFO.start); USBD_CDC_D2H_FIFO.start = 0;

	USBD_CDC_IsReady = 0;

	return USBD_OK;
}

/**
 * @brief  USBD_CDC_Setup
 *         Handle the CDC specific requests
 * @param  pdev: instance
 * @param  req: usb requests
 * @retval status
 */
static uint8_t  USBD_CDC_Setup (void  *pdev, USB_SETUP_REQ *req)
{
	dbgwdg_feed();

	uint16_t len=USB_CDC_DESC_SIZ;
	uint8_t  *pbuf=USBD_CDC_ConfigDescriptor + 9;

	switch (req->bmRequest & USB_REQ_TYPE_MASK)
	{
		/* CDC Class Requests -------------------------------*/
		case USB_REQ_TYPE_CLASS :
			/* Check if the request is a data setup packet */
			if (req->wLength > 0)
			{
				/* Check if the request is Device-to-Host */
				if (req->bmRequest & 0x80)
				{
					// nothing to do
					//VCP_Ctrl(req->bRequest, CmdBuff, req->wLength);

					if (req->bRequest == CDC_GET_LINE_CODING) {
						memcpy(USBD_CDC_CMD_Buff, &cdc_linecoding, req->wLength);
					}

					/* Send the data to the host */
					USBD_CtlSendData (pdev, USBD_CDC_CMD_Buff, req->wLength);
				}
				else /* Host-to-Device request */
				{
					/* Set the value of the current command to be processed */
					// nothing to do
					//cdcCmd = req->bRequest;
					//cdcLen = req->wLength;

					/* Prepare the reception of the buffer over EP0
					Next step: the received data will be managed in USBD_CDC_EP0_TxSent()
					function. */
					USBD_CtlPrepareRx (pdev, USBD_CDC_CMD_Buff, req->wLength);
				}
			}
			else
			{
				// nothing to do
				//VCP_Ctrl(req->bRequest, NULL, 0);
				USBD_CtlSendStatus(pdev);
			}

			return USBD_OK;

		/* Standard Requests -------------------------------*/
	case USB_REQ_TYPE_STANDARD:
		switch (req->bRequest)
		{
		case USB_REQ_GET_DESCRIPTOR:
			if( (req->wValue >> 8) == CDC_DESCRIPTOR_TYPE)
			{
				pbuf = USBD_CDC_ConfigDescriptor + 9 + (9 * USBD_ITF_MAX_NUM);
				len = MIN(USB_CDC_DESC_SIZ , req->wLength);
			}

			USBD_CtlSendData (pdev,
							pbuf,
							len);
			break;

		case USB_REQ_GET_INTERFACE :
			USBD_CtlSendData (pdev, (uint8_t *)&USBD_CDC_AltSet, 1);
			break;

		case USB_REQ_SET_INTERFACE :
			if ((uint8_t)(req->wValue) < USBD_ITF_MAX_NUM)
			{
				USBD_CDC_AltSet = (uint8_t)(req->wValue);
			}
			else
			{
				USBD_CtlError (pdev, req);
				dbg_printf(DBGMODE_ERR, "CDC Stall, USB_REQ_SET_INTERFACE, unknown wValue 0x%04X, file " __FILE__ ":%d\r\n", req->wValue, __LINE__);
			}
			break;
		}

		default:
			USBD_CtlError (pdev, req);
			dbg_printf(DBGMODE_ERR, "CDC Stall, unknown bmRequest 0x%02X, file " __FILE__ ":%d\r\n", req->bmRequest, __LINE__);
			return USBD_FAIL;
	}
	return USBD_OK;
}

/**
 * @brief  USBD_CDC_EP0_RxReady
 *         Data received on control endpoint
 * @param  pdev: device device instance
 * @retval status
 */
static uint8_t  USBD_CDC_EP0_RxReady (void  *pdev)
{
	// all virtual, nothing to do
	return USBD_OK;
}

/**
 * @brief  USBD_CDC_DataIn
 *         Data sent on non-control IN endpoint
 * @param  pdev: device instance
 * @param  epnum: endpoint number
 * @retval status
 */
static uint8_t  USBD_CDC_DataIn (void *pdev, uint8_t epnum)
{
	uint16_t i;

	if (USBD_CDC_InFlight != 0) {
		USBD_CDC_InFlight = 0;
	}

	if (ringbuffer_isempty(&USBD_CDC_D2H_FIFO) == 0)
	{
		for (i = 0; i < CDC_DATA_IN_PACKET_SIZE && ringbuffer_isempty(&USBD_CDC_D2H_FIFO) == 0; i++) {
			USBD_CDC_D2H_Buff[i] = ringbuffer_pop(&USBD_CDC_D2H_FIFO);
		}
		dbgwdg_feed();
		DCD_EP_Tx (pdev,
					USBD_Dev_CDC_D2H_EP,
					(uint8_t*)USBD_CDC_D2H_Buff,
					i);
		USBD_CDC_InFlight = 1;
	}

	if (USBD_CDC_InFlight == 0) {
		DCD_EP_Flush(pdev, epnum);
	}

	return USBD_OK;
}

/**
 * @brief  USBD_CDC_DataOut
 *         Data received on non-control Out endpoint
 * @param  pdev: device instance
 * @param  epnum: endpoint number
 * @retval status
 */
static uint8_t  USBD_CDC_DataOut (void *pdev, uint8_t epnum)
{
	uint16_t USB_Rx_Cnt;

	/* Get the received data buffer and update the counter */
	USB_Rx_Cnt = ((USB_OTG_CORE_HANDLE*)pdev)->dev.out_ep[epnum].xfer_count;

	for (uint16_t i = 0; i < USB_Rx_Cnt; i++) {
		uint8_t c = USBD_CDC_H2D_Buff[i];
		ringbuffer_push(&USBD_CDC_H2D_FIFO, c);
	}

	/* Prepare Out endpoint to receive next packet */
	DCD_EP_PrepareRx(pdev,
						USBD_Dev_CDC_H2D_EP,
						(uint8_t*)(USBD_CDC_H2D_Buff),
						CDC_DATA_OUT_PACKET_SIZE);

	return USBD_OK;
}

/**
 * @brief  USBD_CDC_SOF
 *         Start Of Frame event management
 * @param  pdev: instance
 * @param  epnum: endpoint number
 * @retval status
 */
static uint32_t CdcFrameCount;
static uint8_t  USBD_CDC_SOF (void *pdev)
{
	if (CdcFrameCount >= CDC_IN_FRAME_INTERVAL)
	{
		if (USBD_CDC_InFlight == 0)
		{
			if (USBD_CDC_D2H_FIFO.ready == 0xAB)
			{
				USBD_CDC_IsReady = 1;

				if (ringbuffer_isempty(&USBD_CDC_D2H_FIFO) == 0)
				{
					uint16_t i;
					for (i = 0; i < CDC_DATA_IN_PACKET_SIZE && ringbuffer_isempty(&USBD_CDC_D2H_FIFO) == 0; i++) {
						USBD_CDC_D2H_Buff[i] = ringbuffer_pop(&USBD_CDC_D2H_FIFO);
					}
					DCD_EP_Tx (pdev,
								USBD_Dev_CDC_D2H_EP,
								(uint8_t*)USBD_CDC_D2H_Buff,
								i);
					USBD_CDC_InFlight = 1;
					CdcFrameCount = 0;
				}
			}
		}
		else if (CdcFrameCount > 100) // timeout on waiting
		{
			led_1_tog();
			CdcFrameCount = 0;
			USBD_CDC_InFlight = 0;
			DCD_EP_Flush(pdev, USBD_Dev_CDC_D2H_EP);
		}
	}

	CdcFrameCount++;

	return USBD_OK;
}

static uint8_t* USBD_CDC_GetConfigDescriptor (uint8_t speed, uint16_t *length)
{
	*length = sizeof (USBD_CDC_ConfigDescriptor);
	return USBD_CDC_ConfigDescriptor;
}

uint8_t* USBD_CDC_GetUsrStrDescriptor( uint8_t speed , uint8_t index ,  uint16_t *length)
{
	USBD_GetString ("", USBD_StrDesc, length);
	return USBD_StrDesc;
}

uint8_t *  USBD_CDC_DeviceDescriptor( uint8_t speed , uint16_t *length)
{
	*length = sizeof(USBD_CDC_DeviceDesc);
	return USBD_CDC_DeviceDesc;
}

uint8_t *  USBD_CDC_LangIDStrDescriptor( uint8_t speed , uint16_t *length)
{
	*length =  sizeof(USBD_CDC_LangIDDesc);
	return USBD_CDC_LangIDDesc;
}

uint8_t *  USBD_CDC_ProductStrDescriptor( uint8_t speed , uint16_t *length)
{
	USBD_GetString ("CDC", USBD_StrDesc, length);
	return USBD_StrDesc;
}

uint8_t *  USBD_CDC_ManufacturerStrDescriptor( uint8_t speed , uint16_t *length)
{
	USBD_GetString ("Frank", USBD_StrDesc, length);
	return USBD_StrDesc;
}

uint8_t *  USBD_CDC_SerialStrDescriptor( uint8_t speed , uint16_t *length)
{
	USBD_GetString ("", USBD_StrDesc, length);
	return USBD_StrDesc;
}

uint8_t *  USBD_CDC_ConfigStrDescriptor( uint8_t speed , uint16_t *length)
{
	USBD_GetString ("", USBD_StrDesc, length);
	return USBD_StrDesc;
}

uint8_t *  USBD_CDC_InterfaceStrDescriptor( uint8_t intf, uint8_t speed , uint16_t *length)
{
	USBD_GetString ("", USBD_StrDesc, length);
	return USBD_StrDesc;
}

void USBD_CDC_USR_Init(void) { }
void USBD_CDC_DeviceReset(uint8_t speed ) { }
void USBD_CDC_DeviceConfigured (void) { }
void USBD_CDC_DeviceSuspended(void) { }
void USBD_CDC_DeviceResumed(void) { }
void USBD_CDC_DeviceConnected (void) { }
void USBD_CDC_DeviceDisconnected (void) { }

USBD_Device_cb_TypeDef USBD_Dev_CDC_cb =
{
	USBD_CDC_ClassInit,
	USBD_CDC_ClassDeInit,
	USBD_CDC_Setup,
	0,
	USBD_CDC_EP0_RxReady,
	USBD_CDC_DataIn,
	USBD_CDC_DataOut,
	USBD_CDC_SOF,
	0,
	0,

	USBD_CDC_DeviceDescriptor,
	USBD_CDC_GetConfigDescriptor,
	USBD_CDC_GetConfigDescriptor,
	USBD_CDC_GetUsrStrDescriptor,
	USBD_CDC_LangIDStrDescriptor,
	USBD_CDC_ManufacturerStrDescriptor,
	USBD_CDC_ProductStrDescriptor,
	USBD_CDC_SerialStrDescriptor,
	USBD_CDC_ConfigStrDescriptor,
	USBD_CDC_InterfaceStrDescriptor,

	USBD_CDC_USR_Init,
	USBD_CDC_DeviceReset,
	USBD_CDC_DeviceConfigured,
	USBD_CDC_DeviceSuspended,
	USBD_CDC_DeviceResumed,
	USBD_CDC_DeviceConnected,
	USBD_CDC_DeviceDisconnected,
};
