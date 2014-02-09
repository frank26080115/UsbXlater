#include "usbh_devio_manager.h"
#include "usbh_dev_manager.h"
#include <usbotg_lib/usb_core.h>
#include <usbh_lib/usbh_core.h>
#include <usbh_lib/usbh_def.h>
#include <usbh_lib/usbh_ioreq.h>
#include <usbh_lib/usbh_hcs.h>
#include <usbh_lib/usb_hcd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void USBH_DevIO_Manager_Add(USBH_DevIO_t* x);
void USBH_DevIO_Manager_Remove(USBH_DevIO_t* x);

USBH_DevIO_t* USBH_DevIO_HeadNode;
USBH_DevIO_t* USBH_DevIO_CurrentNode;

void USBH_DevD2HManager_Init()
{
	USBH_DevIO_HeadNode = 0;
	USBH_DevIO_CurrentNode = 0;
}

void USBH_DevIO_DummyHandler(void* p_io, uint8_t* data, uint16_t len)
{
}

USBH_DevIO_t* USBH_DevIO_Manager_New(
		USB_OTG_CORE_HANDLE* pcore,
		USBH_DEV* pdev,
		USBH_EpDesc_TypeDef* ep,
		char hc_as_needed,
		USBH_DevD2H_DataHandler_t data_handler,
		void* extra
	)
{
	USBH_DevIO_t* i = (USBH_DevIO_t*)malloc(sizeof(USBH_DevIO_t));

	if (i == 0) {
		dbg_printf(DBGMODE_TRACE, " error no memory\r\n");
		return 0;
	}

	i->next_node = 0;
	i->hc = -1;
	i->pcore = pcore;
	i->pdev = pdev;
	i->ep = ep;
	i->timeout = i->ep->bInterval < 10 ? 10 : i->ep->bInterval;
	i->data_handler = data_handler;
	i->start_toggle = 0;
	i->state = UIOSTATE_IDLE;
	i->buff = (uint8_t*)malloc(i->ep->wMaxPacketSize);
	i->extra = extra;
	i->remaining = 0;
	i->retries = 0;

	if (i->data_handler == 0) {
		i->data_handler = USBH_DevIO_DummyHandler;
	}

	i->hc_as_needed = hc_as_needed;

	if (i->hc_as_needed == 0)
	{
		if (USBH_Open_Channel(
			i->pcore,
			&(i->hc),
			i->ep->bEndpointAddress,
			i->pdev->device_prop.address,
			i->pdev->device_prop.speed,
			i->ep->bmAttributes & EP_TYPE_MSK,
			i->ep->wMaxPacketSize
		) != HC_OK) {
			dbg_printf(DBGMODE_ERR, "Failed to open HC for %s\r\n", USBH_Dev_DebugPrint(i->pdev, i->ep));
		}
		else {
			i->hc_has_error = 0;
			dbg_printf(DBGMODE_DEBUG, "Opened HC for %s:HC%d\r\n", USBH_Dev_DebugPrint(i->pdev, i->ep), i->hc);
		}
	}

	USBH_DevIO_Manager_Add(i);

	return i;
}

void USBH_DevIO_Manager_Add(USBH_DevIO_t* x)
{
	// check if already exists
	USBH_DevIO_t* i;
	for (i = USBH_DevIO_HeadNode; i != 0; i = i->next_node)
	{
		if (i == x) {
			return; // do not add, already exists
		}
	}
	// add as head
	USBH_DevIO_t* prev_head = USBH_DevIO_HeadNode;
	x->next_node = prev_head;
	USBH_DevIO_HeadNode = x;
}

void USBH_DevIO_Manager_Remove(USBH_DevIO_t* x)
{
	if (x->hc >= 0)
	{
		USBH_Free_Channel(x->pcore, &(x->hc));
	}

	if (x->buff != 0) {
		free(x->buff);
	}

	USBH_DevIO_t* i;
	for (i = USBH_DevIO_HeadNode; i != 0; i = i->next_node)
	{
		if (i->next_node == x) {
			i->next_node = x->next_node;
		}
	}

	if (USBH_DevIO_CurrentNode == x)
	{
		if (x->next_node == 0) {
			USBH_DevIO_CurrentNode = USBH_DevIO_HeadNode;
		}
		else {
			USBH_DevIO_CurrentNode = x->next_node;
		}
	}
}

void USBH_DevIO_Manager_Unplug(USBH_DEV* pdev)
{
	USBH_DevIO_t* i;
	USBH_DevIO_t* n;
	for (i = USBH_DevIO_HeadNode; i != 0;)
	{
		if (i->pdev == pdev) {
			n = i->next_node;
			USBH_DevIO_Manager_Remove(i);
			free(i);
			i = n;
		}
		else {
			i = i->next_node;
		}
	}
}

