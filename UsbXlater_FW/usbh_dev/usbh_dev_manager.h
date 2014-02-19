#ifndef USBH_DEV_MANAGER_H_
#define USBH_DEV_MANAGER_H_

#include <usbh_lib/usbh_core.h>
#include <usbh_lib/usbh_def.h>
#include <usbotg_lib/usb_core.h>
#include <stdint.h>
#include <hidrpt.h>

extern volatile uint32_t USBH_Dev_Reset_Timer;

int8_t USBH_Dev_GetAvailAddress();
void USBH_Dev_FreeAddress(uint8_t addr);
void USBH_Dev_AddrManager_Reset();
int8_t USBH_Dev_AllocControl(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev);
void USBH_Dev_FreeControl(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev);

char* USBH_Dev_DebugPrint(USBH_DEV *pdev, USBH_EpDesc_TypeDef* ep);
void USBH_Dev_DebugFreeChannels(USB_OTG_CORE_HANDLE * pcore);

typedef struct {
	uint16_t vid;
	uint16_t pid;
	HID_Rpt_Parsing_Params_t parser;
}
Dev_Parser_Lookup_t;

#endif /* USBH_DEV_MANAGER_H_ */
