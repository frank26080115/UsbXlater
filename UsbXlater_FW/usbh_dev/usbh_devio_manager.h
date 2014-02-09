#ifndef USBH_INTF_MANAGER_H_
#define USBH_INTF_MANAGER_H_

#include <stdint.h>
#include <cmsis/core_cmx.h>
#include <usbotg_lib/usb_core.h>
#include <usbh_lib/usbh_core.h>
#include <usbh_lib/usbh_def.h>

typedef enum
{
	UIOSTATE_IDLE = 0,
	UIOSTATE_START,
	UIOSTATE_GET_DATA,
	UIOSTATE_SEND_DATA,
	UIOSTATE_WAIT_DATA,
	UIOSTATE_FINALIZE,
	UIOSTATE_NEXT,
	UIOSTATE_ERROR,
}
USBH_DevIO_State_t;

typedef void (*USBH_DevD2H_DataHandler_t)(void* p_io, uint8_t* data, uint16_t len);

typedef struct
{
	void* next_node;
	USB_OTG_CORE_HANDLE* pcore;
	USBH_DEV* pdev;
	USBH_EpDesc_TypeDef* ep;
	int8_t hc;
	uint8_t* buff;
	uint8_t* out_ptr;
	uint16_t remaining;
	int timeout;
	uint8_t retries;
	char hc_as_needed;
	char hc_has_error;
	__IO uint16_t timer;
	char start_toggle;
	USBH_DevIO_State_t state;
	USBH_DevD2H_DataHandler_t data_handler;
	void* extra;
}
USBH_DevIO_t;

void USBH_DevD2HManager_Init();
USBH_DevIO_t* USBH_DevIO_Manager_New(USB_OTG_CORE_HANDLE* pcore, USBH_DEV* pdev, USBH_EpDesc_TypeDef* ep, char hc_as_needed, USBH_DevD2H_DataHandler_t data_handler, void* extra);
void USBH_DevIO_Manager_Unplug(USBH_DEV* pdev);
void USBH_DevIO_Task();
USBH_Status USBH_DevIO_TxData_Blocking(USB_OTG_CORE_HANDLE* pcore, USBH_DEV* pdev, USBH_EpDesc_TypeDef* ep, uint8_t* data, uint16_t len);

#endif
