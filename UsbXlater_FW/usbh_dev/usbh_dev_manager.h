#ifndef USBH_DEV_MANAGER_H_
#define USBH_DEV_MANAGER_H_

#include <stdint.h>
#include <hidrpt.h>

extern volatile uint8_t USBH_Dev_Reset_Timer;

int8_t USBH_Dev_GetAvailAddress();
void USBH_Dev_FreeAddress(uint8_t addr);
void USBH_Dev_AddrManager_Reset();

typedef struct {
	uint16_t vid;
	uint16_t pid;
	HID_Rpt_Parsing_Params_t parser;
}
Dev_Parser_Lookup_t;

#endif /* USBH_DEV_MANAGER_H_ */
