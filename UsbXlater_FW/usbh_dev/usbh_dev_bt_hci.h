#ifndef usbh_dev_bt_hci_h
#define usbh_dev_bt_hci_h

#include <usbh_lib/usbh_core.h>
#include <bluetooth/hci.h>

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

void USBH_Dev_BtHci_Handle_InterruptIn(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, void* data_);
void USBH_Dev_BtHci_Handle_BulkIn(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, void* data_);

#define USBHCI_MIN_POLL	10

typedef enum
{
	UsbHciState_IDLE = 0,
	UsbHciState_SEND_DATA,
	UsbHciState_BUSY,
	UsbHciState_GET_DATA,
	UsbHciState_SYNC,
	UsbHciState_POLL,
	UsbHciState_ERROR,
}
UsbHci_State;

typedef struct
{
	UsbHci_State         state;
	uint8_t              buff[64];
	int8_t               hc_num_intin;
	int8_t               hc_num_bulkin;
	int8_t               hc_num_bulkout;
	uint8_t              intInEp;
	uint8_t              bulkOutEp;
	uint8_t              bulkInEp;
	uint16_t             length;
	uint8_t              ep_addr;
	uint16_t             poll;
	__IO uint16_t        timer;
	//USB_Setup_TypeDef    setup;
	uint8_t              num_ports;
	uint8_t              port_busy;
	uint8_t              check_bulk;
	uint8_t              start_toggle;
	BTHCI_t              bthci;
}
UsbHci_Data_t;

//#define USBHCI_ENABLE_DYNAMIC_HC_ALLOC

#endif
