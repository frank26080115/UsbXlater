#include "usbh_dev_hub.h"
#include "usbh_dev_cb_default.h"
#include "usbh_dev_manager.h"
#include <usbh_lib/usbh_core.h>
#include <usbh_lib/usbh_hcs.h>
#include <usbh_lib/usbh_ioreq.h>
#include <usbh_lib/usbh_stdreq.h>
#include <stm32f2/stm32f2xx_rcc.h>
#include <stm32f2/stm32f2xx_gpio.h>
#include <stdlib.h>
#include <vcp.h>

USBH_Device_cb_TypeDef USBH_Dev_Hub_CB = {
	USBH_Dev_Hub_DeInitDev,
	USBH_Dev_Hub_Task,
	USBH_Dev_Hub_Init,
	USBH_Dev_Hub_DeInit,
	USBH_Dev_Hub_DeviceAttached,
	USBH_Dev_Hub_ResetDevice,
	USBH_Dev_Hub_DeviceDisconnected,
	USBH_Dev_DefaultCB_OverCurrentDetected,
	USBH_Dev_Hub_DeviceSpeedDetected,
	USBH_Dev_Hub_DeviceDescAvailable,
	USBH_Dev_Hub_DeviceAddressAssigned,
	USBH_Dev_Hub_ConfigurationDescAvailable,
	USBH_Dev_DefaultCB_ManufacturerString,
	USBH_Dev_DefaultCB_ProductString,
	USBH_Dev_DefaultCB_SerialNumString,
	USBH_Dev_Hub_EnumerationDone,
	USBH_Dev_DefaultCB_DeviceNotSupported,
	USBH_Dev_Hub_UnrecoveredError,
};

extern volatile uint8_t USBH_Dev_Reset_Timer;

void USB_Hub_Hardware_Init()
{
	// function is stubbed, new 20131021 circuit does not have an integrated hub
	// and thus there is no good way to perform a hardware reset
	return;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	GPIO_InitTypeDef gi;
	GPIO_StructInit(&gi);
	gi.GPIO_Mode = GPIO_Mode_OUT;
	gi.GPIO_Pin = GPIO_Pin_15;
	gi.GPIO_OType = GPIO_OType_PP;
	gi.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOA, &gi);
}

void USB_Hub_Hardware_Reset()
{
	// function is stubbed, new 20131021 circuit does not have an integrated hub
	// and thus there is no good way to perform a hardware reset
	return;

	GPIO_WriteBit(GPIOA, GPIO_Pin_15, Bit_RESET);
	delay_us(200);
	GPIO_WriteBit(GPIOA, GPIO_Pin_15, Bit_SET);
}

