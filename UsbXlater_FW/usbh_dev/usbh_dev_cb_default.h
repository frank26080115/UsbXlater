#ifndef usbh_dev_cb_default_h
#define usbh_dev_cb_default_h

#include <usbh_lib/usbh_core.h>

void USBH_Dev_DefaultCB_DeInitDev(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
USBH_Status USBH_Dev_DefaultCB_Task(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_DefaultCB_Init(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_DefaultCB_DeInit(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_DefaultCB_DeviceAttached(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_DefaultCB_ResetDevice(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_DefaultCB_DeviceDisconnected(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_DefaultCB_OverCurrentDetected(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_DefaultCB_DeviceSpeedDetected(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, uint8_t DeviceSpeed);
void USBH_Dev_DefaultCB_DeviceDescAvailable(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, void* data_);
void USBH_Dev_DefaultCB_DeviceAddressAssigned(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_DefaultCB_ConfigurationDescAvailable(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, USBH_CfgDesc_TypeDef *, USBH_InterfaceDesc_TypeDef *, USBH_EpDesc_TypeDef **);
void USBH_Dev_DefaultCB_ManufacturerString(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, void * data_);
void USBH_Dev_DefaultCB_ProductString(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, void * data_);
void USBH_Dev_DefaultCB_SerialNumString(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, void * data_);
void USBH_Dev_DefaultCB_EnumerationDone(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_DefaultCB_DeviceNotSupported(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_DefaultCB_UnrecoveredError(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);

extern USBH_Device_cb_TypeDef USBH_Dev_CB_Default;

#endif
