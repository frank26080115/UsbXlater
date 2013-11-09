#include "usbh_dev_manager.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

volatile uint8_t USBH_Dev_Reset_Timer = 0;

#define USBH_DEV_MAX_ADDR 127
#define USBH_DEV_MAX_ADDR_DIV8 ((USBH_DEV_MAX_ADDR + 1) / 8)
uint8_t USBH_Dev_UsedAddr[USBH_DEV_MAX_ADDR_DIV8 + 1];

int8_t USBH_Dev_GetAvailAddress()
{
	// 0 is only used for unenumerated devices, never assign 0
	// go through all of them
	for (int i = 1; i < USBH_DEV_MAX_ADDR; i++)
	{
		uint8_t bitIdx = i % 8;
		uint8_t byteIdx = i / 8;
		if ((USBH_Dev_UsedAddr[byteIdx] & (1 << bitIdx)) == 0) { // it is unused
			USBH_Dev_UsedAddr[byteIdx] |= (1 << bitIdx); // mark as used
			return i; // found
		}
	}
	return -1;
}

void USBH_Dev_FreeAddress(uint8_t addr)
{
	uint8_t bitIdx = addr % 8;
	uint8_t byteIdx = addr / 8;
	USBH_Dev_UsedAddr[byteIdx] &= ~(1 << bitIdx); // mark as used
}

void USBH_Dev_AddrManager_Reset()
{
	// mark all addresses as free
	for (int i = 0; i < USBH_DEV_MAX_ADDR_DIV8; i++) {
		USBH_Dev_UsedAddr[i] = 0;
	}
}
