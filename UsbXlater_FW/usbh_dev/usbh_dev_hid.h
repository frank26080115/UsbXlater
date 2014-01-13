#ifndef usbh_dev_hid_h
#define usbh_dev_hid_h

#include <usbh_lib/usbh_core.h>
#include <stdint.h>
#include <hidrpt.h>

void USBH_Dev_HID_DeInitDev(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
USBH_Status USBH_Dev_HID_Task(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_HID_Init(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_HID_DeInit(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_HID_FreeData(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_HID_DeviceAttached(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_HID_ResetDevice(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_HID_DeviceDisconnected(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_HID_OverCurrentDetected(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_HID_DeviceSpeedDetected(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, uint8_t DeviceSpeed);
void USBH_Dev_HID_DeviceDescAvailable(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, void* data_);
void USBH_Dev_HID_DeviceAddressAssigned(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_HID_ConfigurationDescAvailable(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, USBH_CfgDesc_TypeDef *, USBH_InterfaceDesc_TypeDef *, USBH_EpDesc_TypeDef **);
void USBH_Dev_HID_EnumerationDone(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_HID_DeviceNotSupported(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);
void USBH_Dev_HID_UnrecoveredError(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev);

void HID_Init_Default_Handler(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, uint8_t intf);
void HID_Decode_Default_Handler(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, uint8_t intf, uint8_t ep, uint8_t* data, uint8_t len);

USBH_Status USBH_Get_Report (USB_OTG_CORE_HANDLE *pcore,
                                 USBH_DEV *pdev,
                                    uint8_t intf,
                                    uint8_t reportType,
                                    uint8_t reportId,
                                    uint8_t reportLen,
                                    uint8_t* reportBuff);

USBH_Status USBH_Set_Report (USB_OTG_CORE_HANDLE *pcore,
                                 USBH_DEV *pdev,
                                    uint8_t intf,
                                    uint8_t reportType,
                                    uint8_t reportId,
                                    uint8_t reportLen,
                                    uint8_t* reportBuff);

extern USBH_Device_cb_TypeDef USBH_Dev_CB_HID;

#define HID_MIN_POLL	10

typedef enum
{
	HID_IDLE= 0,
	HID_SEND_DATA,
	HID_BUSY,
	HID_GET_DATA,
	HID_SYNC,
	HID_POLL,
	HID_ERROR,
}
HID_State;

typedef struct HID_cb
{
	void  (*Init)   (USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, uint8_t intf);
	void  (*Decode) (USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, uint8_t intf, uint8_t ep, uint8_t *data, uint8_t len);
	void  (*DeInit) (USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, uint8_t intf);
} HID_cb_TypeDef;

// there's no use for this struct in UsbXlater yet
typedef  struct  _HID_Report
{
	uint8_t   ReportID;
	uint8_t   ReportType;
	uint16_t  UsagePage;
	uint32_t  Usage[2];
	uint32_t  NbrUsage;
	uint32_t  UsageMin;
	uint32_t  UsageMax;
	int32_t   LogMin;
	int32_t   LogMax;
	int32_t   PhyMin;
	int32_t   PhyMax;
	int32_t   UnitExp;
	uint32_t  Unit;
	uint32_t  ReportSize;
	uint32_t  ReportCnt;
	uint32_t  Flag;
	uint32_t  PhyUsage;
	uint32_t  AppUsage;
	uint32_t  LogUsage;
}
HID_Report_TypeDef;

typedef struct
{
	USBH_HIDDesc_TypeDef	HID_Desc;
	char					start_toggle;
	uint8_t              buff[64];
	int8_t               hc_num_in;
	int8_t               hc_num_out;
	HID_State            state;
	uint8_t              HIDIntOutEp;
	uint8_t              HIDIntInEp;
	uint16_t             length;
	uint8_t              ep_addr;
	uint16_t             poll;
	__IO uint16_t        timer;
	HID_cb_TypeDef       *cb;
	HID_Rpt_Parsing_Params_t parserParams;
}
HID_Data_t;

// Dynamic HC allocation for HID devices is tricky
// keyboards cannot afford to miss any events or else you end up with sticky keys
// mouse events come much more frequently and missing one will not be too much
// of an issue
// but we leave it statically allocated instead of dynamic right now
// just in case, it does perform better, we just have less HCs to work with
//#define USBH_HID_ENABLE_DYNAMIC_HC_ALLOC

#define USB_HID_REQ_GET_REPORT       0x01
#define USB_HID_GET_IDLE             0x02
#define USB_HID_GET_PROTOCOL         0x03
#define USB_HID_SET_REPORT           0x09
#define USB_HID_SET_IDLE             0x0A
#define USB_HID_SET_PROTOCOL         0x0B

extern char USBH_Dev_Has_HID;

#endif
