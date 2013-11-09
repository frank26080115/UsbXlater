/*
 * UsbXlater - by Frank Zhao
 *
 * this file is for DualShock 3 emulation
 * code takes hints from gimx.fr
 *
 */

#include "usbd_dev_ds3.h"
#include "usbd_dev_inc_all.h"
#include <stdlib.h>
#include <string.h>

const uint8_t USBD_Dev_DS3_DeviceDescriptor[USBD_DEV_DS3_DEVICEDESCRIPTOR_SIZE] = {
	0x12,        // bLength
	0x01,        // bDescriptorType (Device)
	0x00, 0x02,  // bcdUSB 2.00
	0x00,        // bDeviceClass (Use class information in the Interface Descriptors)
	0x00,        // bDeviceSubClass
	0x00,        // bDeviceProtocol
	0x40,        // bMaxPacketSize0 64
	0x4C, 0x05,  // idVendor 0x054C
	0x68, 0x02,  // idProduct 0x0268
	0x00, 0x01,  // bcdDevice 1.00
	0x01,        // iManufacturer (String Index)
	0x02,        // iProduct (String Index)
	0x00,        // iSerialNumber (String Index)
	0x01,        // bNumConfigurations 1
};

const uint8_t USBD_Dev_DS3_ConfigurationDescriptor[USBD_DEV_DS3_CONFIGURATIONDESCRIPTOR_SIZE] = {
	0x09,        // bLength
	0x02,        // bDescriptorType (Configuration)
	0x29, 0x00,  // wTotalLength 41
#ifdef ENABLE_DS3_ADDITIONAL_FEATURES
	0x02,        // bNumInterfaces 2
#else
	0x01,        // bNumInterfaces 1
#endif
	0x01,        // bConfigurationValue
	0x00,        // iConfiguration (String Index)
	0x80,        // bmAttributes
	0xFA,        // bMaxPower 500mA

	0x09,        // bLength
	0x04,        // bDescriptorType (Interface)
	0x00,        // bInterfaceNumber 0
	0x00,        // bAlternateSetting
	0x02,        // bNumEndpoints 2
	0x03,        // bInterfaceClass
	0x00,        // bInterfaceSubClass
	0x00,        // bInterfaceProtocol
	0x00,        // iInterface (String Index)

	0x09,        // bLength
	0x21,        // bDescriptorType (HID)
	0x11, 0x01,  // bcdHID 1.17
	0x00,        // bCountryCode
	0x01,        // bNumDescriptors
	0x22,        // bDescriptorType[0] (HID)
	USBD_DEV_DS3_HIDREPORTDESCRIPTOR_SIZE, 0x00,  // wDescriptorLength[0]

	0x07,        // bLength
	0x05,        // bDescriptorType (Endpoint)
	USBD_Dev_DS3_H2D_EP, // bEndpointAddress (OUT/H2D)
	0x03,        // bmAttributes (Interrupt)
	USBD_Dev_DS3_H2D_EP_SZ, 0x00, // wMaxPacketSize 64
	0x01,        // bInterval 1 (unit depends on device speed)

	0x07,        // bLength
	0x05,        // bDescriptorType (Endpoint)
	USBD_Dev_DS3_D2H_EP, // bEndpointAddress (IN/D2H)
	0x03,        // bmAttributes (Interrupt)
	USBD_Dev_DS3_D2H_EP_SZ, 0x00, // wMaxPacketSize 64
	0x01,        // bInterval 1 (unit depends on device speed)

#ifdef ENABLE_DS3_ADDITIONAL_FEATURES
	0x09,        // bLength
	0x04,        // bDescriptorType (Interface)
	0x01,        // bInterfaceNumber 1
	0x00,        // bAlternateSetting
	0x02,        // bNumEndpoints 1
	0x03,        // bInterfaceClass
	0x00,        // bInterfaceSubClass
	0x00,        // bInterfaceProtocol
	0x00,        // iInterface (String Index)

	0x09,        // bLength
	0x21,        // bDescriptorType (HID)
	0x11, 0x01,  // bcdHID 1.17
	0x00,        // bCountryCode
	0x01,        // bNumDescriptors
	0x22,        // bDescriptorType[0] (HID)
	0x94, 0x00,  // wDescriptorLength[0] 148

	0x07,        // bLength
	0x05,        // bDescriptorType (Endpoint)
	USBD_Dev_DS3_D2H_EP_ADDITIONAL, // bEndpointAddress (IN/D2H)
	0x03,        // bmAttributes (Interrupt)
	USBD_Dev_DS3_D2H_EP_ADDITIONAL_SZ, 0x00, // wMaxPacketSize 8
	0x01,        // bInterval 1 (unit depends on device speed)
#endif
};