void USBH_Dev_Hub_Handle_InterruptIn(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, void* data_)
{
	uint8_t* data = data_;
	Hub_Data_t* Hub_Data = pdev->Usr_Data;
	USBH_Status errCode;

	// when a device connects or disconnects from the hub, the hub will issue an interrupt-in message

	// we have previously deallocated the channel for the control EP
	// we need to reallocate them
	if (pdev->Control.hc_num_in < 0)
	{
		pdev->Control.hc_num_in  = USBH_Alloc_Channel(pcore, 0x80);
		USBH_Open_Channel(	pcore,
							pdev->Control.hc_num_in,
							pdev->device_prop.address,
							pdev->device_prop.speed,
							EP_TYPE_CTRL,
							pdev->Control.ep0size);
	}
	if (pdev->Control.hc_num_out < 0)
	{
		pdev->Control.hc_num_out = USBH_Alloc_Channel(pcore, 0x00);
		USBH_Open_Channel(	pcore,
							pdev->Control.hc_num_out,
							pdev->device_prop.address,
							pdev->device_prop.speed,
							EP_TYPE_CTRL,
							pdev->Control.ep0size);
	}

	// iterate all the ports
	for (int pn = 0; pn < Hub_Data->num_ports; pn++)
	{
		int pnp1 = pn + 1;
		uint8_t bitIdx = pnp1 % 8;
		uint8_t byteIdx = pnp1 / 8;
		// did this port cause the event?
		if (data[byteIdx] & (1 << bitIdx))
		{
			if (Hub_Data->port_busy != 0 && Hub_Data->port_busy != pnp1) {
				// we can only handle one new device at a time, or else they all listen to address 0
				continue;
			}

			uint16_t wPortStatus, wPortChange;
			errCode = USBH_Dev_Hub_GetPortStatus(pcore, pdev, pn, &wPortStatus, &wPortChange);

			if (errCode != USBH_OK) {
				dbg_printf(DBGMODE_ERR, "USBH_Dev_Hub_Handle_InterruptIn GetPortStatus (pn %d) failed (status 0x%04X) \r\n", pn, errCode);
				USBH_ErrorHandle(pcore, pdev, errCode);
				continue;
			}

			dbg_printf(DBGMODE_DEBUG, "Hub_Handle_InterruptIn Hub_GetPortStatus, pn: %d, s: 0x%04X, c: 0x%04X\r\n", pnp1, wPortStatus, wPortChange);

			if ((wPortStatus & (1 << HUBWPORTSTATUS_POWER_BIT)) == 0) {
				errCode = USBH_Dev_Hub_SetPortFeature(pcore, pdev, pn, HUBREQ_PORT_POWER);
			}

			if ((wPortStatus & (1 << HUBWPORTCHANGE_ENABLED_BIT)) == 0) {
				errCode = USBH_Dev_Hub_SetPortFeature(pcore, pdev, pn, HUBREQ_PORT_ENABLE);
			}

			if ((wPortChange & (1 << HUBWPORTCHANGE_RESET_BIT)) != 0) {
				errCode = USBH_Dev_Hub_ClearPortFeature(pcore, pdev, pn, HUBREQ_C_PORT_RESET, 0);
			}

			if ((wPortChange & (1 << HUBWPORTCHANGE_CONNSTAT_BIT)) != 0) {
				errCode = USBH_Dev_Hub_ClearPortFeature(pcore, pdev, pn, HUBREQ_C_PORT_CONNECTION, 0);
			}

			if ((wPortChange & (1 << HUBWPORTCHANGE_ENABLED_BIT)) != 0) {
				errCode = USBH_Dev_Hub_ClearPortFeature(pcore, pdev, pn, HUBREQ_C_PORT_ENABLE, 0);
			}

			if ((wPortStatus & (1 << HUBWPORTSTATUS_RESET_BIT)) != 0)
			{
				if (Hub_Data->children[pn] != 0 && Hub_Data->children[pn]->gState == HOST_IDLE)
				{
					Hub_Data->children[pn]->gState = HOST_DEV_RESET_PENDING;
				}
			}
			else
			{
				if (((wPortStatus & (1 << HUBWPORTSTATUS_CURCONN_BIT)) == 0 ||
				     (wPortStatus & (1 << HUBWPORTSTATUS_ENABLED_BIT)) == 0) &&
				     Hub_Data->children[pn] != 0 &&
				     Hub_Data->children[pn]->gState != HOST_IDLE && Hub_Data->children[pn]->gState != HOST_DEV_RESET_PENDING)
				{
					dbg_printf(DBGMODE_TRACE, "Hub disconnected device (pn %d)\r\n", pn);

					((USBH_Device_cb_TypeDef*)Hub_Data->children[pn]->cb)->DeviceDisconnected(pcore, pdev);
					USBH_DeInit(pcore, Hub_Data->children[pn]); // frees channels
					((USBH_Device_cb_TypeDef*)Hub_Data->children[pn]->cb)->DeInit(pcore, Hub_Data->children[pn]);
					((USBH_Device_cb_TypeDef*)Hub_Data->children[pn]->cb)->DeInitDev(pcore, Hub_Data->children[pn]); // this is the one that frees memory
					free(Hub_Data->children[pn]);
					Hub_Data->children[pn] = 0;
				}
				else if ((wPortStatus & (1 << HUBWPORTSTATUS_CURCONN_BIT)) != 0)
				{
					if ((wPortStatus & (1 << HUBWPORTSTATUS_RESET_BIT)) == 0 &&
					    (wPortStatus & (1 << HUBWPORTSTATUS_ENABLED_BIT)) != 0 &&
						Hub_Data->children[pn] != 0 && (Hub_Data->children[pn]->gState == HOST_DEV_RESET_PENDING || Hub_Data->children[pn]->gState == HOST_IDLE))
					{
						((USBH_Device_cb_TypeDef*)Hub_Data->children[pn]->cb)->ResetDevice(pcore, pdev);
						Hub_Data->children[pn]->device_prop.speed = (wPortStatus & (1 << 9)) ? HPRT0_PRTSPD_LOW_SPEED : HPRT0_PRTSPD_FULL_SPEED; // if bit 9 is 1, then it is low speed (0x02), or else it is full speed (0x01)
						((USBH_Device_cb_TypeDef*)Hub_Data->children[pn]->cb)->DeviceSpeedDetected(pcore, Hub_Data->children[pn], Hub_Data->children[pn]->device_prop.speed);

						Hub_Data->children[pn]->gState = HOST_DEV_DELAY;
						USBH_Dev_Reset_Timer = HCD_GetCurrentFrame(pcore);
						Hub_Data->children[pn]->device_prop.address = 0; // new attached devices to a hub is always address 0
						Hub_Data->port_busy = pnp1;
						// we are allowed to pass this on to the upper state machine now, it will seem like it was attached normally
						dbg_printf(DBGMODE_TRACE, "Hub passed new device (pn %d) to upper state machine \r\n", pn);
						vcp_printf("Hub V%04XP%04XA%d New Device on Port %d\r\n", pdev->device_prop.Dev_Desc.idVendor, pdev->device_prop.Dev_Desc.idProduct, pdev->device_prop.address, pn);
					}
					else if (Hub_Data->children[pn] == 0)
					{
						errCode = USBH_Dev_Hub_SetPortFeature(pcore, pdev, pn, HUBREQ_PORT_RESET);
						Hub_Data->children[pn] = calloc(1, sizeof(USBH_DEV));
						Hub_Data->children[pn]->Parent = pdev;
						Hub_Data->children[pn]->gState = HOST_IDLE;
						Hub_Data->children[pn]->gStateBkp = HOST_IDLE;
						Hub_Data->children[pn]->EnumState = ENUM_IDLE;
						Hub_Data->children[pn]->RequestState = CMD_SEND;
						Hub_Data->children[pn]->device_prop.address = USBH_DEVICE_ADDRESS_DEFAULT; // this better be 0
						Hub_Data->children[pn]->device_prop.speed = HPRT0_PRTSPD_LOW_SPEED;
						Hub_Data->children[pn]->port_num = pn;
						Hub_Data->children[pn]->Control.state = CTRL_SETUP;
						Hub_Data->children[pn]->Control.ep0size = USB_OTG_MAX_EP0_SIZE;
						Hub_Data->children[pn]->cb = (void*)&USBH_Dev_CB_Default;
						Hub_Data->port_busy = pnp1;
						dbg_printf(DBGMODE_TRACE, "Hub created new child (pn %d) \r\n", pn);
					}
				}
			}
		}
	}

	// we don't need the control EP for a while
	// free them so other devices can use them
	if (pdev->Control.hc_num_in >= 0)
	{
		USB_OTG_HC_Halt(pcore, pdev->Control.hc_num_in);
		USBH_Free_Channel(pcore, pdev->Control.hc_num_in);
		pdev->Control.hc_num_in = -1;
	}
	if (pdev->Control.hc_num_out >= 0)
	{
		USB_OTG_HC_Halt(pcore, pdev->Control.hc_num_out);
		USBH_Free_Channel(pcore, pdev->Control.hc_num_out);
		pdev->Control.hc_num_out = -1;
	}
}

