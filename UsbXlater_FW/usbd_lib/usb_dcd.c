/**
  ******************************************************************************
  * @file    usb_dcd.c
  * @author  MCD Application Team
  * @version V2.1.0
  * @date    19-March-2012
  * @brief   Peripheral Device Interface Layer
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2012 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "usb_dcd.h"
#include <usbotg_lib/usb_bsp.h>


/** @addtogroup USB_OTG_DRIVER
* @{
*/

/** @defgroup USB_DCD
* @brief This file is the interface between EFSL ans Host mass-storage class
* @{
*/


/** @defgroup USB_DCD_Private_Defines
* @{
*/
/**
* @}
*/


/** @defgroup USB_DCD_Private_TypesDefinitions
* @{
*/
/**
* @}
*/



/** @defgroup USB_DCD_Private_Macros
* @{
*/
/**
* @}
*/


/** @defgroup USB_DCD_Private_Variables
* @{
*/
/**
* @}
*/


/** @defgroup USB_DCD_Private_FunctionPrototypes
* @{
*/

/**
* @}
*/


/** @defgroup USB_DCD_Private_Functions
* @{
*/



void DCD_Init(USB_OTG_CORE_HANDLE *pcore ,
              USB_OTG_CORE_ID_TypeDef coreID)
{
  uint32_t i;
  USB_OTG_EP *ep;

  USB_OTG_SelectCore (pcore , coreID);

  pcore->dev.device_status = USB_OTG_DEFAULT;
  pcore->dev.device_address = 0;

  /* Init ep structure */
  for (i = 0; i < pcore->cfg.dev_endpoints ; i++)
  {
    ep = &pcore->dev.in_ep[i];
    /* Init ep structure */
    ep->is_in = 1;
    ep->num = i;
    ep->tx_fifo_num = i;
    /* Control until ep is actvated */
    ep->type = EP_TYPE_CTRL;
    ep->maxpacket =  USB_OTG_MAX_EP0_SIZE;
    ep->xfer_buff = 0;
    ep->xfer_len = 0;
  }

  for (i = 0; i < pcore->cfg.dev_endpoints; i++)
  {
    ep = &pcore->dev.out_ep[i];
    /* Init ep structure */
    ep->is_in = 0;
    ep->num = i;
    ep->tx_fifo_num = i;
    /* Control until ep is activated */
    ep->type = EP_TYPE_CTRL;
    ep->maxpacket = USB_OTG_MAX_EP0_SIZE;
    ep->xfer_buff = 0;
    ep->xfer_len = 0;
  }

  USB_OTG_DisableGlobalInt(pcore);

  /*Init the Core (common init.) */
  USB_OTG_CoreInit(pcore);


  /* Force Device Mode*/
  USB_OTG_SetCurrentMode(pcore, DEVICE_MODE);

  /* Init Device */
  USB_OTG_CoreInitDev(pcore);


  /* Enable USB Global interrupt */
  USB_OTG_EnableGlobalInt(pcore);
}


/**
* @brief  Configure an EP
* @param pcore : Device instance
* @param epdesc : Endpoint Descriptor
* @retval : status
*/
uint32_t DCD_EP_Open(USB_OTG_CORE_HANDLE *pcore ,
                     uint8_t ep_addr,
                     uint16_t ep_mps,
                     uint8_t ep_type)
{
  USB_OTG_EP *ep;

  if ((ep_addr & 0x80) == 0x80)
  {
    ep = &pcore->dev.in_ep[ep_addr & 0x7F];
  }
  else
  {
    ep = &pcore->dev.out_ep[ep_addr & 0x7F];
  }
  ep->num   = ep_addr & 0x7F;

  ep->is_in = (0x80 & ep_addr) != 0;
  ep->maxpacket = ep_mps;
  ep->type = ep_type;
  if (ep->is_in)
  {
    /* Assign a Tx FIFO */
    /* Hack by Frank: only allow FIFO 0 and 1 to be used, 2 and 3 are not allocated, 4 doesn't exist */
    if (ep->num > 1) {
      ep->tx_fifo_num = 1;
    }
    else {
      ep->tx_fifo_num = ep->num;
    }
  }
  /* Set initial data PID. */
  if (ep_type == USB_OTG_EP_BULK )
  {
    ep->data_pid_start = 0;
  }
  USB_OTG_EPActivate(pcore , ep );
  return 0;
}
/**
* @brief  called when an EP is disabled
* @param pcore: device instance
* @param ep_addr: endpoint address
* @retval : status
*/
uint32_t DCD_EP_Close(USB_OTG_CORE_HANDLE *pcore , uint8_t  ep_addr)
{
  USB_OTG_EP *ep;

  if ((ep_addr&0x80) == 0x80)
  {
    ep = &pcore->dev.in_ep[ep_addr & 0x7F];
  }
  else
  {
    ep = &pcore->dev.out_ep[ep_addr & 0x7F];
  }
  ep->num   = ep_addr & 0x7F;
  ep->is_in = (0x80 & ep_addr) != 0;
  USB_OTG_EPDeactivate(pcore , ep );
  return 0;
}


