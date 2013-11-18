#ifndef USBH_DEV_DUALSHOCK_H_
#define USBH_DEV_DUALSHOCK_H_

#include <stdint.h>
#include <usbh_lib/usbh_core.h>

extern char USBH_DS3_Available;
extern char USBH_DS4_Available;
extern char USBH_DS3_NewData;
extern char USBH_DS4_NewData;
extern uint8_t USBH_DS_Buffer[64];

void USBH_DS4_Init_Handler(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t intf);
void USBH_DS4_DeInit_Handler(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t intf);
void USBH_DS4_Decode_Handler(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t intf, uint8_t ep, uint8_t* data, uint8_t len);
void USBH_DS3_Init_Handler(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t intf);
void USBH_DS3_DeInit_Handler(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t intf);
void USBH_DS3_Decode_Handler(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t intf, uint8_t ep, uint8_t* data, uint8_t len);

#endif