void USBH_Dev_Hub_DeInitDev(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	// note: this function is a possible source of hardfaults because it's freeing memory

	Hub_Data_t* Hub_Data = pdev->Usr_Data;
	// iterate all the ports
	for (int pn = 0; pn < Hub_Data->num_ports; pn++)
	{
		if (Hub_Data->children[pn] != 0)
		{
			// disconnect event for all children
			((USBH_Device_cb_TypeDef*)Hub_Data->children[pn]->cb)->DeInitDev(pcore, Hub_Data->children[pn]);

			// free memory allocated for the child
			free(Hub_Data->children[pn]);
			Hub_Data->children[pn] = 0;
		}
	}

	// free the array
	if (Hub_Data->children != 0) free(Hub_Data->children);
	Hub_Data->children = 0;

	// these HCs are for the interrupt-in EPs
	if (Hub_Data->hc_num_in >= 0)
	{
		USB_OTG_HC_Halt(pcore, Hub_Data->hc_num_in);
		USBH_Free_Channel(pcore, Hub_Data->hc_num_in);
		Hub_Data->hc_num_in = -1;
	}

	// this one shouldn't be allocated at all since there shouldn't be an interrupt-out EP
	// but do it just in case (no performance hit here yet)
	if(Hub_Data->hc_num_out >= 0)
	{
		USB_OTG_HC_Halt(pcore, Hub_Data->hc_num_out);
		USBH_Free_Channel(pcore, Hub_Data->hc_num_out);
		Hub_Data->hc_num_out = -1;
	}

	Hub_Data->start_toggle = 0;
}

