/*
 * UsbXlater - by Frank Zhao
 *
 * this file is for DualShock 4 emulation
 * reverse engineering notes: http://eleccelerator.com/wiki/index.php?title=DualShock_4
 *
 */

#include "usbd_dev_ds4.h"
#include "usbd_dev_inc_all.h"
#include <usbh_dev/usbh_dev_dualshock.h>
#include <flashfile.h>
#include <stdlib.h>
#include <string.h>

#define USBD_DEV_DS4_Feature_A3_SIZE 49
const uint8_t USBD_DEV_DS4_Feature_A3[USBD_DEV_DS4_Feature_A3_SIZE] = { 0xA3, 0x41, 0x75, 0x67, 0x20, 0x20, 0x33, 0x20, 0x32, 0x30, 0x31, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x37, 0x3A, 0x30, 0x31, 0x3A, 0x31, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x31, 0x03, 0x00, 0x00, 0x00, 0x49, 0x00, 0x05, 0x00, 0x00, 0x80, 0x03, 0x00 };

#define USBD_DEV_DS4_Feature_02_SIZE 37
const uint8_t USBD_DEV_DS4_Feature_02[USBD_DEV_DS4_Feature_02_SIZE] = { 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x87, 0x22, 0x7B, 0xDD, 0xB2, 0x22, 0x47, 0xDD, 0xBD, 0x22, 0x43, 0xDD, 0x1C, 0x02, 0x1C, 0x02, 0x7F, 0x1E, 0x2E, 0xDF, 0x60, 0x1F, 0x4C, 0xE0, 0x3A, 0x1D, 0xC6, 0xDE, 0x08, 0x00 };

