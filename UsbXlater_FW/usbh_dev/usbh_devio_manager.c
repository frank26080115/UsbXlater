#include "usbh_devio_manager.h"
#include "usbh_dev_manager.h"
#include <core_cmFunc.h>
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
int8_t USBH_DevIO_OpenChannel(USBH_DevIO_t* ci);
void USBH_DevIO_FreeChannel(USBH_DevIO_t* ci);

USBH_DevIO_t* USBH_DevIO_HeadNode;
USBH_DevIO_t* USBH_DevIO_CurrentNode;

#ifdef ENABLE_USBDEVIO_STATISTICS
USBH_DevIO_Statistics_t USBH_DevIO_Statistics;
#endif

void USBH_DevIOManager_Init()
{
	USBH_DevIO_HeadNode = 0;
	USBH_DevIO_CurrentNode = 0;
	memset(&USBH_DevIO_Statistics, 0, sizeof(USBH_DevIO_Statistics_t));
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
	USBH_DevIO_t* i = (USBH_DevIO_t*)calloc(1, sizeof(USBH_DevIO_t));

	if (i == 0) {
		dbg_printf(DBGMODE_TRACE, " error no memory\r\n");
		return 0;
	}

	i->enabled = 1;
	i->next_node = 0;
	i->hc = -1;
	i->hc_toggle_in = -1;
	i->hc_toggle_out = -1;
	i->pcore = pcore;
	i->pdev = pdev;
	i->ep = ep;
	i->timeout = i->ep->bInterval < 10 ? 10 : (i->ep->bInterval * 1);
	i->data_handler = data_handler;
	i->start_toggle = 0;
	i->state = UIOSTATE_IDLE;
	i->buff = (uint8_t*)malloc(i->ep->wMaxPacketSize);
	i->extra = extra;
	i->poll_interval = i->ep->bInterval;

	if ((i->ep->bEndpointAddress & USB_EP_DIR_MSK) == USB_D2H) {
		i->out_ptr = (uint8_t*)malloc(i->ep->wMaxPacketSize);
	}

	if (i->data_handler == 0) {
		i->data_handler = USBH_DevIO_DummyHandler;
	}

	i->hc_as_needed = hc_as_needed;

	if (i->hc_as_needed == 0)
	{
		USBH_DevIO_OpenChannel(i);
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
#ifdef ENABLE_USBDEVIO_STATISTICS
	USBH_DevIO_Statistics.dev_cnt++;
#endif
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

	if ((x->ep->bEndpointAddress & USB_EP_DIR_MSK) == USB_D2H && x->out_ptr != 0) {
		free(x->out_ptr);
	}

	USBH_DevIO_t* i;
	for (i = USBH_DevIO_HeadNode; i != 0; i = i->next_node)
	{
		if (i->next_node == x) {
			i->next_node = x->next_node;
			#ifdef ENABLE_USBDEVIO_STATISTICS
			USBH_DevIO_Statistics.dev_cnt--;
			#endif
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

char USBH_DevIO_Task(USBH_DevIO_t* p_io)
{
#ifdef FORCE_DEVIO_ORDERED_TASK
	p_io = 0;
#endif

	USBH_DevIO_t* ci;
	if (p_io == 0)
	{
		if (USBH_DevIO_CurrentNode == 0) {
			USBH_DevIO_CurrentNode = USBH_DevIO_HeadNode;
		}
		if (USBH_DevIO_CurrentNode == 0) {
			return 2;
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
	HC_STATUS hcsts;

#ifdef ENABLE_DEVIO_INTR_WAITEVENODDFRAME
	if (ci->state == UIOSTATE_SEND_REQUEST && epDir == USB_D2H && (epType == EP_TYPE_INTR || epType == EP_TYPE_ISOC))
	{
		if (USB_OTG_IsEvenFrame(ci->pcore) == 0)
		{
#ifdef ENABLE_EVENODDFRAME_WAIT
			if (ci->evenodd_frm_cnt > 1000) {
				// too many failures? it's unlikely, but if it happens, we force a sync by blocking
				while (USB_OTG_IsEvenFrame(ci->pcore) == 0);
			}
			else
#endif
			{
				ci->evenodd_frm_cnt++;
				return 0;
			}
		}
		else
		{
			ci->evenodd_frm_cnt = 0;
		}
	}
	else if (ci->state == UIOSTATE_SEND_REQUEST && epDir == USB_H2D && (epType == EP_TYPE_INTR || epType == EP_TYPE_ISOC))
	{
		if (USB_OTG_IsEvenFrame(ci->pcore) != 0)
		{
#ifdef ENABLE_EVENODDFRAME_WAIT
			if (ci->evenodd_frm_cnt > 1000) {
				// too many failures? it's unlikely, but if it happens, we force a sync by blocking
				while (USB_OTG_IsEvenFrame(ci->pcore) != 0);
			}
			else
#endif
			{
				ci->evenodd_frm_cnt++;
				return 0;
			}
		}
		else
		{
			ci->evenodd_frm_cnt = 0;
		}
	}
#endif

	switch (ci->state)
	{
		case UIOSTATE_IDLE:
			ci->start_toggle = 0;
			ci->state = UIOSTATE_START;
			break;
		case UIOSTATE_START:
			if (epDir == USB_D2H)
			{
				if (ci->enabled)
				{
					if ((ci->force_poll_interval == 0 || (ci->force_poll_interval != 0 && (systick_1ms_cnt - ci->timestamp) > ci->ep->bInterval)) && ci->priority_countdown <= 0)
					{
						ci->state = UIOSTATE_SEND_REQUEST;
						ci->priority_countdown = ci->priority;
					}
					else
					{
						ci->state = UIOSTATE_NEXT;
						break;
					}
				}
				else
				{
					ci->state = UIOSTATE_FINALIZE;
					break;
				}
			}
			else
			{
				ci->retries = 5;
				if (ci->remaining > 0) {
					ci->state = UIOSTATE_SEND_REQUEST;
				}
				else {
					ci->state = UIOSTATE_FINALIZE;
				}
				break;
			}
			// fall through
		case UIOSTATE_SEND_REQUEST:
			if (USBH_DevIO_OpenChannel(ci) < 0) {
				ci->state = UIOSTATE_FINALIZE;
				break;
			}
			if (ci->hc_as_needed != 0) {
				ci->hc_as_needed = 1;
			}
			ci->timestamp = systick_1ms_cnt;
			if (epDir == USB_D2H)
			{
				if (epType == EP_TYPE_BULK) {
					USBH_BulkReceiveData(ci->pcore, ci->buff, epSize, ci->hc);
				}
				else if (epType == EP_TYPE_INTR) {
					#ifdef ENABLE_USBDEVIO_STATISTICS
					USBH_DevIO_Statistics.intr_req_cnt++;
					#endif
					USBH_InterruptReceiveData(ci->pcore, ci->buff, epSize, ci->hc);
				}
				else if (epType == EP_TYPE_ISOC) {
					USBH_IsocReceiveData(ci->pcore, ci->buff, epSize, ci->hc);
				}
				else {
					dbg_printf(DBGMODE_ERR, "USBH_DevIO_Task UIOSTATE_SEND_REQUEST %s error unknown EP type 0x%02X\r\n", USBH_Dev_DebugPrint(ci->pdev, ci->ep), ci->ep->bmAttributes);
					ci->state = UIOSTATE_FINALIZE;
					break;
				}
				ci->start_toggle = 1;
			}
			else
			{
				sendSize = epSize > ci->remaining ? ci->remaining : epSize;
				memcpy(ci->buff, ci->out_ptr, sendSize);
				if (epType == EP_TYPE_BULK) {
					USBH_BulkSendData(ci->pcore, ci->buff, sendSize, ci->hc);
				}
				else if (epType == EP_TYPE_INTR) {
					USBH_InterruptSendData(ci->pcore, ci->buff, sendSize, ci->hc);
				}
				else if (epType == EP_TYPE_ISOC) {
					USBH_IsocSendData(ci->pcore, ci->buff, sendSize, ci->hc);
				}
				else {
					dbg_printf(DBGMODE_ERR, "USBH_DevIO_Task UIOSTATE_SEND_DATA %s error unknown EP type 0x%02X\r\n", USBH_Dev_DebugPrint(ci->pdev, ci->ep), ci->ep->bmAttributes);
					ci->state = UIOSTATE_FINALIZE;
					break;
				}
			}
			#ifdef ENABLE_USBDEVIO_STATISTICS
			USBH_DevIO_Statistics.req_cnt++;
			ci->req_cnt++;
			#endif
			ci->state = UIOSTATE_WAIT_DATA;
		case UIOSTATE_WAIT_DATA:
			urb = HCD_GetURB_State(ci->pcore, ci->hc);
			if (urb == URB_ERROR)
			{
				hcsts = HCD_GetHCState(ci->pcore, ci->hc);
				ci->hc_has_error = 1;
				// handle the case when a transaction has data but returns as error
				if (ci->pcore->host.XferCnt[ci->hc] > 0 && ci->start_toggle != 0 && epDir == USB_D2H) {
					urb = URB_DONE;
					#ifdef ENABLE_USBDEVIO_STATISTICS
					ci->err_cnt++;
					USBH_DevIO_Statistics.err_cnt++;
					#endif
				}
			}
			if (urb != URB_IDLE) {
				#ifdef ENABLE_USBDEVIO_STATISTICS
				USBH_DevIO_Statistics.notidle_cnt++;
				ci->notidle_cnt++;
				#endif
			}
			if (urb == URB_DONE)
			{
				ci->priority_countdown = 0;
				if (ci->start_toggle != 0 && epDir == USB_D2H)
				{
					if (ci->data_handler != 0)
					{
						__disable_irq();
						int len = ci->pcore->host.XferCnt[ci->hc]; // read this here, closing the HC will clear this data
						memcpy(ci->out_ptr, ci->buff, len);
						__enable_irq();
						if (ci->hc >= 0 && ci->hc_as_needed != 0) {
							USBH_DevIO_FreeChannel(ci);
						}
						ci->data_handler(ci, ci->out_ptr, len);
					}
					#ifdef ENABLE_USBDEVIO_STATISTICS
					ci->done_cnt++;
					USBH_DevIO_Statistics.done_cnt++;
					#endif
					ci->start_toggle = 0;
					ci->state = UIOSTATE_FINALIZE;
					// fall through
				}
				else if (epDir == USB_H2D)
				{
					#ifdef ENABLE_USBDEVIO_STATISTICS
					ci->done_cnt++;
					USBH_DevIO_Statistics.done_cnt++;
					#endif
					sendSize = epSize > ci->remaining ? ci->remaining : epSize;
					ci->remaining -= sendSize;
					if (ci->remaining <= 0) {
						ci->state = UIOSTATE_FINALIZE;
						ci->remaining = 0;
						// fall through
					}
					else {
						ci->out_ptr = &(ci->out_ptr[sendSize]);
						ci->state = UIOSTATE_SEND_REQUEST;
						break;
					}
				}
			}
			else if (urb == URB_STALL)
			{
				#ifdef ENABLE_USBDEVIO_STATISTICS
				ci->stall_cnt++;
				USBH_DevIO_Statistics.stall_cnt++;
				#endif

				dbg_printf(DBGMODE_ERR, "UIOSTATE_WAIT_DATA URB_STALL %s\r\n", USBH_Dev_DebugPrint(ci->pdev, ci->ep));
				if (epDir == USB_H2D && (ci->retries--) > 0) {
					ci->state = UIOSTATE_SEND_REQUEST;
					break;
				}
				ci->state = UIOSTATE_FINALIZE;
			}
			else if (urb == URB_NOTREADY)
			{ // NAK, no data available
				#ifdef ENABLE_USBDEVIO_STATISTICS
				ci->nak_cnt++;
				USBH_DevIO_Statistics.nak_cnt++;
				#endif

				if (epDir == USB_H2D && (ci->retries--) > 0) {
					ci->state = UIOSTATE_SEND_REQUEST;
					break;
				}
				ci->state = UIOSTATE_FINALIZE;
			}
			else if (urb == URB_ERROR)
			{
				#ifdef ENABLE_USBDEVIO_STATISTICS
				ci->err_cnt++;
				USBH_DevIO_Statistics.err_cnt++;
				#endif

				ci->hc_has_error = 1;
				dbg_printf(DBGMODE_ERR, "UIOSTATE_WAIT_DATA URB_ERROR %s, HC_STATE %d\r\n", USBH_Dev_DebugPrint(ci->pdev, ci->ep), hcsts);
				if (hcsts == HC_DATATGLERR) {
					ci->hc_has_error = 2;
				}

				if (epDir == USB_H2D && (ci->retries--) > 0) {
					ci->state = UIOSTATE_SEND_REQUEST;
					break;
				}
				ci->state = UIOSTATE_FINALIZE;
			}
			else if (urb == URB_IDLE)
			{
				if ((systick_1ms_cnt - ci->timestamp) > ci->timeout)
				{
					ci->timeout_cnt++;
					#ifdef ENABLE_USBDEVIO_STATISTICS
					USBH_DevIO_Statistics.timeout_cnt++;
					#endif

					ci->state = UIOSTATE_FINALIZE;

					if (epType == EP_TYPE_INTR) {
						ci->hc_has_error = 1;
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
				USBH_DevIO_FreeChannel(ci);
			}
			break;
		case UIOSTATE_NEXT:
			ci->state = UIOSTATE_START;
			if (ci->priority_countdown > 0) ci->priority_countdown--;
			if (p_io == 0) {
				USBH_DevIO_CurrentNode = (ci->next_node != 0) ? ci->next_node : USBH_DevIO_HeadNode;
				return 1;
			}
			break;
		default:
			ci->state = UIOSTATE_START;
			break;
	}
	return 0;
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

	while ((systick_1ms_cnt - t) < timeout && USBH_DevIO_Task(0) != 0);

	t = systick_1ms_cnt;

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
		if ((systick_1ms_cnt - t) >= timeout) {
			dbg_printf(DBGMODE_ERR, "%s USBH_CtlReq_Blocking Timeout \r\n", USBH_Dev_DebugPrint(pdev, 0));
		}

		// close and reopen to clear the error, not sure if good
		USBH_Dev_FreeControl(pcore, pdev);
		if (alloc == 0) {
			USBH_Dev_AllocControl(pcore, pdev);
		}
	}

	if (alloc != 0) {
		USBH_Dev_FreeControl(pcore, pdev);
	}

	dbgwdg_feed();
	return status;
}

int8_t USBH_DevIO_OpenChannel(USBH_DevIO_t* ci)
{
	int8_t ret = 0;
	if (ci->hc < 0)
	{
		if (USBH_Open_Channel(
			ci->pcore,
			&(ci->hc),
			ci->ep->bEndpointAddress,
			ci->pdev->device_prop.address,
			ci->pdev->device_prop.speed,
			ci->ep->bmAttributes & EP_TYPE_MSK,
			ci->ep->wMaxPacketSize
		) != HC_OK) {
			dbg_printf(DBGMODE_ERR, "Failed to open HC for %s\r\n", USBH_Dev_DebugPrint(ci->pdev, ci->ep));
			ret = -1;
		}
		else {
			ci->hc_has_error = 0;
			// restore the toggle states so PID's DATA0 and DATA1 are correct for packet sync
			if (ci->hc_toggle_in >= 0) ci->pcore->host.hc[ci->hc].toggle_in = ci->hc_toggle_in;
			if (ci->hc_toggle_out >= 0) ci->pcore->host.hc[ci->hc].toggle_out = ci->hc_toggle_out;
			ret = 1;
			#ifdef ENABLE_HC_DEBUG
			dbg_printf(DBGMODE_DEBUG, "Opened HC for %s:HC%d\r\n", USBH_Dev_DebugPrint(ci->pdev, ci->ep), ci->hc);
			#endif
		}
	}
	if (ci->hc >= 0 && ci->hc_as_needed != 0) {
		ci->hc_as_needed = 1;
	}
	return ret;
}

void USBH_DevIO_FreeChannel(USBH_DevIO_t* ci)
{
	if (ci->hc >= 0)
	{
		if (ci->hc_as_needed != 0) {
			ci->hc_as_needed = 1;
		}
		// remember the toggle states so PID's DATA0 and DATA1 are correct for packet sync
		ci->hc_toggle_in = ci->pcore->host.hc[ci->hc].toggle_in;
		ci->hc_toggle_out = ci->pcore->host.hc[ci->hc].toggle_out;
		// toggle error will set this to 2, so toggle again to correct the error
		if (ci->hc_has_error == 2) {
			ci->hc_toggle_in ^= 1;
			ci->hc_toggle_out ^= 1;
		}
		#ifdef ENABLE_HC_DEBUG
		dbg_printf(DBGMODE_ERR, "Closing HC%d for %s\r\n", ci->hc, USBH_Dev_DebugPrint(ci->pdev, ci->ep));
		#endif
		if (ci->hc_has_error != 0) {
			USB_OTG_HC_Halt(ci->pcore, ci->hc);
		}
	}
	USBH_Free_Channel_Without_Halt(ci->pcore, &(ci->hc));
	ci->hc_has_error = 0;
}

void USBH_DevIO_Reset_HC(USB_OTG_CORE_HANDLE *pcore, int8_t hc)
{
	if (hc < 0) return;
	USB_OTG_HC_Halt(pcore, hc);
	USB_OTG_HC_Init(pcore, hc);
}
