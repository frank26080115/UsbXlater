#ifndef usbh_dev_bt_hci_h
#define usbh_dev_bt_hci_h

#include <usbh_lib/usbh_core.h>
#include <usbh_dev/usbh_devio_manager.h>

void USBH_Dev_BtHci_DeInitDev(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
USBH_Status USBH_Dev_BtHci_Task(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_BtHci_Init(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_BtHci_DeInit(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_BtHci_DeviceAttached(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_BtHci_ResetDevice(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_BtHci_DeviceDisconnected(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
//void USBH_Dev_BtHci_OverCurrentDetected(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_BtHci_DeviceSpeedDetected(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, uint8_t DeviceSpeed);
void USBH_Dev_BtHci_DeviceDescAvailable(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, void* data_);
void USBH_Dev_BtHci_DeviceAddressAssigned(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_BtHci_ConfigurationDescAvailable(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, USBH_CfgDesc_TypeDef *, USBH_InterfaceDesc_TypeDef *, USBH_EpDesc_TypeDef **);
//void USBH_Dev_BtHci_ManufacturerString(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, void * data_);
//void USBH_Dev_BtHci_ProductString(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, void * data_);
//void USBH_Dev_BtHci_SerialNumString(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, void * data_);
void USBH_Dev_BtHci_EnumerationDone(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_BtHci_DeviceNotSupported(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_BtHci_UnrecoveredError(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);

extern USBH_Device_cb_TypeDef USBH_Dev_BtHci_CB;

USBH_Status USBH_Dev_BtHci_Command(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t* data_, int len);
USBH_Status USBH_Dev_BtHci_TxData(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, uint8_t* data, int len);
char USBH_Dev_BtHci_CanSendPacket(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);

typedef struct
{
	USBH_DevIO_t* eventIO;
	USBH_DevIO_t* dataInIO;
	USBH_DevIO_t* dataOutIO;
	uint8_t ioIdx;
	void (*packet_handler)(uint8_t packet_type, uint8_t *packet, int size);
}
UsbHci_Data_t;

//#define USBHCI_ENABLE_DYNAMIC_HC_ALLOC

#endif