USBH_Status USBH_Dev_Hub_Task(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
  Hub_Data_t* Hub_Data = pdev->Usr_Data;

  USBH_Status status = USBH_OK;

  char toDeallocate = 0;

  switch (Hub_Data->state)
  {

  case HubState_IDLE:

    Hub_Data->state = HubState_SYNC;

  case HubState_SYNC:

    /* Sync with start of Even Frame */
    if(USB_OTG_IsEvenFrame(pcore) != FALSE)
    {
      Hub_Data->state = HubState_GET_DATA;

      // deallocate channel here so we have more channels available
      if (Hub_Data->has_control != 0)
      {
        // I was going to deallocate the channel here, but it's already done inside
        // USBH_Dev_Hub_Handle_InterruptIn

        Hub_Data->has_control = 0;
      }
    }
    else
    {
      break;
    }

  case HubState_GET_DATA:

	#ifdef HUB_ENABLE_DYNAMIC_HC_ALLOC
	if (Hub_Data->hc_num_in < 0) {
		Hub_Data->hc_num_in = USBH_Alloc_Channel(pcore, Hub_Data->intInEp);
		if (Hub_Data->hc_num_in >= 0) {
			USBH_Open_Channel  (pcore,
								Hub_Data->hc_num_in,
								pdev->device_prop.address,
								pdev->device_prop.speed,
								EP_TYPE_INTR,
								Hub_Data->length);
		}
	}

	if (Hub_Data->hc_num_in >= 0) {
	#endif
	if(USB_OTG_IsEvenFrame(pcore) != FALSE) {
		USBH_InterruptReceiveData(pcore,
								  Hub_Data->buff,
								  Hub_Data->length,
								  Hub_Data->hc_num_in);
		Hub_Data->start_toggle = 1;

		Hub_Data->state = HubState_POLL;
		Hub_Data->timer = HCD_GetCurrentFrame(pcore);
	}
	#ifdef HUB_ENABLE_DYNAMIC_HC_ALLOC
    }
    else {
      dbg_printf(DBGMODE_ERR, "Unable to allocate channel for hub (addr %d) \r\n", pdev->device_prop.address);
      Hub_Data->hc_num_in = 0;
    }
    #endif
    break;

  case HubState_POLL:
    if(( HCD_GetCurrentFrame(pcore) - Hub_Data->timer) >= Hub_Data->poll)
    {
      Hub_Data->state = HubState_GET_DATA;
      toDeallocate = 1;
    }
    else if(HCD_GetURB_State(pcore , Hub_Data->hc_num_in) == URB_DONE)
    {
      if(Hub_Data->start_toggle != 0) /* handle data once */
      {
        Hub_Data->start_toggle = 0;
        USBH_Dev_Hub_Handle_InterruptIn(pcore, pdev, Hub_Data->buff);
        toDeallocate = 1;
      }
    }
    else if(HCD_GetURB_State(pcore, Hub_Data->hc_num_in) == URB_NOTREADY) // NAK
    {
      toDeallocate = 1;
    }
    else if(HCD_GetURB_State(pcore, Hub_Data->hc_num_in) == URB_STALL) // Stalled
    {
      Hub_Data->state = HubState_GET_DATA;
      toDeallocate = 1;
    }

    #ifdef HUB_ENABLE_DYNAMIC_HC_ALLOC
    if (toDeallocate != 0) {
      if (Hub_Data->hc_num_in >= 0) {
        USB_OTG_HC_Halt(pcore, Hub_Data->hc_num_in);
        USBH_Free_Channel(pcore, Hub_Data->hc_num_in);
        Hub_Data->hc_num_in = -1;
      }
    }
    #endif
    break;

  default:
    dbg_printf(DBGMODE_ERR, "Hub_Task unknown Hub_Data->state 0x%02X \r\n", Hub_Data->state);
    break;
  }

	// iterate all the ports
	for (int pn = 0; pn < Hub_Data->num_ports; pn++)
	{
		if (Hub_Data->children[pn] != 0) {
			USBH_Process(pcore, Hub_Data->children[pn]);
		}
	}
	return status;
}

