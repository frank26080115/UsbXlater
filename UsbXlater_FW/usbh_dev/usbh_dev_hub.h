#ifndef usbh_dev_hub_h
#define usbh_dev_hub_h

#include <usbh_lib/usbh_core.h>

void USB_Hub_Hardware_Init();
void USB_Hub_Hardware_Reset();

void USBH_Dev_Hub_DeInitDev(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
USBH_Status USBH_Dev_Hub_Task(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_Hub_Init(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_Hub_DeInit(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_Hub_DeviceAttached(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_Hub_ResetDevice(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_Hub_DeviceDisconnected(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
//void USBH_Dev_Hub_OverCurrentDetected(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_Hub_DeviceSpeedDetected(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, uint8_t DeviceSpeed);
void USBH_Dev_Hub_DeviceDescAvailable(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, void* data_);
void USBH_Dev_Hub_DeviceAddressAssigned(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_Hub_ConfigurationDescAvailable(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, USBH_CfgDesc_TypeDef *, USBH_InterfaceDesc_TypeDef *, USBH_EpDesc_TypeDef **);
//void USBH_Dev_Hub_ManufacturerString(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, void * data_);
//void USBH_Dev_Hub_ProductString(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, void * data_);
//void USBH_Dev_Hub_SerialNumString(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, void * data_);
void USBH_Dev_Hub_EnumerationDone(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_Hub_DeviceNotSupported(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_Hub_UnrecoveredError(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);

extern USBH_Device_cb_TypeDef USBH_Dev_Hub_CB;

void USBH_Dev_Hub_Handle_InterruptIn(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, void* data_);
USBH_Status USBH_Dev_Hub_SetPortFeature(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t port, uint16_t feature);
USBH_Status USBH_Dev_Hub_GetPortStatus(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t port, uint16_t* wPortStatus, uint16_t* wPortChange);
USBH_Status USBH_Dev_Hub_ClearPortFeature(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t port, uint16_t feature, uint8_t selector);

#define HUB_MIN_POLL	10

typedef enum
{
	HubState_IDLE = 0,
	HubState_SEND_DATA,
	HubState_BUSY,
	HubState_GET_DATA,
	HubState_SYNC,
	HubState_POLL,
	HubState_ERROR,
}
Hub_State;

typedef struct
{
	Hub_State            state;
	uint8_t              buff[64];
	int8_t               hc_num_in;
	int8_t               hc_num_out;
	uint8_t              intOutEp;
	uint8_t              intInEp;
	uint16_t             length;
	uint8_t              ep_addr;
	uint16_t             poll;
	__IO uint16_t        timer;
	//USB_Setup_TypeDef    setup;
	uint8_t              num_ports;
	uint8_t              port_busy;
	uint8_t              has_control;
	USBH_DEV**           children;
	uint8_t              start_toggle;
}
Hub_Data_t;

// hubs can afford to miss some interrupt-in transactions
// because if you miss one, it'll just come again
// so dynamically allocating HCs is OK since there's no risk of missing
// something forever
#define HUB_ENABLE_DYNAMIC_HC_ALLOC

#define USB_DESC_TYPE_HUB	0x29
#define USB_DESC_HUB		((USB_DESC_TYPE_HUB << 8) & 0xFF00)
#define USB_HUB_DESC_SIZE	9

// these definitions are from official USB specifications

#define HUBREQ_C_HUB_LOCAL_POWER	0
#define HUBREQ_C_HUB_OVER_CURRENT	1
#define HUBREQ_PORT_CONNECTION		0
#define HUBREQ_PORT_ENABLE			1
#define HUBREQ_PORT_SUSPEND			2
#define HUBREQ_PORT_OVER_CURRENT	3
#define HUBREQ_PORT_RESET			4
#define HUBREQ_PORT_POWER			8
#define HUBREQ_PORT_LOW_SPEED		9
#define HUBREQ_C_PORT_CONNECTION	16
#define HUBREQ_C_PORT_ENABLE		17
#define HUBREQ_C_PORT_SUSPEND		18
#define HUBREQ_C_PORT_OVER_CURRENT	19
#define HUBREQ_C_PORT_RESET			20
#define HUBREQ_PORT_TEST			21
#define HUBREQ_PORT_INDICATOR		22

#define HUBWPORTSTATUS_CURCONN_BIT 0
#define HUBWPORTSTATUS_ENABLED_BIT 1
#define HUBWPORTSTATUS_SUSPEND_BIT 2
#define HUBWPORTSTATUS_OVERCUR_BIT 3
#define HUBWPORTSTATUS_RESET_BIT   4
#define HUBWPORTSTATUS_POWER_BIT   8
#define HUBWPORTSTATUS_LSDEVATTACHED_BIT 9
#define HUBWPORTSTATUS_HSDEVATTACHED_BIT 10
#define HUBWPORTSTATUS_TEST_BIT 11
#define HUBWPORTSTATUS_INDICATOR_BIT 12

#define HUBWPORTCHANGE_CONNSTAT_BIT 0
#define HUBWPORTCHANGE_ENABLED_BIT  1
#define HUBWPORTCHANGE_SUSPEND_BIT  2
#define HUBWPORTCHANGE_OVERCUR_BIT  3
#define HUBWPORTCHANGE_RESET_BIT    4

#endif
