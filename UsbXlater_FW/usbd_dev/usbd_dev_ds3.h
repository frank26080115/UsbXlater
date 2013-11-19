#ifndef USBD_DEV_DS3_H_
#define USBD_DEV_DS3_H_

#include <stdint.h>

#include <usbd_lib/usbd_core.h>
#include <usbd_lib/usbd_req.h>
#include <usbd_lib/usbd_conf.h>
#include <usbd_lib/usbd_ioreq.h>
#include <usbd_lib/usb_dcd.h>
#include <usbotg_lib/usb_regs.h>

//#define ENABLE_DS3_ADDITIONAL_FEATURES // experimental, untested, possible cheat giveaway

#define USBD_Dev_DS3_D2H_EP 0x81
#define USBD_Dev_DS3_H2D_EP 0x02
#define USBD_Dev_DS3_D2H_EP_ADDITIONAL 0x83
#define USBD_Dev_DS3_D2H_EP_SZ 64
#define USBD_Dev_DS3_H2D_EP_SZ 64
#define USBD_Dev_DS3_D2H_EP_ADDITIONAL_SZ 8

#define USBD_DEV_DS3_DEVICEDESCRIPTOR_SIZE 18

#ifdef ENABLE_DS3_ADDITIONAL_FEATURES
#define USBD_DEV_DS3_CONFIGURATIONDESCRIPTOR_SIZE (41 + 25)
#else
#define USBD_DEV_DS3_CONFIGURATIONDESCRIPTOR_SIZE 41
#endif

#define USBD_DEV_DS3_HIDREPORTDESCRIPTOR_SIZE 148
#define USBD_DEV_DS3_HIDREPORTDESCRIPTOR_ADDITIONAL_SIZE 121

uint8_t USBD_Dev_DS3_SendReport (USB_OTG_CORE_HANDLE *pcore,
                                uint8_t *report,
                                uint16_t len);
uint8_t USBD_Dev_DS3_ClassInit         (void *pcore , uint8_t cfgidx);
uint8_t USBD_Dev_DS3_ClassDeInit       (void *pcore , uint8_t cfgidx);
uint8_t USBD_Dev_DS3_Setup             (void *pcore , USB_SETUP_REQ  *req);
uint8_t USBD_Dev_DS3_EP0_TxSent        (void *pcore );
uint8_t USBD_Dev_DS3_EP0_RxReady       (void *pcore );
uint8_t USBD_Dev_DS3_DataIn            (void *pcore , uint8_t epnum);
uint8_t USBD_Dev_DS3_DataOut           (void *pcore , uint8_t epnum);
uint8_t USBD_Dev_DS3_SOF               (void *pcore);
uint8_t* USBD_Dev_DS3_GetDeviceDescriptor( uint8_t speed , uint16_t *length);
uint8_t* USBD_Dev_DS3_GetConfigDescriptor( uint8_t speed , uint16_t *length);
uint8_t* USBD_Dev_DS3_GetOtherConfigDescriptor( uint8_t speed , uint16_t *length);
uint8_t* USBD_Dev_DS3_GetUsrStrDescriptor( uint8_t speed , uint8_t index ,  uint16_t *length);
uint8_t* USBD_Dev_DS3_GetLangIDStrDescriptor( uint8_t speed , uint16_t *length);
uint8_t* USBD_Dev_DS3_GetManufacturerStrDescriptor( uint8_t speed , uint16_t *length);
uint8_t* USBD_Dev_DS3_GetProductStrDescriptor( uint8_t speed , uint16_t *length);
uint8_t* USBD_Dev_DS3_GetSerialStrDescriptor( uint8_t speed , uint16_t *length);
uint8_t* USBD_Dev_DS3_GetConfigurationStrDescriptor( uint8_t speed , uint16_t *length);
uint8_t* USBD_Dev_DS3_GetInterfaceStrDescriptor( uint8_t intf , uint8_t speed , uint16_t *length);
void USBD_Dev_DS3_UsrInit(void);
void USBD_Dev_DS3_DeviceReset(uint8_t speed);
void USBD_Dev_DS3_DeviceConfigured(void);
void USBD_Dev_DS3_DeviceSuspended(void);
void USBD_Dev_DS3_DeviceResumed(void);
void USBD_Dev_DS3_DeviceConnected(void);
void USBD_Dev_DS3_DeviceDisconnected(void);

extern USBD_Device_cb_TypeDef USBD_Dev_DS3_cb;
extern char USBD_Host_Is_PS3;
extern char USBD_Dev_DS3_IsActive;

#endif