void USBH_DevIO_Task(USBH_DevIO_t* p_io)
{
	USBH_DevIO_t* ci;
	if (p_io == 0)
	{
		if (USBH_DevIO_CurrentNode == 0) {
			USBH_DevIO_CurrentNode = USBH_DevIO_HeadNode;
		}
		if (USBH_DevIO_CurrentNode == 0) {
			return;
		}
		ci = USBH_DevIO_CurrentNode;
	}
	else
	{
		ci = p_io;
	}

	uint8_t epType = ci->ep->bmAttributes & EP_TYPE_MSK;
	uint8_t epDir = ci->ep->bEndpointAddress & USB_EP_DIR_MSK;
	uint16_t epSize = ci->ep->wMaxPacketSize;
	uint16_t sendSize;
	USBH_Status sts;
	URB_STATE urb;

	switch (ci->state)
	{
		case UIOSTATE_IDLE:
			ci->start_toggle = 0;
			ci->state = UIOSTATE_START;
			break;
		case UIOSTATE_START:
			if (epDir == USB_D2H)
			{
				ci->state = UIOSTATE_GET_DATA;
			}
			else
			{
				ci->retries = 5;
				if (ci->remaining > 0) {
					ci->state = UIOSTATE_SEND_DATA;
				}
				else {
					ci->state = UIOSTATE_NEXT;
				}
				break;
			}
			// fall through
		case UIOSTATE_GET_DATA:
			if (USB_OTG_IsEvenFrame(ci->pcore) != 0)
			{
				if (ci->hc < 0)
				{
					if (USBH_Open_Channel(
						ci->pcore,
						&(ci->hc),
						ci->ep->bEndpointAddress,
						ci->pdev->device_prop.address,
						ci->pdev->device_prop.speed,
						epType,
						epSize
					) != HC_OK) {
						dbg_printf(DBGMODE_ERR, "Failed to quick-open HC for %s\r\n", USBH_Dev_DebugPrint(ci->pdev, ci->ep));
					}
					else {
						ci->hc_has_error = 0;
						if (ci->hc_as_needed != 0) {
							ci->hc_as_needed = 1;
						}
						//dbg_printf(DBGMODE_DEBUG, "Quickly opened HC for %s:HC%d\r\n", USBH_Dev_DebugPrint(ci->pdev, ci->ep), ci->hc);
					}
				}
				if (ci->hc < 0) {
					ci->state = UIOSTATE_FINALIZE;
					break;
				}
				ci->timer = HCD_GetCurrentFrame(ci->pcore);
				if (epType == EP_TYPE_ISOC)
				{
					if (sts = USBH_IsocReceiveData(ci->pcore, ci->buff, epSize, ci->hc) == USBH_OK) {
						ci->start_toggle = 1;
						ci->state = UIOSTATE_WAIT_DATA;
						break;
					}
				}
				else if (epType == EP_TYPE_BULK) {
					if (sts = USBH_BulkReceiveData(ci->pcore, ci->buff, epSize, ci->hc) == USBH_OK) {
						ci->start_toggle = 1;
						ci->state = UIOSTATE_WAIT_DATA;
						break;
					}
				}
				else if (epType == EP_TYPE_INTR) {
					if (sts = USBH_InterruptReceiveData(ci->pcore, ci->buff, epSize, ci->hc) == USBH_OK) {
						ci->start_toggle = 1;
						ci->state = UIOSTATE_WAIT_DATA;
						break;
					}
				}
				else {
					dbg_printf(DBGMODE_ERR, "USBH_DevIO_Task UIOSTATE_GET_DATA %s error unknown EP type 0x%02X\r\n", USBH_Dev_DebugPrint(ci->pdev, ci->ep), ci->ep->bmAttributes);
				}
			}
			break;
		case UIOSTATE_SEND_DATA:
			if (ci->hc < 0)
			{
				if (USBH_Open_Channel(
					ci->pcore,
					&(ci->hc),
					ci->ep->bEndpointAddress,
					ci->pdev->device_prop.address,
					ci->pdev->device_prop.speed,
					epType,
					epSize
				) != HC_OK) {
					dbg_printf(DBGMODE_ERR, "Failed to quick open HC for %s\r\n", USBH_Dev_DebugPrint(ci->pdev, ci->ep));
				}
				else {
					ci->hc_has_error = 0;
					if (ci->hc_as_needed != 0) {
						ci->hc_as_needed = 1;
					}
					//dbg_printf(DBGMODE_DEBUG, "Quickly opened HC for %s:HC%d\r\n", USBH_Dev_DebugPrint(ci->pdev, ci->ep), ci->hc);
				}
			}
			if (ci->hc < 0) {
				ci->state = UIOSTATE_FINALIZE;
				break;
			}
			ci->timer = HCD_GetCurrentFrame(ci->pcore);
			sendSize = epSize > ci->remaining ? ci->remaining : epSize;
			memcpy(ci->buff, ci->out_ptr, sendSize);
			if (epType == EP_TYPE_ISOC)
			{
				USBH_IsocSendData(ci->pcore, ci->buff, sendSize, ci->hc);
				ci->state = UIOSTATE_WAIT_DATA;
				break;
			}
			else if (epType == EP_TYPE_BULK) {
				USBH_BulkSendData(ci->pcore, ci->buff, sendSize, ci->hc);
				ci->state = UIOSTATE_WAIT_DATA;
			}
			else if (epType == EP_TYPE_INTR) {
				USBH_InterruptSendData(ci->pcore, ci->buff, sendSize, ci->hc);
				ci->state = UIOSTATE_WAIT_DATA;
				break;
			}
			else {
				dbg_printf(DBGMODE_ERR, "USBH_DevIO_Task UIOSTATE_SEND_DATA %s error unknown EP type 0x%02X\r\n", USBH_Dev_DebugPrint(ci->pdev, ci->ep), ci->ep->bmAttributes);
				ci->state = UIOSTATE_FINALIZE;
				break;
			}
			break;
		case UIOSTATE_WAIT_DATA:
			urb = HCD_GetURB_State(ci->pcore, ci->hc);
			if (urb == URB_DONE)
			{
				if (ci->start_toggle != 0 && epDir == USB_D2H)
				{
					ci->data_handler(ci, ci->buff, ci->pcore->host.XferCnt[ci->hc]);
					ci->start_toggle = 0;
					ci->state = UIOSTATE_FINALIZE;
					// fall through
				}
				else if (epDir == USB_H2D)
				{
					sendSize = ci->pcore->host.XferCnt[ci->hc];
					ci->remaining -= sendSize;
					if (ci->remaining <= 0) {
						ci->state = UIOSTATE_FINALIZE;
						ci->remaining = 0;
						// fall through
					}
					else {
						ci->out_ptr = &ci->out_ptr[sendSize];
						ci->state = UIOSTATE_SEND_DATA;
						break;
					}
				}
			}
			else if (urb == URB_STALL)
			{
				dbg_printf(DBGMODE_ERR, "UIOSTATE_WAIT_DATA URB_STALL %s\r\n", USBH_Dev_DebugPrint(ci->pdev, ci->ep));
				if (epDir == USB_H2D && (ci->retries--) > 0) {
					ci->state = UIOSTATE_SEND_DATA;
					break;
				}
				ci->state = UIOSTATE_FINALIZE;
			}
			else if (urb == URB_NOTREADY)
			{ // NAK, no data available
				if (epDir == USB_H2D && (ci->retries--) > 0) {
					ci->state = UIOSTATE_SEND_DATA;
					break;
				}
				ci->state = UIOSTATE_FINALIZE;
			}
			else if (urb == URB_ERROR)
			{
				ci->hc_has_error = 1;
				dbg_printf(DBGMODE_ERR, "UIOSTATE_WAIT_DATA URB_ERROR %s, HC_STATE %d\r\n", USBH_Dev_DebugPrint(ci->pdev, ci->ep), HCD_GetHCState(ci->pcore, ci->hc));
				if (epDir == USB_H2D && (ci->retries--) > 0) {
					ci->state = UIOSTATE_SEND_DATA;
					break;
				}
				ci->state = UIOSTATE_FINALIZE;
			}
			else if (urb == URB_IDLE)
			{
				if ((HCD_GetCurrentFrame(ci->pcore) - ci->timer) > ci->timeout) {
					ci->state = UIOSTATE_FINALIZE;
					if (ci->hc_as_needed != 0) {
						ci->hc_as_needed = 2;
						// signal that finalization was due to timeout
						// closing the HC while IDLE causes HC_DATATGLERR
					}
				}
				else {
					break; // stay in the waiting state
				}
			}
			// fall through
		case UIOSTATE_FINALIZE:
			ci->start_toggle = 0;
			ci->state = UIOSTATE_NEXT;
			if (ci->hc >= 0 && (ci->hc_as_needed == 1 || ci->hc_has_error != 0))
			{
				if (ci->hc_has_error != 0) {
					HC_STATUS hcsts = HCD_GetHCState(ci->pcore, ci->hc);
					dbg_printf(DBGMODE_ERR, "DevIO %s has error, HC_STATUS %d\r\n", USBH_Dev_DebugPrint(ci->pdev, ci->ep), hcsts);
					if (hcsts == HC_DATATGLERR) {
						static int hcerrCnt = 0;
						hcerrCnt++;
						while (hcerrCnt > 128);
					}
				}
				USBH_Free_Channel(ci->pcore, &(ci->hc));
				ci->hc_has_error = 0;
			}
			break;
		case UIOSTATE_NEXT:
			ci->state = UIOSTATE_START;
			if (p_io == 0) {
				USBH_DevIO_CurrentNode = (ci->next_node != 0) ? ci->next_node : USBH_DevIO_HeadNode;
			}
			break;
		default:
			ci->state = UIOSTATE_START;
			break;
	}
}