void USBH_Dev_Hub_Init(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	dbg_printf(DBGMODE_TRACE, "USBH_Dev_Hub_Init \r\n");
}

void USBH_Dev_Hub_DeInit(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	dbg_printf(DBGMODE_TRACE, "USBH_Dev_Hub_DeInit \r\n");
}

void USBH_Dev_Hub_DeviceAttached(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	dbg_printf(DBGMODE_TRACE, "USBH_Dev_Hub_DeviceAttached \r\n");
}

void USBH_Dev_Hub_ResetDevice(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	dbg_printf(DBGMODE_TRACE, "USBH_Dev_Hub_ResetDevice \r\n");
}

void USBH_Dev_Hub_DeviceDisconnected(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	dbg_printf(DBGMODE_TRACE, "USBH_Dev_Hub_DeviceDisconnected \r\n");

	Hub_Data_t* Hub_Data = pdev->Usr_Data;
	// iterate all the ports
	for (int pn = 0; pn < Hub_Data->num_ports; pn++)
	{
		if (Hub_Data->children[pn] != 0) {
			// disconnect event for all children
			((USBH_Device_cb_TypeDef*)Hub_Data->children[pn]->cb)->DeviceDisconnected(pcore, pdev);
			USBH_DeInit(pcore, Hub_Data->children[pn]); // frees channels
			((USBH_Device_cb_TypeDef*)Hub_Data->children[pn]->cb)->DeInit(pcore, Hub_Data->children[pn]);
			((USBH_Device_cb_TypeDef*)Hub_Data->children[pn]->cb)->DeInitDev(pcore, Hub_Data->children[pn]); // this is the one that frees memory
			free(Hub_Data->children[pn]);
			Hub_Data->children[pn] = 0;
		}
	}
}

void USBH_Dev_Hub_OverCurrentDetected(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	dbg_printf(DBGMODE_ERR, "USBH_Dev_Hub_OverCurrentDetected \r\n");
}

void USBH_Dev_Hub_DeviceSpeedDetected(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, uint8_t DeviceSpeed)
{
	dbg_printf(DBGMODE_DEBUG, "USBH_Dev_Hub_DeviceSpeedDetected: %d\r\n", DeviceSpeed);
}

void USBH_Dev_Hub_DeviceDescAvailable(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, void* data_)
{
	dbg_printf(DBGMODE_TRACE, "Hub_DeviceDescAvailable (should not be called) \r\n");
	// should not be callled
}

void USBH_Dev_Hub_DeviceAddressAssigned(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	dbg_printf(DBGMODE_TRACE, "Hub_DeviceAddressAssigned \r\n");
}

void USBH_Dev_Hub_ConfigurationDescAvailable(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev, USBH_CfgDesc_TypeDef * cfg, USBH_InterfaceDesc_TypeDef * intf, USBH_EpDesc_TypeDef ** ep)
{
	dbg_printf(DBGMODE_TRACE, "Hub_ConfigurationDescAvailable \r\n");
}