const uint8_t USBD_Dev_DS3_HIDDescriptor[USBD_HIDDESCRIPTOR_SIZE] = {
	0x09,        // bLength
	0x21,        // bDescriptorType (HID)
	0x11, 0x01,  // bcdHID 1.17
	0x00,        // bCountryCode
	0x01,        // bNumDescriptors
	0x22,        // bDescriptorType[0] (HID)
	USBD_DEV_DS3_HIDREPORTDESCRIPTOR_SIZE, 0x00,  // wDescriptorLength[0]
};

const uint8_t USBD_Dev_DS3_HIDReportDescriptor[USBD_DEV_DS3_HIDREPORTDESCRIPTOR_SIZE] = {
	0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
	0x09, 0x04,        // Usage (Joystick)
	0xA1, 0x01,        // Collection (Physical)
	0xA1, 0x02,        //   Collection (Application)
	0x85, 0x01,        //     Report ID (1)
	0x75, 0x08,        //     Report Size (8)
	0x95, 0x01,        //     Report Count (1)
	0x15, 0x00,        //     Logical Minimum (0)
	0x26, 0xFF, 0x00,  //     Logical Maximum (255)
	0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x75, 0x01,        //     Report Size (1)
	0x95, 0x13,        //     Report Count (19)
	0x15, 0x00,        //     Logical Minimum (0)
	0x25, 0x01,        //     Logical Maximum (1)
	0x35, 0x00,        //     Physical Minimum (0)
	0x45, 0x01,        //     Physical Maximum (1)
	0x05, 0x09,        //     Usage Page (Button)
	0x19, 0x01,        //     Usage Minimum (0x01)
	0x29, 0x13,        //     Usage Maximum (0x13)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x75, 0x01,        //     Report Size (1)
	0x95, 0x0D,        //     Report Count (13)
	0x06, 0x00, 0xFF,  //     Usage Page (Vendor Defined 0xFF00)
	0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x15, 0x00,        //     Logical Minimum (0)
	0x26, 0xFF, 0x00,  //     Logical Maximum (255)
	0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
	0x09, 0x01,        //     Usage (Pointer)
	0xA1, 0x00,        //     Collection (Undefined)
	0x75, 0x08,        //       Report Size (8)
	0x95, 0x04,        //       Report Count (4)
	0x35, 0x00,        //       Physical Minimum (0)
	0x46, 0xFF, 0x00,  //       Physical Maximum (255)
	0x09, 0x30,        //       Usage (X)
	0x09, 0x31,        //       Usage (Y)
	0x09, 0x32,        //       Usage (Z)
	0x09, 0x35,        //       Usage (Rz)
	0x81, 0x02,        //       Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0xC0,              //     End Collection
	0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
	0x75, 0x08,        //     Report Size (8)
	0x95, 0x27,        //     Report Count (39)
	0x09, 0x01,        //     Usage (Pointer)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x75, 0x08,        //     Report Size (8)
	0x95, 0x30,        //     Report Count (48)
	0x09, 0x01,        //     Usage (Pointer)
	0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
	0x75, 0x08,        //     Report Size (8)
	0x95, 0x30,        //     Report Count (48)
	0x09, 0x01,        //     Usage (Pointer)
	0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
	0xC0,              //   End Collection
	0xA1, 0x02,        //   Collection (Application)
	0x85, 0x02,        //     Report ID (2)
	0x75, 0x08,        //     Report Size (8)
	0x95, 0x30,        //     Report Count (48)
	0x09, 0x01,        //     Usage (Pointer)
	0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
	0xC0,              //   End Collection
	0xA1, 0x02,        //   Collection (Application)
	0x85, 0xEE,        //     Report ID (238)
	0x75, 0x08,        //     Report Size (8)
	0x95, 0x30,        //     Report Count (48)
	0x09, 0x01,        //     Usage (Pointer)
	0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
	0xC0,              //   End Collection
	0xA1, 0x02,        //   Collection (Application)
	0x85, 0xEF,        //     Report ID (239)
	0x75, 0x08,        //     Report Size (8)
	0x95, 0x30,        //     Report Count (48)
	0x09, 0x01,        //     Usage (Pointer)
	0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
	0xC0,              //   End Collection
	0xC0,              // End Collection
};

#ifdef ENABLE_DS3_ADDITIONAL_FEATURES
const uint8_t USBD_Dev_DS3_HIDDescriptor_Additional[USBD_HIDDESCRIPTOR_SIZE] = {
	0x09,        // bLength
	0x21,        // bDescriptorType (HID)
	0x11, 0x01,  // bcdHID 1.17
	0x00,        // bCountryCode
	0x01,        // bNumDescriptors
	0x22,        // bDescriptorType[0] (HID)
	USBD_DEV_DS3_HIDREPORTDESCRIPTOR_ADDITIONAL_SIZE, 0x00,  // wDescriptorLength[0]
};