#define USBD_DEV_DS4_Feature_12_SIZE 16
const uint8_t USBD_DEV_DS4_Feature_12[USBD_DEV_DS4_Feature_12_SIZE] = { 0x12, 0x8B, 0x09, 0x07, 0x6D, 0x66, 0x1C, 0x08, 0x25, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

#define USBD_Dev_DS4_DeviceDescriptor_SIZE 18
const uint8_t USBD_Dev_DS4_DeviceDescriptor[USBD_Dev_DS4_DeviceDescriptor_SIZE] = {
0x12,        // bLength
0x01,        // bDescriptorType (Device)
0x00, 0x02,  // bcdUSB 2.00
0x00,        // bDeviceClass (Use class information in the Interface Descriptors)
0x00,        // bDeviceSubClass 
0x00,        // bDeviceProtocol 
0x40,        // bMaxPacketSize0 64
0x4C, 0x05,  // idVendor 0x054C
0xC4, 0x05,  // idProduct 0x05C4
0x00, 0x01,  // bcdDevice 1.00
0x01,        // iManufacturer (String Index)
0x02,        // iProduct (String Index)
0x00,        // iSerialNumber (String Index)
0x01,        // bNumConfigurations 1
};

#define USBD_Dev_DS4_ConfigurationDescriptor_SIZE 41
const uint8_t USBD_Dev_DS4_ConfigurationDescriptor[USBD_Dev_DS4_ConfigurationDescriptor_SIZE] = {
0x09,        // bLength
0x02,        // bDescriptorType (Configuration)
0x29, 0x00,  // wTotalLength 41
0x01,        // bNumInterfaces 1
0x01,        // bConfigurationValue
0x00,        // iConfiguration (String Index)
0xC0,        // bmAttributes Self Powered
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
0xD3, 0x01,  // wDescriptorLength[0] 467

0x07,        // bLength
0x05,        // bDescriptorType (Endpoint)
USBD_Dev_DS4_D2H_EP,        // bEndpointAddress (IN/D2H)
0x03,        // bmAttributes (Interrupt)
0x40, 0x00,  // wMaxPacketSize 64
0x05,        // bInterval 5 (unit depends on device speed)

0x07,        // bLength
0x05,        // bDescriptorType (Endpoint)
USBD_Dev_DS4_H2D_EP,        // bEndpointAddress (OUT/H2D)
0x03,        // bmAttributes (Interrupt)
0x40, 0x00,  // wMaxPacketSize 64
0x05,        // bInterval 5 (unit depends on device speed)
};

#define USBD_Dev_DS4_HIDReportDescriptor_SIZE 467
const uint8_t USBD_Dev_DS4_HIDReportDescriptor[USBD_Dev_DS4_HIDReportDescriptor_SIZE] = {
0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
0x09, 0x05,        // Usage (Game Pad)
0xA1, 0x01,        // Collection (Physical)
0x85, 0x01,        //   Report ID (1)
0x09, 0x30,        //   Usage (X)
0x09, 0x31,        //   Usage (Y)
0x09, 0x32,        //   Usage (Z)
0x09, 0x35,        //   Usage (Rz)
0x15, 0x00,        //   Logical Minimum (0)
0x26, 0xFF, 0x00,  //   Logical Maximum (255)
0x75, 0x08,        //   Report Size (8)
0x95, 0x04,        //   Report Count (4)
0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0x09, 0x39,        //   Usage (Hat switch)
0x15, 0x00,        //   Logical Minimum (0)
0x25, 0x07,        //   Logical Maximum (7)
0x35, 0x00,        //   Physical Minimum (0)
0x46, 0x3B, 0x01,  //   Physical Maximum (315)
0x65, 0x14,        //   Unit (System: English Rotation, Length: Centimeter)
0x75, 0x04,        //   Report Size (4)
0x95, 0x01,        //   Report Count (1)
0x81, 0x42,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
0x65, 0x00,        //   Unit (None)
0x05, 0x09,        //   Usage Page (Button)
0x19, 0x01,        //   Usage Minimum (0x01)
0x29, 0x0E,        //   Usage Maximum (0x0E)
0x15, 0x00,        //   Logical Minimum (0)
0x25, 0x01,        //   Logical Maximum (1)
0x75, 0x01,        //   Report Size (1)
0x95, 0x0E,        //   Report Count (14)
0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
0x09, 0x20,        //   Usage (0x20)
0x75, 0x06,        //   Report Size (6)
0x95, 0x01,        //   Report Count (1)
0x15, 0x00,        //   Logical Minimum (0)
0x25, 0x7F,        //   Logical Maximum (127)
0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
0x09, 0x33,        //   Usage (Rx)
0x09, 0x34,        //   Usage (Ry)
0x15, 0x00,        //   Logical Minimum (0)
0x26, 0xFF, 0x00,  //   Logical Maximum (255)
0x75, 0x08,        //   Report Size (8)
0x95, 0x02,        //   Report Count (2)
0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
0x09, 0x21,        //   Usage (0x21)
0x95, 0x36,        //   Report Count (54)
0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0x85, 0x05,        //   Report ID (5)
0x09, 0x22,        //   Usage (0x22)
0x95, 0x1F,        //   Report Count (31)
0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0x04,        //   Report ID (4)
0x09, 0x23,        //   Usage (0x23)
0x95, 0x24,        //   Report Count (36)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0x02,        //   Report ID (2)
0x09, 0x24,        //   Usage (0x24)
0x95, 0x24,        //   Report Count (36)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0x08,        //   Report ID (8)
0x09, 0x25,        //   Usage (0x25)
0x95, 0x03,        //   Report Count (3)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0x10,        //   Report ID (16)
0x09, 0x26,        //   Usage (0x26)
0x95, 0x04,        //   Report Count (4)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0x11,        //   Report ID (17)
0x09, 0x27,        //   Usage (0x27)
0x95, 0x02,        //   Report Count (2)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0x12,        //   Report ID (18)
0x06, 0x02, 0xFF,  //   Usage Page (Vendor Defined 0xFF02)
0x09, 0x21,        //   Usage (0x21)
0x95, 0x0F,        //   Report Count (15)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0x13,        //   Report ID (19)
0x09, 0x22,        //   Usage (0x22)
0x95, 0x16,        //   Report Count (22)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0x14,        //   Report ID (20)
0x06, 0x05, 0xFF,  //   Usage Page (Vendor Defined 0xFF05)
0x09, 0x20,        //   Usage (0x20)
0x95, 0x10,        //   Report Count (16)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0x15,        //   Report ID (21)
0x09, 0x21,        //   Usage (0x21)
0x95, 0x2C,        //   Report Count (44)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x06, 0x80, 0xFF,  //   Usage Page (Vendor Defined 0xFF80)
0x85, 0x80,        //   Report ID (128)
0x09, 0x20,        //   Usage (0x20)
0x95, 0x06,        //   Report Count (6)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0x81,        //   Report ID (129)
0x09, 0x21,        //   Usage (0x21)
0x95, 0x06,        //   Report Count (6)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0x82,        //   Report ID (130)
0x09, 0x22,        //   Usage (0x22)
0x95, 0x05,        //   Report Count (5)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0x83,        //   Report ID (131)
0x09, 0x23,        //   Usage (0x23)
0x95, 0x01,        //   Report Count (1)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0x84,        //   Report ID (132)
0x09, 0x24,        //   Usage (0x24)
0x95, 0x04,        //   Report Count (4)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0x85,        //   Report ID (133)
0x09, 0x25,        //   Usage (0x25)
0x95, 0x06,        //   Report Count (6)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0x86,        //   Report ID (134)
0x09, 0x26,        //   Usage (0x26)
0x95, 0x06,        //   Report Count (6)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0x87,        //   Report ID (135)
0x09, 0x27,        //   Usage (0x27)
0x95, 0x23,        //   Report Count (35)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0x88,        //   Report ID (136)
0x09, 0x28,        //   Usage (0x28)
0x95, 0x22,        //   Report Count (34)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0x89,        //   Report ID (137)
0x09, 0x29,        //   Usage (0x29)
0x95, 0x02,        //   Report Count (2)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0x90,        //   Report ID (144)
0x09, 0x30,        //   Usage (0x30)
0x95, 0x05,        //   Report Count (5)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0x91,        //   Report ID (145)
0x09, 0x31,        //   Usage (0x31)
0x95, 0x03,        //   Report Count (3)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0x92,        //   Report ID (146)
0x09, 0x32,        //   Usage (0x32)
0x95, 0x03,        //   Report Count (3)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0x93,        //   Report ID (147)
0x09, 0x33,        //   Usage (0x33)
0x95, 0x0C,        //   Report Count (12)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0xA0,        //   Report ID (160)
0x09, 0x40,        //   Usage (0x40)
0x95, 0x06,        //   Report Count (6)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0xA1,        //   Report ID (161)
0x09, 0x41,        //   Usage (0x41)
0x95, 0x01,        //   Report Count (1)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0xA2,        //   Report ID (162)
0x09, 0x42,        //   Usage (0x42)
0x95, 0x01,        //   Report Count (1)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0xA3,        //   Report ID (163)
0x09, 0x43,        //   Usage (0x43)
0x95, 0x30,        //   Report Count (48)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0xA4,        //   Report ID (164)
0x09, 0x44,        //   Usage (0x44)
0x95, 0x0D,        //   Report Count (13)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0xA5,        //   Report ID (165)
0x09, 0x45,        //   Usage (0x45)
0x95, 0x15,        //   Report Count (21)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0xA6,        //   Report ID (166)
0x09, 0x46,        //   Usage (0x46)
0x95, 0x15,        //   Report Count (21)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0xF0,        //   Report ID (240)
0x09, 0x47,        //   Usage (0x47)
0x95, 0x3F,        //   Report Count (63)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0xF1,        //   Report ID (241)
0x09, 0x48,        //   Usage (0x48)
0x95, 0x3F,        //   Report Count (63)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0xF2,        //   Report ID (242)
0x09, 0x49,        //   Usage (0x49)
0x95, 0x0F,        //   Report Count (15)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0xA7,        //   Report ID (167)
0x09, 0x4A,        //   Usage (0x4A)
0x95, 0x01,        //   Report Count (1)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0xA8,        //   Report ID (168)
0x09, 0x4B,        //   Usage (0x4B)
0x95, 0x01,        //   Report Count (1)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0xA9,        //   Report ID (169)
0x09, 0x4C,        //   Usage (0x4C)
0x95, 0x08,        //   Report Count (8)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0xAA,        //   Report ID (170)
0x09, 0x4E,        //   Usage (0x4E)
0x95, 0x01,        //   Report Count (1)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0xAB,        //   Report ID (171)
0x09, 0x4F,        //   Usage (0x4F)
0x95, 0x39,        //   Report Count (57)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0xAC,        //   Report ID (172)
0x09, 0x50,        //   Usage (0x50)
0x95, 0x39,        //   Report Count (57)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0xAD,        //   Report ID (173)
0x09, 0x51,        //   Usage (0x51)
0x95, 0x0B,        //   Report Count (11)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0xAE,        //   Report ID (174)
0x09, 0x52,        //   Usage (0x52)
0x95, 0x01,        //   Report Count (1)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0xAF,        //   Report ID (175)
0x09, 0x53,        //   Usage (0x53)
0x95, 0x02,        //   Report Count (2)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0xB0,        //   Report ID (176)
0x09, 0x54,        //   Usage (0x54)
0x95, 0x3F,        //   Report Count (63)
0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0xC0,              // End Collection
};

#define USBD_Dev_DS4_HIDDescriptor_SIZE 9
const uint8_t USBD_Dev_DS4_HIDDescriptor[USBD_Dev_DS4_HIDDescriptor_SIZE] = {
0x09,        // bLength
0x21,        // bDescriptorType (HID)
0x11, 0x01,  // bcdHID 1.17
0x00,        // bCountryCode
0x01,        // bNumDescriptors
0x22,        // bDescriptorType[0] (HID)
0xD3, 0x01,  // wDescriptorLength[0] 467
};

static uint8_t USBD_HID_Protocol, USBD_HID_IdleState, USBD_HID_AltSet;
extern uint8_t ds4_rpt_cnt;
char USBD_Host_Is_PS4 = 0;
char USBD_Dev_DS4_IsActive = 0;

uint8_t USBD_Dev_DS4_ClassInit(void *pcore , uint8_t cfgidx)
{
	DCD_EP_Open(pcore,
                USBD_Dev_DS4_D2H_EP,
                USBD_Dev_DS4_D2H_EP_SZ,
                USB_OTG_EP_INT);
	DCD_EP_Flush(pcore, USBD_Dev_DS4_D2H_EP);

	DCD_EP_Open(pcore,
                USBD_Dev_DS4_H2D_EP,
                USBD_Dev_DS4_H2D_EP_SZ,
                USB_OTG_EP_INT);
	DCD_EP_Flush(pcore, USBD_Dev_DS4_H2D_EP);

	USBD_Host_Is_PS4 = 0;
	USBD_Dev_DS4_IsActive = 1;
	ds4_rpt_cnt = 0;
}

uint8_t USBD_Dev_DS4_ClassDeInit(void *pcore , uint8_t cfgidx)
{
	DCD_EP_Close (pcore , USBD_Dev_DS4_H2D_EP);
	DCD_EP_Close (pcore , USBD_Dev_DS4_D2H_EP);

	USBD_Dev_DS4_IsActive = 0;
	USBD_Host_Is_PS4 = 0;

	dbg_printf(DBGMODE_TRACE, "USBD_Dev_DS4_ClassDeInit\r\n");

	return USBD_OK;
}

uint8_t USBD_Dev_DS4_Setup(void *pcore , USB_SETUP_REQ  *req)
{
	dbgwdg_feed();

	uint16_t len = 0;
	uint8_t  *pbuf = NULL;

	if ((req->bmRequest & USB_REQ_RECIPIENT_MASK) == USB_REQ_RECIPIENT_INTERFACE && (req->bmRequest & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_CLASS && (req->bmRequest & 0x80) == USB_D2H && req->bRequest == 0x01 && req->wIndex == 0)
	{
		if (req->wValue == 0x0302) {
			USBD_Host_Is_PS4 = 1;
			USBD_Dev_DS_bufTemp[0] = 0x02;
			memcpy(&USBD_Dev_DS_bufTemp[1], flashfilesystem.nvm_file->d.fmt.report_02, req->wLength - 1);
			USBD_CtlSendData (pcore, (uint8_t *)USBD_Dev_DS_bufTemp, req->wLength);
		}
		else if (req->wValue == 0x03A3) {
			USBD_Host_Is_PS4 = 1;
			USBD_Dev_DS_bufTemp[0] = 0xA3;
			memcpy(&USBD_Dev_DS_bufTemp[1], flashfilesystem.nvm_file->d.fmt.report_a3, req->wLength - 1);
			USBD_CtlSendData (pcore, (uint8_t *)USBD_Dev_DS_bufTemp, req->wLength);
		}
		else if (req->wValue == 0x0312) {
			USBD_Host_Is_PS4 = 1;
			USBD_Dev_DS_bufTemp[0] = 0x12;
			memcpy(&USBD_Dev_DS_bufTemp[1], flashfilesystem.nvm_file->d.fmt.report_12, req->wLength - 1);
			USBD_CtlSendData (pcore, (uint8_t *)USBD_Dev_DS_bufTemp, req->wLength);
		}
		else {
			dbg_printf(DBGMODE_ERR, "Unknown GET_REPORT from PS4, wValue 0x%04X\r\n", req->wValue);
			USBD_CtlPrepareRx (pcore, (uint8_t *)USBD_Dev_DS_bufTemp, req->wLength);
		}
		return USBD_OK;
	}
	else if ((req->bmRequest & USB_REQ_RECIPIENT_MASK) == USB_REQ_RECIPIENT_INTERFACE && (req->bmRequest & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_CLASS && (req->bmRequest & 0x80) == USB_H2D && req->bRequest == 0x09 && req->wIndex == 0)
	{
		if (req->wValue == 0x0312) {
			// this we can ignore
			USBD_Host_Is_PS4 = 1;
		}
		else if (req->wValue == 0x0313) {
			// incoming mac address
			USBD_Host_Is_PS4 = 1;
		}
		else {
			dbg_printf(DBGMODE_ERR, "Unknown SET_REPORT from PS4, wValue 0x%04X\r\n", req->wValue);
		}

		USBD_Dev_DS_lastWValue = req->wValue;
		USBD_CtlPrepareRx (pcore, (uint8_t *)USBD_Dev_DS_bufTemp, req->wLength);
		return USBD_OK;
	}

	if (req->bmRequest == 0x21 && req->bRequest == HID_REQ_SET_IDLE && req->wIndex == 0)
	{
		USBD_HID_IdleState = (uint8_t)(req->wValue >> 8);
		USBD_Dev_DS_lastWValue = req->wValue;
		USBD_CtlPrepareRx (pcore, (uint8_t *)USBD_Dev_DS_bufTemp, req->wLength);
		return USBD_OK;
	}

	// these are standard HID class stuff, the PS3 might never actually do these
	switch (req->bmRequest & USB_REQ_TYPE_MASK)
	{
		case USB_REQ_TYPE_CLASS :
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
					USBD_CtlSendData (pcore, (uint8_t *)USBD_Dev_DS_bufTemp, req->wLength);
					break;

				case HID_REQ_SET_REPORT:
					// TODO: handle data
					USBD_CtlPrepareRx (pcore, (uint8_t *)USBD_Dev_DS_bufTemp, req->wLength);
					break;

				default:
					USBD_CtlError (pcore, req);
					dbg_printf(DBGMODE_ERR, "DS4 Stall, unknown req 0x%02X, file " __FILE__ ":%d\r\n", req->bRequest, __LINE__);
					return USBD_FAIL;
			}
			break;

		case USB_REQ_TYPE_STANDARD:
			switch (req->bRequest)
			{
				case USB_REQ_GET_DESCRIPTOR:
					if( req->wValue >> 8 == HID_REPORT_DESC)
					{
						pbuf = USBD_Dev_DS4_HIDReportDescriptor;
						if (USBD_Dev_DS4_HIDReportDescriptor_SIZE < req->wLength)
							len = USBD_Dev_DS4_HIDReportDescriptor_SIZE;
						else
							len = req->wLength;
					}
					else if( req->wValue >> 8 == HID_DESCRIPTOR_TYPE)
					{
						pbuf = USBD_Dev_DS4_HIDDescriptor;
						if (USBD_HIDDESCRIPTOR_SIZE < req->wLength)
							len = USBD_HIDDESCRIPTOR_SIZE;
						else
							len = req->wLength;
					}

					USBD_CtlSendData (pcore,
									pbuf,
									len);

					break;

				case USB_REQ_GET_INTERFACE :
					USBD_CtlSendData (pcore,
									(uint8_t *)&USBD_HID_AltSet,
									1);
					break;

				case USB_REQ_SET_INTERFACE :
					USBD_HID_AltSet = (uint8_t)(req->wValue);
					break;
			}
	}
	return USBD_OK;
}

uint8_t USBD_Dev_DS4_SendReport (USB_OTG_CORE_HANDLE *pcore,
                                uint8_t *report,
                                uint16_t len)
{
	static char repCnt;

	if (pcore->dev.device_status == USB_OTG_CONFIGURED && USBD_Dev_DS4_IsActive != 0)
	{
		dbgwdg_feed();

		DCD_EP_Tx (pcore, USBD_Dev_DS4_D2H_EP, report, len);

		ds4_rpt_cnt++;

		if (repCnt > 50) {
			led_1_tog();
			repCnt = 0;
		}
		repCnt++;
	}
	else
	{
		//dbg_printf(DBGMODE_ERR, "DS4_SendReport, device not ready yet, file " __FILE__ ":%d\r\n", __LINE__);
	}
	return USBD_OK;
}

uint8_t USBD_Dev_DS4_EP0_TxSent        (void *pcore )
{
	return USBD_OK;
}

uint8_t USBD_Dev_DS4_EP0_RxReady       (void *pcore )
{
	USBD_Dev_DS4_DataOut(pcore, 0x00);
	return USBD_OK;
}

uint8_t USBD_Dev_DS4_DataIn            (void *pcore , uint8_t epnum)
{
	// Ensure that the FIFO is empty before a new transfer, this condition could be caused by a new transfer before the end of the previous transfer
	DCD_EP_Flush(pcore, epnum);
	return USBD_OK;
}

uint8_t USBD_Dev_DS4_DataOut            (void *pcore , uint8_t epnum)
{
	if (epnum == 0x00)
	{
		if (USBD_Dev_DS_lastWValue == 0x0313)
		{
			nvm_file_t* f = &flashfilesystem.cache;
			memcpy(f->d.fmt.report_13, &(USBD_Dev_DS_bufTemp[1]), 6 + 8 + 8);
			flashfilesystem.cache_dirty = 1;
			flashfile_cacheFlush();
		}
		else if (USBD_Dev_DS_lastWValue == 0x0312)
		{
			// this event doesn't happen but it can theoretically happen
			nvm_file_t* f = &flashfilesystem.cache;
			memcpy(f->d.fmt.report_12, &(USBD_Dev_DS_bufTemp[1]), 3 * 5);
			flashfilesystem.cache_dirty = 1;
			flashfile_cacheFlush();
		}
		else if (USBD_Dev_DS_lastWValue == 0x0314)
		{
			if (USBD_Dev_DS_bufTemp[1] == 0x01) {
				USBH_DS_PostedTask = USBH_DSPT_TASK_FEATURE_14_1401;
			}
			else if (USBD_Dev_DS_bufTemp[1] == 0x02) {
				USBH_DS_PostedTask = USBH_DSPT_TASK_FEATURE_14_1402;
			}
		}
		USBD_CtlSendStatus(pcore);
	}
	else if (epnum == USBD_Dev_DS4_H2D_EP)
	{
		// TODO: figure out how rumble works
	}
	return USBD_OK;
}

uint8_t USBD_Dev_DS4_SOF(void *pcore)
{
	return USBD_OK;
}

uint8_t* USBD_Dev_DS4_GetDeviceDescriptor( uint8_t speed , uint16_t *length)
{
	*length = sizeof(USBD_Dev_DS4_DeviceDescriptor);
	return USBD_Dev_DS4_DeviceDescriptor;
}

uint8_t* USBD_Dev_DS4_GetConfigDescriptor( uint8_t speed , uint16_t *length)
{
	*length = sizeof(USBD_Dev_DS4_ConfigurationDescriptor);
	return USBD_Dev_DS4_ConfigurationDescriptor;
}

uint8_t* USBD_Dev_DS4_GetOtherConfigDescriptor( uint8_t speed , uint16_t *length)
{
	*length = 0;
	return 0;
}


uint8_t* USBD_Dev_DS4_GetUsrStrDescriptor( uint8_t speed , uint8_t index ,  uint16_t *length)
{
	USBD_GetString ("", USBD_StrDesc, length);
	return USBD_StrDesc;
}

uint8_t* USBD_Dev_DS4_GetLangIDStrDescriptor( uint8_t speed , uint16_t *length)
{
	*length = sizeof(USBD_LangIDDesc);
	return USBD_LangIDDesc;
}

uint8_t* USBD_Dev_DS4_GetManufacturerStrDescriptor( uint8_t speed , uint16_t *length)
{
	USBD_GetString ("Sony Computer Entertainment", USBD_StrDesc, length);
	return USBD_StrDesc;
}

uint8_t* USBD_Dev_DS4_GetProductStrDescriptor( uint8_t speed , uint16_t *length)
{
	USBD_GetString ("Wireless Controller", USBD_StrDesc, length);
	return USBD_StrDesc;
}

uint8_t* USBD_Dev_DS4_GetSerialStrDescriptor( uint8_t speed , uint16_t *length)
{
	USBD_GetString ("", USBD_StrDesc, length);
	return USBD_StrDesc;
}

uint8_t* USBD_Dev_DS4_GetConfigurationStrDescriptor( uint8_t speed , uint16_t *length)
{
	USBD_GetString ("", USBD_StrDesc, length);
	return USBD_StrDesc;
}

uint8_t* USBD_Dev_DS4_GetInterfaceStrDescriptor( uint8_t intf , uint8_t speed , uint16_t *length)
{
	USBD_GetString ("", USBD_StrDesc, length);
	return USBD_StrDesc;
}

void USBD_Dev_DS4_UsrInit(void) { }
void USBD_Dev_DS4_DeviceReset(uint8_t speed) { }
void USBD_Dev_DS4_DeviceConfigured(void) { }
void USBD_Dev_DS4_DeviceSuspended(void) { }
void USBD_Dev_DS4_DeviceResumed(void) { }
void USBD_Dev_DS4_DeviceConnected(void) { }
void USBD_Dev_DS4_DeviceDisconnected(void) { }

USBD_Device_cb_TypeDef USBD_Dev_DS4_cb =
{
	USBD_Dev_DS4_ClassInit,
	USBD_Dev_DS4_ClassDeInit,
	USBD_Dev_DS4_Setup,
	USBD_Dev_DS4_EP0_TxSent,
	USBD_Dev_DS4_EP0_RxReady,
	USBD_Dev_DS4_DataIn,
	USBD_Dev_DS4_DataOut,
	USBD_Dev_DS4_SOF,
	0,
	0,

	USBD_Dev_DS4_GetDeviceDescriptor,
	USBD_Dev_DS4_GetConfigDescriptor,
	USBD_Dev_DS4_GetOtherConfigDescriptor,
	USBD_Dev_DS4_GetUsrStrDescriptor,
	USBD_Dev_DS4_GetLangIDStrDescriptor,
	USBD_Dev_DS4_GetManufacturerStrDescriptor,
	USBD_Dev_DS4_GetProductStrDescriptor,
	USBD_Dev_DS4_GetSerialStrDescriptor,
	USBD_Dev_DS4_GetConfigurationStrDescriptor,
	USBD_Dev_DS4_GetInterfaceStrDescriptor,

	USBD_Dev_DS4_UsrInit,
	USBD_Dev_DS4_DeviceReset,
	USBD_Dev_DS4_DeviceConfigured,
	USBD_Dev_DS4_DeviceSuspended,
	USBD_Dev_DS4_DeviceResumed,
	USBD_Dev_DS4_DeviceConnected,
	USBD_Dev_DS4_DeviceDisconnected,
};