/**
* @brief  DCD_EP_PrepareRx
* @param pcore: device instance
* @param ep_addr: endpoint address
* @param pbuf: pointer to Rx buffer
* @param buf_len: data length
* @retval : status
*/
uint32_t   DCD_EP_PrepareRx( USB_OTG_CORE_HANDLE *pcore,
                            uint8_t   ep_addr,
                            uint8_t *pbuf,
                            uint16_t  buf_len)
{
  USB_OTG_EP *ep;

  ep = &pcore->dev.out_ep[ep_addr & 0x7F];

  /*setup and start the Xfer */
  ep->xfer_buff = pbuf;
  ep->xfer_len = buf_len;
  ep->xfer_count = 0;
  ep->is_in = 0;
  ep->num = ep_addr & 0x7F;

  if (pcore->cfg.dma_enable == 1)
  {
    ep->dma_addr = (uint32_t)pbuf;
  }

  if ( ep->num == 0 )
  {
    USB_OTG_EP0StartXfer(pcore , ep);
  }
  else
  {
    USB_OTG_EPStartXfer(pcore, ep );
  }
  return 0;
}

/**
* @brief  Transmit data over USB
* @param pcore: device instance
* @param ep_addr: endpoint address
* @param pbuf: pointer to Tx buffer
* @param buf_len: data length
* @retval : status
*/
uint32_t  DCD_EP_Tx ( USB_OTG_CORE_HANDLE *pcore,
                     uint8_t   ep_addr,
                     uint8_t   *pbuf,
                     uint32_t   buf_len)
{
  USB_OTG_EP *ep;

  int epIdx = ep_addr & 0x7F;
  ep = &pcore->dev.in_ep[epIdx];

  /* Setup and start the Transfer */
  ep->is_in = 1;
  ep->num = epIdx;
  ep->xfer_buff = pbuf;
  ep->dma_addr = (uint32_t)pbuf;
  ep->xfer_count = 0;
  ep->xfer_len  = buf_len;

  if ( ep->num == 0 )
  {
    USB_OTG_EP0StartXfer(pcore , ep);
  }
  else
  {
    USB_OTG_EPStartXfer(pcore, ep );
  }
  return 0;
}


/**
* @brief  Stall an endpoint.
* @param pcore: device instance
* @param epnum: endpoint address
* @retval : status
*/
uint32_t  DCD_EP_Stall (USB_OTG_CORE_HANDLE *pcore, uint8_t   epnum)
{
  USB_OTG_EP *ep;
  if ((0x80 & epnum) == 0x80)
  {
    ep = &pcore->dev.in_ep[epnum & 0x7F];
  }
  else
  {
    ep = &pcore->dev.out_ep[epnum];
  }

  ep->is_stall = 1;
  ep->num   = epnum & 0x7F;
  ep->is_in = ((epnum & 0x80) == 0x80);

  USB_OTG_EPSetStall(pcore , ep);
  return (0);
}


/**
* @brief  Clear stall condition on endpoints.
* @param pcore: device instance
* @param epnum: endpoint address
* @retval : status
*/
uint32_t  DCD_EP_ClrStall (USB_OTG_CORE_HANDLE *pcore, uint8_t epnum)
{
  USB_OTG_EP *ep;
  if ((0x80 & epnum) == 0x80)
  {
    ep = &pcore->dev.in_ep[epnum & 0x7F];
  }
  else
  {
    ep = &pcore->dev.out_ep[epnum];
  }

  ep->is_stall = 0;
  ep->num   = epnum & 0x7F;
  ep->is_in = ((epnum & 0x80) == 0x80);

  USB_OTG_EPClearStall(pcore , ep);
  return (0);
}