USBH_Status USBH_DevIO_TxData_Blocking(USB_OTG_CORE_HANDLE* pcore, USBH_DEV* pdev, USBH_EpDesc_TypeDef* ep, uint8_t* data, uint16_t len)
{
	int8_t hc = -1;
	if ((ep->bEndpointAddress & USB_EP_DIR_MSK) == USB_EP_DIR_IN) return USBH_NOT_SUPPORTED;

	uint8_t epType = ep->bmAttributes & EP_TYPE_MSK;

	if (USBH_Open_Channel(
			pcore,
			&hc,
			ep->bEndpointAddress,
			pdev->device_prop.address,
			pdev->device_prop.speed,
			epType,
			ep->wMaxPacketSize
			) == HC_ERROR
		) {
		return USBH_BUSY;
	}

	URB_STATE urb;
	uint32_t timeout = (len + 1) * 8;
	uint32_t t = systick_1ms_cnt;
	int remaining = len;
	int idx = 0;
	for (idx = 0; remaining > 0 && (systick_1ms_cnt - t) < timeout; )
	{
		int can_send = ep->wMaxPacketSize;
		can_send = (can_send > remaining) ? remaining : can_send;

		if (epType == EP_TYPE_ISOC) {
			USBH_IsocSendData(pcore, &data[idx], can_send, hc);
		}
		else if (epType == EP_TYPE_BULK) {
			USBH_BulkSendData(pcore, &data[idx], can_send, hc);
		}
		else if (epType == EP_TYPE_INTR) {
			USBH_InterruptSendData(pcore, &data[idx], can_send, hc);
		}

		while ((systick_1ms_cnt - t) < 100)
		{
			urb = HCD_GetURB_State(pcore, hc);
			if (urb == URB_DONE) {
				idx += can_send;
				remaining -= can_send;
				break;
			}
		}
	}

	USBH_Free_Channel(pcore, &hc);
	if (urb == URB_STALL) {
		return USBH_STALL;
	}
	else if (urb != URB_DONE) {
		return USBH_FAIL;
	}
	return USBH_OK;
}