const uint8_t USBD_Dev_DS3_HIDReportDescriptor_Additional[USBD_DEV_DS3_HIDREPORTDESCRIPTOR_ADDITIONAL_SIZE] = {
	0x05, 0x01,       // USAGE_PAGE (Generic Desktop)
	0x09, 0x06,       // USAGE (Keyboard)
	0xA1, 0x01,       // COLLECTION (Application)
	0x85, 0x01,       // REPORT_ID (1)
	0x75, 0x01,       //   REPORT_SIZE (1)
	0x95, 0x08,       //   REPORT_COUNT (8)
	0x05, 0x07,       //   USAGE_PAGE (Keyboard)(Key Codes)
	0x19, 0xE0,       //   USAGE_MINIMUM (Keyboard LeftControl)(224)
	0x29, 0xE7,       //   USAGE_MAXIMUM (Keyboard Right GUI)(231)
	0x15, 0x00,       //   LOGICAL_MINIMUM (0)
	0x25, 0x01,       //   LOGICAL_MAXIMUM (1)
	0x81, 0x02,       //   INPUT (Data,Var,Abs) ; Modifier byte
	0x95, 0x01,       //   REPORT_COUNT (1)
	0x75, 0x08,       //   REPORT_SIZE (8)
	0x81, 0x03,       //   INPUT (Cnst,Var,Abs) ; Reserved byte
	0x95, 0x05,       //   REPORT_COUNT (5)
	0x75, 0x01,       //   REPORT_SIZE (1)
	0x05, 0x08,       //   USAGE_PAGE (LEDs)
	0x19, 0x01,       //   USAGE_MINIMUM (Num Lock)
	0x29, 0x05,       //   USAGE_MAXIMUM (Kana)
	0x91, 0x02,       //   OUTPUT (Data,Var,Abs) ; LED report
	0x95, 0x01,       //   REPORT_COUNT (1)
	0x75, 0x03,       //   REPORT_SIZE (3)
	0x91, 0x03,       //   OUTPUT (Cnst,Var,Abs) ; LED report padding
	0x95, 0x05,       //   REPORT_COUNT (5)
	0x75, 0x08,       //   REPORT_SIZE (8)
	0x15, 0x00,       //   LOGICAL_MINIMUM (0)
	0x26, 0xA4, 0x00, //   LOGICAL_MAXIMUM (164)
	0x05, 0x07,       //   USAGE_PAGE (Keyboard)(Key Codes)
	0x19, 0x00,       //   USAGE_MINIMUM (Reserved (no event indicated))(0)
	0x2A, 0xA4, 0x00, //   USAGE_MAXIMUM (Keyboard Application)(164)
	0x81, 0x00,       //   INPUT (Data,Ary,Abs)
	0xC0,             // END_COLLECTION

	// this second consumer report is what handles the multimedia keys
	0x05, 0x0C,       // USAGE_PAGE (Consumer Devices)
	0x09, 0x01,       // USAGE (Consumer Control)
	0xA1, 0x01,       // COLLECTION (Application)
	0x85, 0x02,       //   REPORT_ID (2)
	0x19, 0x00,       //   USAGE_MINIMUM (Unassigned)
	0x2A, 0x3C, 0x02, //   USAGE_MAXIMUM
	0x15, 0x00,       //   LOGICAL_MINIMUM (0)
	0x26, 0x3C, 0x02, //   LOGICAL_MAXIMUM
	0x95, 0x01,       //   REPORT_COUNT (1)
	0x75, 0x10,       //   REPORT_SIZE (16)
	0x81, 0x00,       //   INPUT (Data,Ary,Abs)
	0xC0,             // END_COLLECTION

	// system controls, like power, needs a 3rd different report and report descriptor
	0x05, 0x01,       // USAGE_PAGE (Generic Desktop)
	0x09, 0x80,       // USAGE (System Control)
	0xA1, 0x01,       // COLLECTION (Application)
	0x85, 0x03,       //   REPORT_ID (3)
	0x95, 0x01,       //   REPORT_COUNT (1)
	0x75, 0x02,       //   REPORT_SIZE (2)
	0x15, 0x01,       //   LOGICAL_MINIMUM (1)
	0x25, 0x03,       //   LOGICAL_MAXIMUM (3)
	0x09, 0x82,       //   USAGE (System Sleep)
	0x09, 0x81,       //   USAGE (System Power)
	0x09, 0x83,       //   USAGE (System Wakeup)
	0x81, 0x60,       //   INPUT
	0x75, 0x06,       //   REPORT_SIZE (6)
	0x81, 0x03,       //   INPUT (Cnst,Var,Abs)
	0xC0,             // END_COLLECTION
};
#endif