void USBH_Dev_Hub_EnumerationDone(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	dbg_printf(DBGMODE_TRACE, "Hub_EnumerationDone \r\n");

	if (pdev->Usr_Data != 0) {
		free(pdev->Usr_Data);
		pdev->Usr_Data = 0;
	}
	pdev->Usr_Data = calloc(1, sizeof(Hub_Data_t));
	Hub_Data_t* Hub_Data = pdev->Usr_Data;

	Hub_Data->state     = HubState_IDLE;
	//Hub_Data->ctl_state = HubState_REQ_IDLE;

	uint8_t maxEP = ( (pdev->device_prop.Itf_Desc[0].bNumEndpoints <= USBH_MAX_NUM_ENDPOINTS) ?
					pdev->device_prop.Itf_Desc[0].bNumEndpoints :
					USBH_MAX_NUM_ENDPOINTS);

	for (uint8_t num=0; num < maxEP; num++)
	{
		USBH_EpDesc_TypeDef* epDesc = &(pdev->device_prop.Ep_Desc[0][num]);

		if ((epDesc->bmAttributes & USB_EP_TYPE_INTR) == USB_EP_TYPE_INTR && (epDesc->bEndpointAddress & USB_EP_DIR_MSK) == USB_EP_DIR_IN)
		{
			Hub_Data->ep_addr   = epDesc->bEndpointAddress;
			Hub_Data->length    = epDesc->wMaxPacketSize;
			Hub_Data->poll      = epDesc->bInterval;
			if (Hub_Data->poll < HUB_MIN_POLL) {
				Hub_Data->poll = HUB_MIN_POLL;
			}

			Hub_Data->intInEp = (epDesc->bEndpointAddress);
			Hub_Data->hc_num_in = USBH_Alloc_Channel(pcore, Hub_Data->intInEp);

			USBH_Open_Channel  (pcore,
								Hub_Data->hc_num_in,
								pdev->device_prop.address,
								pdev->device_prop.speed,
								EP_TYPE_INTR,
								Hub_Data->length);
		}
	}

	// the HID code used a state machine to perform the sequence of requests, but here we will just do everything in a sequence

	// get the hub descriptor so we know the number of ports available
	USBH_Status status = USBH_GetDescriptor_Blocking(pcore,
								pdev,
								USB_REQ_RECIPIENT_DEVICE
								| USB_REQ_TYPE_CLASS,
								USB_DESC_HUB,
								0,
								pcore->host.Rx_Buffer,
								USB_HUB_DESC_SIZE,
								200);

	if (status != USBH_OK) {
		dbg_printf(DBGMODE_ERR, "Hub_EnumerationDone GetDescriptor failed, status 0x%02X \r\n", status);
		USBH_ErrorHandle(pcore, pdev, status);
		return;
	}

	Hub_Data->num_ports = pcore->host.Rx_Buffer[2];

	dbg_printf(DBGMODE_DEBUG, "\rbDescriptorType = 0x%02X\r\n", pcore->host.Rx_Buffer[1]);
	dbg_printf(DBGMODE_DEBUG, "\rnum_ports = %d\r\n", Hub_Data->num_ports);
	vcp_printf("Hub V%04XP%04XA%d Enumerated, %d ports\r\n", pdev->device_prop.Dev_Desc.idVendor, pdev->device_prop.Dev_Desc.idProduct, pdev->device_prop.address, Hub_Data->num_ports);

	// allocate memory for the children devices
	if (Hub_Data->children != 0) {
		free(Hub_Data->children);
		Hub_Data->children = 0;
	}
	Hub_Data->children = calloc(Hub_Data->num_ports, sizeof(USBH_DEV*));

	// iterate all the ports
	for (int pn = 0; pn < Hub_Data->num_ports; pn++)
	{
		Hub_Data->children[pn] = 0; // reset

		uint16_t wps, wpc;

		status = USBH_Dev_Hub_GetPortStatus(pcore, pdev, pn, &wps, &wpc);
		if (status != USBH_OK) {
			dbg_printf(DBGMODE_ERR, "Hub_EnumerationDone GetPortStatus (pn %d) failed, status 0x%02X \r\n", pn, status);
			USBH_ErrorHandle(pcore, pdev, status);
			continue;
		}

		status = USBH_Dev_Hub_SetPortFeature(pcore, pdev, pn, HUBREQ_PORT_POWER);
		if (status != USBH_OK) {
			dbg_printf(DBGMODE_ERR, "Hub_EnumerationDone SetPortFeature (pn %d) failed, status 0x%02X \r\n", pn, status);
			USBH_ErrorHandle(pcore, pdev, status);
			continue;
		}
	}

	// wait for power ready
	char pwrRdy;
	volatile int pwrTries = 700 / Hub_Data->num_ports;
	do
	{
		pwrRdy = 1;
		for (int pn = 0; pn < Hub_Data->num_ports; pn++)
		{
			uint16_t wps, wpc;

			status = USBH_Dev_Hub_GetPortStatus(pcore, pdev, pn, &wps, &wpc);
			if (status != USBH_OK) {
				dbg_printf(DBGMODE_ERR, "Hub_EnumerationDone GetPortStatus (pn %d) failed, status 0x%02X \r\n", pn, status);
				USBH_ErrorHandle(pcore, pdev, status);
				continue;
			}

			if (wps & (1 << HUBWPORTSTATUS_POWER_BIT) == 0) {
				pwrRdy = 0;
			}
		}
	}
	while (pwrRdy == 0 && pwrTries--);

	if (pwrRdy != 0) {
		dbg_printf(DBGMODE_TRACE, "Hub_EnumerationDone all port power ready \r\n");
	}
	else {
		dbg_printf(DBGMODE_ERR, "Hub_EnumerationDone error, power not ready \r\n");
	}

	vcp_printf("V%04XP%04XA%d:Hub Ready, Ports: %d \r\n", pdev->device_prop.Dev_Desc.idVendor, pdev->device_prop.Dev_Desc.idProduct, pdev->device_prop.address, Hub_Data->num_ports);
}

