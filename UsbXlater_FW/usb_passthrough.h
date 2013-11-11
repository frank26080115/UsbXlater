#ifndef USB_PASSTHROUGH_H_
#define USB_PASSTHROUGH_H_

#include <usbh_lib/usbh_core.h>
#include <usbh_lib/usbh_hcs.h>
#include <usbh_lib/usbh_ioreq.h>
#include <usbh_lib/usbh_stdreq.h>
#include <usbd_lib/usbd_core.h>
#include <usbd_lib/usbd_req.h>
#include <usbd_lib/usbd_conf.h>
#include <usbd_lib/usbd_ioreq.h>
#include <usbd_lib/usb_dcd.h>
#include <usbotg_lib/usb_regs.h>
#include <stdio.h>
#include <stdarg.h>

extern USBD_Device_cb_TypeDef USBPT_Dev_cb;
extern USBH_Device_cb_TypeDef USBPT_Host_cb;

typedef struct
{
	int8_t used;
	int8_t hc;
	USBH_EpDesc_TypeDef* epDesc;
}
USBPTH_HC_EP_t;

#define USBPTH_MAX_LISTENERS 5

void USBPT_Init(USBH_DEV* pdev);
void USBPT_Work();
uint8_t USBPTD_SetupStage(USB_OTG_CORE_HANDLE* pcore, USB_SETUP_REQ* req);
void USBPTH_DeInitDev(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev);
USBH_Status USBPTH_Task(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev);
void USBPTH_Init(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev);
void USBPTH_DeInit(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev);
void USBPTH_DeviceAttached(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev);
void USBPTH_ResetDevice(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev);
void USBPTH_DeviceDisconnected(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev);
void USBPTH_OverCurrentDetected(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev);
void USBPTH_DeviceSpeedDetected(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev, uint8_t DeviceSpeed);
void USBPTH_DeviceDescAvailable(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev, void * data);
void USBPTH_DeviceAddressAssigned(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev);
void USBPTH_ConfigurationDescAvailable(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev,
								 USBH_CfgDesc_TypeDef * config,
								 USBH_InterfaceDesc_TypeDef * intf,
								 USBH_EpDesc_TypeDef ** epDesc);
void USBPTH_ManufacturerString(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev, void * data);
void USBPTH_ProductString(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev, void * data);
void USBPTH_SerialNumString(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev, void * data);
void USBPTH_EnumerationDone(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev);
void USBPTH_DeviceNotSupported(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev);
void USBPTH_UnrecoveredError(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev);
uint8_t USBPTD_ClassInit         (void *pcore , uint8_t cfgidx);
uint8_t USBPTD_ClassDeInit       (void *pcore , uint8_t cfgidx);
uint8_t USBPTD_Setup             (void *pcore , USB_SETUP_REQ  *req);
uint8_t USBPTD_EP0_TxSent        (void *pcore );
uint8_t USBPTD_DataIn            (void *pcore , uint8_t epnum);
uint8_t USBPTD_DataOut           (void *pcore , uint8_t epnum);
uint8_t USBPTD_EP0_RxReady       (void *pcore );
uint8_t USBPTD_SOF               (void *pcore);
uint8_t USBPTD_IsoINIncomplete   (void *pcore);
uint8_t USBPTD_IsoOUTIncomplete  (void *pcore);
uint8_t* USBPTD_GetDeviceDescriptor( uint8_t speed , uint16_t *length);
uint8_t* USBPTD_GetConfigDescriptor( uint8_t speed , uint16_t *length);
uint8_t* USBPTD_GetOtherConfigDescriptor( uint8_t speed , uint16_t *length);
uint8_t* USBPTD_GetUsrStrDescriptor( uint8_t speed , uint8_t index , uint16_t *length);
uint8_t* USBPTD_GetLangIDStrDescriptor( uint8_t speed , uint16_t *length);
uint8_t* USBPTD_GetManufacturerStrDescriptor( uint8_t speed , uint16_t *length);
uint8_t* USBPTD_GetProductStrDescriptor( uint8_t speed , uint16_t *length);
uint8_t* USBPTD_GetSerialStrDescriptor( uint8_t speed , uint16_t *length);
uint8_t* USBPTD_GetConfigurationStrDescriptor( uint8_t speed , uint16_t *length);
uint8_t* USBPTD_GetInterfaceStrDescriptor( uint8_t intf , uint8_t speed , uint16_t *length);
void USBPTD_UsrInit(void);
void USBPTD_DeviceReset(uint8_t speed);
void USBPTD_DeviceConfigured(void);
void USBPTD_DeviceSuspended(void);
void USBPTD_DeviceResumed(void);
void USBPTD_DeviceConnected(void);
void USBPTD_DeviceDisconnected(void);
void USBPT_printf(char * format, ...);

#endif
