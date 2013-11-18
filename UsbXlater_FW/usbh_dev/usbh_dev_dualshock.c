#include "usbh_dev_dualshock.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

char USBH_DS3_Available = 0;
char USBH_DS4_Available = 0;
char USBH_DS3_NewData = 0;
char USBH_DS4_NewData = 0;

uint8_t USBH_DS_Buffer[64];

void USBH_DS4_Init_Handler(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t intf)
{
	USBH_DS4_Available = 1;
}

void USBH_DS4_DeInit_Handler(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t intf)
{
	USBH_DS4_Available = 0;
}

void USBH_DS4_Decode_Handler(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t intf, uint8_t ep, uint8_t* data, uint8_t len)
{
	// assume ep and intf are the only ones available
	if (len > 64) len = 64;
	memcpy(USBH_DS_Buffer, data, len);
	USBH_DS3_NewData = 1;
}

void USBH_DS3_Init_Handler(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t intf)
{
	USBH_DS3_Available = 1;
}

void USBH_DS3_DeInit_Handler(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t intf)
{
	USBH_DS3_Available = 0;
}

void USBH_DS3_Decode_Handler(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t intf, uint8_t ep, uint8_t* data, uint8_t len)
{
	// assume ep and intf are the only ones available
	if (len > 64) len = 64;
	memcpy(USBH_DS_Buffer, data, len);
	USBH_DS3_NewData = 1;
}
