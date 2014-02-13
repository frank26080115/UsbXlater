#ifndef USBH_INTF_MANAGER_H_
#define USBH_INTF_MANAGER_H_

#include <stdint.h>
#include <cmsis/core_cmx.h>
#include <usbotg_lib/usb_core.h>
#include <usbh_lib/usbh_core.h>
#include <usbh_lib/usbh_def.h>

#define ENABLE_USBDEVIO_STATISTICS
//#define ENABLE_EVENODDFRAME_WAIT
//#define FORCE_DEVIO_ORDERED_TASK
#define ENABLE_DEVIO_INTR_WAITEVENODDFRAME

typedef enum
{
	UIOSTATE_IDLE = 0,
	UIOSTATE_START,
	UIOSTATE_SEND_REQUEST,
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
	char enabled;
	USB_OTG_CORE_HANDLE* pcore;
	USBH_DEV* pdev;
	USBH_EpDesc_TypeDef* ep;
	int8_t hc;
	int8_t hc_toggle_in;
	int8_t hc_toggle_out;
	uint8_t* buff;
	uint8_t* out_ptr;
	uint16_t remaining;
	int timeout;
	int evenodd_frm_cnt;
	uint8_t retries;
	char hc_as_needed;
	char hc_has_error;
	uint32_t timestamp;
	char start_toggle;
	USBH_DevIO_State_t state;
	USBH_DevD2H_DataHandler_t data_handler;
	void* extra;
	char force_poll_interval;
	int32_t poll_interval;
	uint32_t priority;
	uint32_t priority_countdown;

	// statistics
	int stat_timestamp;
	int req_cnt;
	int notidle_cnt;
	int done_cnt;
	int err_cnt;
	int nak_cnt;
	int stall_cnt;
	int timeout_cnt;
}
USBH_DevIO_t;

#ifdef ENABLE_USBDEVIO_STATISTICS
typedef struct
{
	int dev_cnt;
	int req_cnt;
	int notidle_cnt;
	int done_cnt;
	int err_cnt;
	int nak_cnt;
	int stall_cnt;
	int timeout_cnt;
	int intr_req_cnt;
}
USBH_DevIO_Statistics_t;

extern USBH_DevIO_Statistics_t USBH_DevIO_Statistics;
#endif

void USBH_DevIOManager_Init();
USBH_DevIO_t* USBH_DevIO_Manager_New(USB_OTG_CORE_HANDLE* pcore, USBH_DEV* pdev, USBH_EpDesc_TypeDef* ep, char hc_as_needed, USBH_DevD2H_DataHandler_t data_handler, void* extra);
void USBH_DevIO_Manager_Unplug(USBH_DEV* pdev);
char USBH_DevIO_Task();
void USBH_DevIO_Reset_HC(USB_OTG_CORE_HANDLE *pcore, int8_t hc);

#endif
