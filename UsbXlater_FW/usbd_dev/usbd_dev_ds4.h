#ifndef USBD_DEV_DS4_H_
#define USBD_DEV_DS4_H_

#include <stdint.h>

#include <usbd_lib/usbd_core.h>
#include <usbd_lib/usbd_req.h>
#include <usbd_lib/usbd_conf.h>
#include <usbd_lib/usbd_ioreq.h>
#include <usbd_lib/usb_dcd.h>
#include <usbotg_lib/usb_regs.h>

#define USBD_Dev_DS4_D2H_EP 0x84
#define USBD_Dev_DS4_H2D_EP 0x05
#define USBD_Dev_DS4_D2H_EP_SZ 64
#define USBD_Dev_DS4_H2D_EP_SZ 64

#define USBD_DEV_DS4_Feature_A3_SIZE 49
#define USBD_DEV_DS4_Feature_02_SIZE 37
#define USBD_DEV_DS4_Feature_12_SIZE 16

uint8_t USBD_Dev_DS4_SendReport (USB_OTG_CORE_HANDLE *pcore,
                                uint8_t *report,
                                uint16_t len);
uint8_t USBD_Dev_DS4_ClassInit         (void *pcore , uint8_t cfgidx);
uint8_t USBD_Dev_DS4_ClassDeInit       (void *pcore , uint8_t cfgidx);
uint8_t USBD_Dev_DS4_Setup             (void *pcore , USB_SETUP_REQ  *req);
uint8_t USBD_Dev_DS4_EP0_TxSent        (void *pcore );
uint8_t USBD_Dev_DS4_EP0_RxReady       (void *pcore );
uint8_t USBD_Dev_DS4_DataIn            (void *pcore , uint8_t epnum);
uint8_t USBD_Dev_DS4_DataOut           (void *pcore , uint8_t epnum);
uint8_t USBD_Dev_DS4_SOF               (void *pcore);
uint8_t* USBD_Dev_DS4_GetDeviceDescriptor( uint8_t speed , uint16_t *length);
uint8_t* USBD_Dev_DS4_GetConfigDescriptor( uint8_t speed , uint16_t *length);
uint8_t* USBD_Dev_DS4_GetOtherConfigDescriptor( uint8_t speed , uint16_t *length);
uint8_t* USBD_Dev_DS4_GetUsrStrDescriptor( uint8_t speed , uint8_t index ,  uint16_t *length);
uint8_t* USBD_Dev_DS4_GetLangIDStrDescriptor( uint8_t speed , uint16_t *length);
uint8_t* USBD_Dev_DS4_GetManufacturerStrDescriptor( uint8_t speed , uint16_t *length);
uint8_t* USBD_Dev_DS4_GetProductStrDescriptor( uint8_t speed , uint16_t *length);
uint8_t* USBD_Dev_DS4_GetSerialStrDescriptor( uint8_t speed , uint16_t *length);
uint8_t* USBD_Dev_DS4_GetConfigurationStrDescriptor( uint8_t speed , uint16_t *length);
uint8_t* USBD_Dev_DS4_GetInterfaceStrDescriptor( uint8_t intf , uint8_t speed , uint16_t *length);
void USBD_Dev_DS4_UsrInit(void);
void USBD_Dev_DS4_DeviceReset(uint8_t speed);
void USBD_Dev_DS4_DeviceConfigured(void);
void USBD_Dev_DS4_DeviceSuspended(void);
void USBD_Dev_DS4_DeviceResumed(void);
void USBD_Dev_DS4_DeviceConnected(void);
void USBD_Dev_DS4_DeviceDisconnected(void);

extern USBD_Device_cb_TypeDef USBD_Dev_DS4_cb;
extern char USBD_Dev_DS4_NeedsWriteFlash;

#endif