const uint8_t USBD_LangIDDesc[USBD_LANGIDDESC_SIZE] =
{
	USBD_LANGIDDESC_SIZE,
	USB_DESC_TYPE_STRING,
	0x09, 0x04,
};

uint8_t USBD_Dev_DS3_bufTemp[64];
static char USBD_Dev_DS3_buf3f5HasRead = 0;
static uint8_t USBD_Dev_DS3_masterBdaddr[6];
static uint8_t USBD_Dev_DS3_3efByte6 = 0xB0;
static uint16_t USBD_Dev_DS3_lastWValue;

// a lot of these byte arrays are taken from gimx.fr's code, thanks

const uint8_t ds3_buf301[] = {
    0x00,
    0x01,0x03,0x00,0x04,0x0c,0x01,0x02,0x18,
    0x18,0x18,0x18,0x09,0x0a,0x10,0x11,0x12,
    0x13,0x00,0x00,0x00,0x00,0x04,0x00,0x02,
    0x02,0x02,0x02,0x00,0x00,0x00,0x04,0x04,
    0x04,0x04,0x00,0x00,0x01,0x06,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

const uint8_t ds3_buf3f2[] = {
    0xf2,
    0xff,0xff,0x00,
    0x00,0x06,0xF5,0x48,0xE2,0x49,
    //0x01,0x02,0x03,0x04,0x05,0x06, //device bdaddr
    0x00,0x03,0x50,0x81,0xd8,0x01,0x8a,0x00,
    0x00,0x01,0x64,0x19,0x01,0x00,0x64,0x00,
    0x01,0x90,0x00,0x19,0xfe,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,
};

const uint8_t ds3_buf3f5[] = {
    0x01,0x00,
    0xaa,0xaa,0xaa,0xaa,0xaa,0xaa, //dummy PS3 bdaddr
    0x23,0x1e,0x00,0x03,0x50,0x81,0xd8,0x01,
    0x8a,0x00,0x00,0x01,0x64,0x19,0x01,0x00,
    0x64,0x00,0x01,0x90,0x00,0x19,0xfe,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

const uint8_t ds3_buf3ef[] = {
    0x00,0xef,0x03,0x00,0x04,0x03,0x01,0xb0,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x02,0x05,0x01,0x92,0x02,0x02,0x01,
    0x91,0x02,0x05,0x01,0x91,0x02,0x04,0x00,
    0x76,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

const uint8_t ds3_buf3f8[] = {
    0x00,0x01,0x00,0x00,0x07,0x03,0x01,0xb0,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x02,0x6b,0x02,0x68,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

const uint8_t ds3_buf3f7[] = {
    0x01,0x00,0x08,0x03,0xd2,0x01,0xee,0xff,
    0x10,0x02,0x00,0x03,0x50,0x81,0xd8,0x01,
    0x8a,0x00,0x00,0x01,0x64,0x19,0x01,0x00,
    0x64,0x00,0x01,0x90,0x00,0x19,0xfe,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

uint8_t USBD_HID_Protocol, USBD_HID_IdleState, USBD_HID_AltSet;
#ifdef ENABLE_DS3_ADDITIONAL_FEATURES
uint8_t USBD_HID_Protocol2, USBD_HID_IdleState2, USBD_HID_AltSet2;
#endif

uint8_t DS3_Rumble[2];
uint8_t DS3_LED[5];
char USBD_Host_Is_PS3 = 0;

uint8_t USBD_Dev_DS3_ClassInit         (void *pcore , uint8_t cfgidx)
{
	DCD_EP_Open(pcore,
                USBD_Dev_DS3_D2H_EP,
                USBD_Dev_DS3_D2H_EP_SZ,
                USB_OTG_EP_INT);

	DCD_EP_Open(pcore,
                USBD_Dev_DS3_H2D_EP,
                USBD_Dev_DS3_H2D_EP_SZ,
                USB_OTG_EP_INT);

    USBD_Host_Is_PS3 = 0;

	#ifdef ENABLE_DS3_ADDITIONAL_FEATURES
	DCD_EP_Open(pcore,
                USBD_Dev_DS3_D2H_EP_ADDITIONAL,
                USBD_Dev_DS3_D2H_EP_ADDITIONAL_SZ,
                USB_OTG_EP_INT);
	#endif

	return USBD_OK;
}

uint8_t USBD_Dev_DS3_ClassDeInit       (void *pcore , uint8_t cfgidx)
{
	DCD_EP_Close (pcore , 0x02);
	DCD_EP_Close (pcore , 0x81);
	#ifdef ENABLE_DS3_ADDITIONAL_FEATURES
	DCD_EP_Close (pcore , 0x83);
	#endif

	dbg_printf(DBGMODE_ERR, "\r\nUSBD_Dev_DS3_ClassDeInit\r\n");

	return USBD_OK;
}

uint8_t USBD_Dev_DS3_Setup             (void *pcore , USB_SETUP_REQ  *req)
{
	uint16_t len = 0;
	uint8_t  *pbuf = NULL;

	// PS3 does a lot of USB_REQ_RECIPIENT_INTERFACE requests
	// code takes hints from gimx.fr
	if ((req->bmRequest & USB_REQ_RECIPIENT_MASK) == USB_REQ_RECIPIENT_INTERFACE && (req->bmRequest & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_CLASS && (req->bmRequest & 0x80) == 0x80 && req->bRequest == 0x01 && req->wIndex == 0)
	{
		if (req->wValue == 0x0301) {
			USBD_CtlSendData (pcore, (uint8_t *)ds3_buf301, req->wLength);
		}
		else if (req->wValue == 0x03F2) {
			USBD_CtlSendData (pcore, (uint8_t *)ds3_buf3f2, req->wLength);
		}
		else if (req->wValue == 0x03F5) {
			memcpy(USBD_Dev_DS3_bufTemp, ds3_buf3f5, req->wLength);
			if (USBD_Dev_DS3_buf3f5HasRead == 0) {
				USBD_Dev_DS3_buf3f5HasRead = 1;
			}
			else {
				memcpy(&(USBD_Dev_DS3_bufTemp[2]), USBD_Dev_DS3_masterBdaddr, 6);
			}
			USBD_CtlSendData (pcore, (uint8_t *)USBD_Dev_DS3_bufTemp, req->wLength);
		}
		else if (req->wValue == 0x03EF) {
			memcpy(USBD_Dev_DS3_bufTemp, ds3_buf3ef, req->wLength);
			USBD_Dev_DS3_bufTemp[7] = USBD_Dev_DS3_3efByte6;
			USBD_CtlSendData (pcore, (uint8_t *)USBD_Dev_DS3_bufTemp, req->wLength);
		}
		else if (req->wValue == 0x03F8) {
			memcpy(USBD_Dev_DS3_bufTemp, ds3_buf3f8, req->wLength);
			USBD_Dev_DS3_bufTemp[7] = USBD_Dev_DS3_3efByte6; // not known if required
			USBD_CtlSendData (pcore, (uint8_t *)USBD_Dev_DS3_bufTemp, req->wLength);
		}
		else if (req->wValue == 0x03F7) {
			USBD_CtlSendData (pcore, (uint8_t *)ds3_buf3f7, req->wLength);
		}
		else {
			// should never reach here
			memset(USBD_Dev_DS3_bufTemp, 0, 0x40);
			USBD_CtlSendData (pcore, (uint8_t *)USBD_Dev_DS3_bufTemp, req->wLength);
			dbg_printf(DBGMODE_ERR, "\r\nrequest /w unknown wValue (0x%04X) from PS3\r\n", req->wValue);
		}
		return USBD_OK;
	}

	if ((req->bmRequest & USB_REQ_RECIPIENT_MASK) == USB_REQ_RECIPIENT_INTERFACE && (req->bmRequest & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_CLASS && (req->bmRequest & 0x80) == 0x00 && req->bRequest == HID_REQ_SET_REPORT && req->wIndex == 0)
	{
		if (req->wValue == 0x03F5 || req->wValue == 0x03EF || req->wValue == 0x03F2 || req->wValue == 0x03F4) {
			USBD_Dev_DS3_lastWValue = req->wValue;
			USBD_CtlPrepareRx (pcore, (uint8_t *)USBD_Dev_DS3_bufTemp, req->wLength);
			return USBD_OK;
		}
	}

	if (req->bmRequest == 0x21 && req->bRequest == HID_REQ_SET_IDLE && req->wIndex == 0)
	{
		USBD_HID_IdleState = (uint8_t)(req->wValue >> 8);
		USBD_Dev_DS3_lastWValue = req->wValue;
		USBD_CtlPrepareRx (pcore, (uint8_t *)USBD_Dev_DS3_bufTemp, req->wLength);
		return USBD_OK;
	}

	// these are standard HID class stuff, the PS3 might never actually do these
	switch (req->bmRequest & USB_REQ_TYPE_MASK)
	{
		case USB_REQ_TYPE_CLASS :

			#ifdef ENABLE_DS3_ADDITIONAL_FEATURES
			if (req->wIndex == 0) {
			#endif
			switch (req->bRequest)
			{
				case HID_REQ_SET_PROTOCOL:
					USBD_HID_Protocol = (uint8_t)(req->wValue);
					break;

				case HID_REQ_GET_PROTOCOL:
					USBD_CtlSendData (pcore,
									(uint8_t *)&USBD_HID_Protocol,
									1);
					break;

				case HID_REQ_SET_IDLE:
					USBD_HID_IdleState = (uint8_t)(req->wValue >> 8);
					break;

				case HID_REQ_GET_IDLE:
					USBD_CtlSendData (pcore,
									(uint8_t *)&USBD_HID_IdleState,
									1);
					break;

				case HID_REQ_GET_REPORT:
					// TODO: prep data
					USBD_CtlSendData (pcore, (uint8_t *)USBD_Dev_DS3_bufTemp, req->wLength);
					break;

				case HID_REQ_SET_REPORT:
					// TODO: handle data
					USBD_CtlPrepareRx (pcore, (uint8_t *)USBD_Dev_DS3_bufTemp, req->wLength);
					break;

				default:
					USBD_CtlError (pcore, req);
					dbg_printf(DBGMODE_ERR, "\r\nDS3 Stall, unknown req 0x%02X, file " __FILE__ ":%d\r\n", req->bRequest, __LINE__);
					return USBD_FAIL;
			}
			#ifdef ENABLE_DS3_ADDITIONAL_FEATURES
			} else if (req->wIndex == 1) {
				switch (req->bRequest)
				{
					case HID_REQ_SET_PROTOCOL:
						USBD_HID_Protocol2 = (uint8_t)(req->wValue);
						break;

					case HID_REQ_GET_PROTOCOL:
						USBD_CtlSendData (pcore,
										(uint8_t *)&USBD_HID_Protocol2,
										1);
						break;

					case HID_REQ_SET_IDLE:
						USBD_HID_IdleState2 = (uint8_t)(req->wValue >> 8);
						break;

					case HID_REQ_GET_IDLE:
						USBD_CtlSendData (pcore,
										(uint8_t *)&USBD_HID_IdleState2,
										1);
						break;

					case HID_REQ_GET_REPORT:
						// TODO: prep data
						USBD_CtlSendData (pcore, (uint8_t *)USBD_Dev_DS3_bufTemp, req->wLength);
						break;

					case HID_REQ_SET_REPORT:
						// TODO: handle data
						USBD_CtlPrepareRx (pcore, (uint8_t *)USBD_Dev_DS3_bufTemp, req->wLength);
						break;

					default:
						USBD_CtlError (pcore, req);
						dbg_printf(DBGMODE_ERR, "\r\nDS3 Stall, unknown req 0x%02X, file " __FILE__ ":%d\r\n", req->bRequest, __LINE__);
						return USBD_FAIL;
				}
			}
			#endif
			break;

		case USB_REQ_TYPE_STANDARD:
			switch (req->bRequest)
			{
				case USB_REQ_GET_DESCRIPTOR:
					if( req->wValue >> 8 == HID_REPORT_DESC)
					{
						#ifdef ENABLE_DS3_ADDITIONAL_FEATURES
						if (req->wIndex == 0)
						#endif
						{
							pbuf = USBD_Dev_DS3_HIDReportDescriptor;
							if (USBD_DEV_DS3_HIDREPORTDESCRIPTOR_SIZE < req->wLength)
								len = USBD_DEV_DS3_HIDREPORTDESCRIPTOR_SIZE;
							else
								len = req->wLength;
						}
						#ifdef ENABLE_DS3_ADDITIONAL_FEATURES
						else if (req->wIndex == 1)
						{
							pbuf = USBD_Dev_DS3_HIDReportDescriptor_Additional;
							if (USBD_DEV_DS3_HIDREPORTDESCRIPTOR_ADDITIONAL_SIZE < req->wLength)
								len = USBD_DEV_DS3_HIDREPORTDESCRIPTOR_ADDITIONAL_SIZE;
							else
								len = req->wLength;
						}
						#endif
					}
					else if( req->wValue >> 8 == HID_DESCRIPTOR_TYPE)
					{
						#ifdef ENABLE_DS3_ADDITIONAL_FEATURES
						if (req->wIndex == 0)
						#endif
						{
							pbuf = USBD_Dev_DS3_ConfigurationDescriptor + 0x09 + 0x09;
							if (USBD_HIDDESCRIPTOR_SIZE < req->wLength)
								len = USBD_HIDDESCRIPTOR_SIZE;
							else
								len = req->wLength;
						}
						#ifdef ENABLE_DS3_ADDITIONAL_FEATURES
						else if (req->wIndex == 1)
						{
							pbuf = USBD_Dev_DS3_ConfigurationDescriptor + 0x09 + 0x09 + USBD_HIDDESCRIPTOR_SIZE + 0x07 + 0x07 + 0x09 + 0x09;
							if (USBD_HIDDESCRIPTOR_SIZE < req->wLength)
								len = USBD_HIDDESCRIPTOR_SIZE;
							else
								len = req->wLength;
						}
						#endif
					}

					USBD_CtlSendData (pcore,
									pbuf,
									len);

					break;

				case USB_REQ_GET_INTERFACE :
					#ifdef ENABLE_DS3_ADDITIONAL_FEATURES
					if (req->wIndex == 0) {
					#endif
					USBD_CtlSendData (pcore,
									(uint8_t *)&USBD_HID_AltSet,
									1);
					#ifdef ENABLE_DS3_ADDITIONAL_FEATURES
					} else if (req->wIndex == 1) {
					USBD_CtlSendData (pcore,
									(uint8_t *)&USBD_HID_AltSet2,
									1);
					}
					#endif
					break;

				case USB_REQ_SET_INTERFACE :
					#ifdef ENABLE_DS3_ADDITIONAL_FEATURES
					if (req->wIndex == 0) {
					#endif
					USBD_HID_AltSet = (uint8_t)(req->wValue);
					#ifdef ENABLE_DS3_ADDITIONAL_FEATURES
					} else if (req->wIndex == 1) {
					USBD_HID_AltSet2 = (uint8_t)(req->wValue);
					}
					#endif
					break;
			}
	}
	return USBD_OK;
}

uint8_t USBD_Dev_DS3_SendReport (USB_OTG_CORE_HANDLE *pcore,
                                uint8_t *report,
                                uint16_t len)
{
	static int repCnt;

	if (pcore->dev.device_status == USB_OTG_CONFIGURED)
	{
		DCD_EP_Tx (pcore, USBD_Dev_DS3_D2H_EP, report, len);
		if (repCnt > 50) {
			led_1_tog();
			repCnt = 0;
		}
		repCnt++;
	}
	else
	{
		//dbg_printf(DBGMODE_ERR, "\r\n DS3_SendReport, device not ready yet, file " __FILE__ ":%d\r\n", __LINE__);
	}
	return USBD_OK;
}

uint8_t USBD_Dev_DS3_EP0_TxSent        (void *pcore )
{
	// TODO: implement
	return USBD_OK;
}

uint8_t USBD_Dev_DS3_EP0_RxReady       (void *pcore )
{
	USBD_Dev_DS3_DataOut(pcore, 0x00);
	return USBD_OK;
}

uint8_t USBD_Dev_DS3_DataIn            (void *pcore , uint8_t epnum)
{
	// Ensure that the FIFO is empty before a new transfer, this condition could be caused by a new transfer before the end of the previous transfer
	DCD_EP_Flush(pcore, epnum);
	return USBD_OK;
}

uint8_t USBD_Dev_DS3_DataOut            (void *pcore , uint8_t epnum)
{
	if (epnum == 0x00)
	{
		if (USBD_Dev_DS3_lastWValue == 0x03F5)
		{
			memcpy(USBD_Dev_DS3_masterBdaddr, &(USBD_Dev_DS3_bufTemp[2]), 6);
			USBD_Host_Is_PS3 = 1;
		}
		else if (USBD_Dev_DS3_lastWValue == 0x03EF)
		{
			USBD_Dev_DS3_3efByte6 = USBD_Dev_DS3_bufTemp[6];
		}
		else if (USBD_Dev_DS3_lastWValue == 0x03F4)
		{
			if (USBD_Dev_DS3_bufTemp[1] == 0x08) {
				// TODO: handle system shutdown
			}
			else {
				// TODO: enable reporting
			}
		}
		else if (USBD_Dev_DS3_lastWValue == 0x03F8)
		{
			// I don't think we have to handle this case
		}
		USBD_CtlSendStatus(pcore);
	}
	else if (epnum == USBD_Dev_DS3_H2D_EP)
	{
		// TODO: do something with the rumble and LED info

		uint8_t* tmpBuff = ((USB_OTG_CORE_HANDLE*)pcore)->dev.out_ep[epnum].xfer_buff;
		DS3_Rumble[0] = tmpBuff[1] ? tmpBuff[2] : 0;
		DS3_Rumble[1] = tmpBuff[3] ? tmpBuff[4] : 0;
		for (uint8_t i = 0; i < 5; i++)
		{
			if (tmpBuff[9] & (1 << i))
			{
				if (tmpBuff[13 + 5 * i] == 0)
					DS3_LED[i] = 1;
				else
					DS3_LED[i] = 2;
			} else {
				DS3_LED[i] = 0;
			}
		}
	}
	return USBD_OK;
}

uint8_t USBD_Dev_DS3_SOF               (void *pcore)
{
	return USBD_OK;
}

uint8_t* USBD_Dev_DS3_GetDeviceDescriptor( uint8_t speed , uint16_t *length)
{
	*length = sizeof(USBD_Dev_DS3_DeviceDescriptor);
	return USBD_Dev_DS3_DeviceDescriptor;
}

uint8_t* USBD_Dev_DS3_GetConfigDescriptor( uint8_t speed , uint16_t *length)
{
	*length = sizeof(USBD_Dev_DS3_ConfigurationDescriptor);
	return USBD_Dev_DS3_ConfigurationDescriptor;
}

uint8_t* USBD_Dev_DS3_GetOtherConfigDescriptor( uint8_t speed , uint16_t *length)
{
	*length = 0;
	return 0;
}


uint8_t* USBD_Dev_DS3_GetUsrStrDescriptor( uint8_t speed , uint8_t index ,  uint16_t *length)
{
	USBD_GetString ("", USBD_StrDesc, length);
	return USBD_StrDesc;
}

uint8_t* USBD_Dev_DS3_GetLangIDStrDescriptor( uint8_t speed , uint16_t *length)
{
	*length = sizeof(USBD_LangIDDesc);
	return USBD_LangIDDesc;
}

uint8_t* USBD_Dev_DS3_GetManufacturerStrDescriptor( uint8_t speed , uint16_t *length)
{
	USBD_GetString ("Sony", USBD_StrDesc, length);
	return USBD_StrDesc;
}

uint8_t* USBD_Dev_DS3_GetProductStrDescriptor( uint8_t speed , uint16_t *length)
{
	USBD_GetString ("PLAYSTATION(R)3 Controller", USBD_StrDesc, length);
	return USBD_StrDesc;
}

uint8_t* USBD_Dev_DS3_GetSerialStrDescriptor( uint8_t speed , uint16_t *length)
{
	USBD_GetString ("", USBD_StrDesc, length);
	return USBD_StrDesc;
}

uint8_t* USBD_Dev_DS3_GetConfigurationStrDescriptor( uint8_t speed , uint16_t *length)
{
	USBD_GetString ("", USBD_StrDesc, length);
	return USBD_StrDesc;
}

uint8_t* USBD_Dev_DS3_GetInterfaceStrDescriptor( uint8_t intf , uint8_t speed , uint16_t *length)
{
	USBD_GetString ("", USBD_StrDesc, length);
	return USBD_StrDesc;
}

void USBD_Dev_DS3_UsrInit(void) { }
void USBD_Dev_DS3_DeviceReset(uint8_t speed) { }
void USBD_Dev_DS3_DeviceConfigured(void) { }
void USBD_Dev_DS3_DeviceSuspended(void) { }
void USBD_Dev_DS3_DeviceResumed(void) { }
void USBD_Dev_DS3_DeviceConnected(void) { }
void USBD_Dev_DS3_DeviceDisconnected(void) { }

USBD_Device_cb_TypeDef USBD_Dev_DS3_cb =
{
	USBD_Dev_DS3_ClassInit,
	USBD_Dev_DS3_ClassDeInit,
	USBD_Dev_DS3_Setup,
	USBD_Dev_DS3_EP0_TxSent,
	USBD_Dev_DS3_EP0_RxReady,
	USBD_Dev_DS3_DataIn,
	USBD_Dev_DS3_DataOut,
	USBD_Dev_DS3_SOF,
	0,
	0,

	USBD_Dev_DS3_GetDeviceDescriptor,
	USBD_Dev_DS3_GetConfigDescriptor,
	USBD_Dev_DS3_GetOtherConfigDescriptor,
	USBD_Dev_DS3_GetUsrStrDescriptor,
	USBD_Dev_DS3_GetLangIDStrDescriptor,
	USBD_Dev_DS3_GetManufacturerStrDescriptor,
	USBD_Dev_DS3_GetProductStrDescriptor,
	USBD_Dev_DS3_GetSerialStrDescriptor,
	USBD_Dev_DS3_GetConfigurationStrDescriptor,
	USBD_Dev_DS3_GetInterfaceStrDescriptor,

	USBD_Dev_DS3_UsrInit,
	USBD_Dev_DS3_DeviceReset,
	USBD_Dev_DS3_DeviceConfigured,
	USBD_Dev_DS3_DeviceSuspended,
	USBD_Dev_DS3_DeviceResumed,
	USBD_Dev_DS3_DeviceConnected,
	USBD_Dev_DS3_DeviceDisconnected,
};
