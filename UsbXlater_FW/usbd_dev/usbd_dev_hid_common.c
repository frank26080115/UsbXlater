#include "usbd_dev_inc_all.h"
#include <stdint.h>

const uint8_t USBD_LangIDDesc[USBD_LANGIDDESC_SIZE] =
{
	USBD_LANGIDDESC_SIZE,
	USB_DESC_TYPE_STRING,
	0x09, 0x04,
};

uint8_t USBD_Dev_DS_bufTemp[64];
uint8_t USBD_Dev_DS_masterBdaddr[6];
uint16_t USBD_Dev_DS_lastWValue;