void USBH_Dev_Hub_DeviceNotSupported(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	dbg_printf(DBGMODE_ERR, "USBH_Dev_Hub_DeviceNotSupported \r\n");
}

void USBH_Dev_Hub_UnrecoveredError(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	dbg_printf(DBGMODE_ERR, "USBH_Dev_Hub_UnrecoveredError \r\n");
}

USBH_Status USBH_Dev_Hub_SetPortFeature(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t port, uint16_t feature)
{
	pdev->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_RECIPIENT_OTHER | USB_REQ_TYPE_CLASS;
	pdev->Control.setup.b.bRequest = 0x03;
	pdev->Control.setup.b.wValue.w = feature;
	pdev->Control.setup.b.wIndex.w = port + 1;
	pdev->Control.setup.b.wLength.w = 0;

	return USBH_CtlReq_Blocking(pcore, pdev, 0 , 0 , 500);
}

USBH_Status USBH_Dev_Hub_GetPortStatus(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t port, uint16_t* wPortStatus, uint16_t* wPortChange)
{
	pdev->Control.setup.b.bmRequestType = USB_D2H | USB_REQ_RECIPIENT_OTHER | USB_REQ_TYPE_CLASS;
	pdev->Control.setup.b.bRequest = 0x00; // 0x00 is GET_STATUS
	pdev->Control.setup.b.wValue.w = 0;
	pdev->Control.setup.b.wIndex.w = port + 1;
	pdev->Control.setup.b.wLength.w = 4;

	USBH_Status status = USBH_CtlReq_Blocking(pcore, pdev, pcore->host.Rx_Buffer , 4 , 500);

	*wPortStatus = *((uint16_t*)(&pcore->host.Rx_Buffer[0]));
	*wPortChange = *((uint16_t*)(&pcore->host.Rx_Buffer[2]));

	return status;
}

USBH_Status USBH_Dev_Hub_ClearPortFeature(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, uint8_t port, uint16_t feature, uint8_t selector)
{
	pdev->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_RECIPIENT_OTHER | USB_REQ_TYPE_CLASS;
	pdev->Control.setup.b.bRequest = 0x01;
	pdev->Control.setup.b.wValue.w = feature;
	pdev->Control.setup.b.wIndex.bw.lsb = selector;
	pdev->Control.setup.b.wIndex.bw.msb = port + 1;
	pdev->Control.setup.b.wLength.w = 0;

	return USBH_CtlReq_Blocking(pcore, pdev, 0 , 0 , 500);
}