USBH_Status USBH_CtlReq_Blocking (	USB_OTG_CORE_HANDLE *pcore,
									USBH_DEV            *pdev,
									uint8_t             *buff,
									uint16_t            length)
{
	int8_t alloc = USBH_Dev_AllocControl(pcore, pdev);

	dbgwdg_feed();

	USBH_Status status;
	USBH_Status finalStatus;

	uint32_t timeout = (length + 8) * 100;
	uint32_t t = systick_1ms_cnt;

	while ((systick_1ms_cnt - t) < timeout && pdev->Control.state != CTRL_COMPLETE && pdev->Control.state != CTRL_IDLE && pdev->Control.state != CTRL_ERROR && pdev->Control.state != CTRL_STALLED);
	{
		status = USBH_HandleControl(pcore, pdev);
	}

	if (pdev->RequestState == CMD_WAIT) {
		USBH_CtlReq(pcore, pdev, 0 , 0 );
	}

	pdev->RequestState = CMD_SEND;
	pdev->Control.state = CTRL_IDLE;

	t = systick_1ms_cnt;

	do
	{
		status = USBH_CtlReq(pcore, pdev, buff , length );
		if (status == USBH_OK)
		{
			dbgwdg_feed();
			break;
		}
		else if (status == USBH_FAIL || status == USBH_STALL || status == USBH_NOT_SUPPORTED) {
			dbgwdg_feed();
			break;
		}
		else
		{
			status = USBH_HandleControl(pcore, pdev);
			if (status == USBH_FAIL || status == USBH_STALL || status == USBH_NOT_SUPPORTED) {
				dbgwdg_feed();
				break;
			}
		}
	}
	while ((systick_1ms_cnt - t) < timeout);

	if (status != USBH_OK) {
		dbg_printf(DBGMODE_ERR, "%s USBH_CtlReq_Blocking Timeout \r\n", USBH_Dev_DebugPrint(pdev, 0));

		// close and reopen to clear the stall
		if (status == USBH_STALL)
		{
			USBH_Dev_FreeControl(pcore, pdev);
			if (alloc == 0) {
				USBH_Dev_AllocControl(pcore, pdev);
			}
		}
	}

	if (alloc != 0) {
		USBH_Dev_FreeControl(pcore, pdev);
	}

	dbgwdg_feed();
	return status;
}