/**
* @brief  This Function flushes the FIFOs.
* @param pcore: device instance
* @param epnum: endpoint address
* @retval : status
*/
uint32_t  DCD_EP_Flush (USB_OTG_CORE_HANDLE *pcore , uint8_t epnum)
{

  if ((epnum & 0x80) == 0x80)
  {
    USB_OTG_FlushTxFifo(pcore, epnum & 0x7F);
  }
  else
  {
    USB_OTG_FlushRxFifo(pcore);
  }

  return (0);
}


/**
* @brief  This Function set USB device address
* @param pcore: device instance
* @param address: new device address
* @retval : status
*/
void  DCD_EP_SetAddress (USB_OTG_CORE_HANDLE *pcore, uint8_t address)
{
  USB_OTG_DCFG_TypeDef  dcfg;
  dcfg.d32 = 0;
  dcfg.b.devaddr = address;
  USB_OTG_MODIFY_REG32( &pcore->regs.DREGS->DCFG, 0, dcfg.d32);
}

/**
* @brief  Connect device (enable internal pull-up)
* @param pcore: device instance
* @retval : None
*/
void  DCD_DevConnect (USB_OTG_CORE_HANDLE *pcore)
{
#ifndef USE_OTG_MODE
  USB_OTG_DCTL_TypeDef  dctl;
  dctl.d32 = USB_OTG_READ_REG32(&pcore->regs.DREGS->DCTL);
  /* Connect device */
  dctl.b.sftdiscon  = 0;
  USB_OTG_WRITE_REG32(&pcore->regs.DREGS->DCTL, dctl.d32);
  USBD_ExPullUp_On();
  USB_OTG_BSP_mDelay(3);
#else
#error bad config, do not use USE_OTG_MODE
#endif
}


/**
* @brief  Disconnect device (disable internal pull-up)
* @param pcore: device instance
* @retval : None
*/
void  DCD_DevDisconnect (USB_OTG_CORE_HANDLE *pcore)
{
#ifndef USE_OTG_MODE
  USB_OTG_DCTL_TypeDef  dctl;
  dctl.d32 = USB_OTG_READ_REG32(&pcore->regs.DREGS->DCTL);
  /* Disconnect device for 3ms */
  dctl.b.sftdiscon  = 1;
  USB_OTG_WRITE_REG32(&pcore->regs.DREGS->DCTL, dctl.d32);
  USBD_ExPullUp_Off();
  USB_OTG_BSP_mDelay(3);
#else
#error bad config, do not use USE_OTG_MODE
#endif
}


/**
* @brief  returns the EP Status
* @param  pcore : Selected Peripheral Core
*         epnum : endpoint address
* @retval : EP status
*/

uint32_t DCD_GetEPStatus(USB_OTG_CORE_HANDLE *pcore ,uint8_t epnum)
{
  USB_OTG_EP *ep;
  uint32_t Status = 0;

  if ((0x80 & epnum) == 0x80)
  {
    ep = &pcore->dev.in_ep[epnum & 0x7F];
  }
  else
  {
    ep = &pcore->dev.out_ep[epnum];
  }

  Status = USB_OTG_GetEPStatus(pcore ,ep);

  /* Return the current status */
  return Status;
}

/**
* @brief  Set the EP Status
* @param  pcore : Selected Peripheral Core
*         Status : new Status
*         epnum : EP address
* @retval : None
*/
void DCD_SetEPStatus (USB_OTG_CORE_HANDLE *pcore , uint8_t epnum , uint32_t Status)
{
  USB_OTG_EP *ep;

  if ((0x80 & epnum) == 0x80)
  {
    ep = &pcore->dev.in_ep[epnum & 0x7F];
  }
  else
  {
    ep = &pcore->dev.out_ep[epnum];
  }

   USB_OTG_SetEPStatus(pcore ,ep , Status);
}

/**
* @}
*/

/**
* @}
*/

/**
* @}
*/

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
