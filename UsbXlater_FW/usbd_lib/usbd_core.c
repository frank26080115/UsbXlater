/**
  ******************************************************************************
  * @file    usbd_core.c
  * @author  MCD Application Team
  * @version V1.1.0
  * @date    19-March-2012
  * @brief   This file provides all the USBD core functions.
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
#include "usbd_core.h"
#include "usbd_req.h"
#include "usbd_ioreq.h"
#include "usb_dcd_int.h"
#include <usbotg_lib/usb_bsp.h>

/** @addtogroup STM32_USB_OTG_DEVICE_LIBRARY
* @{
*/


/** @defgroup USBD_CORE
* @brief usbd core module
* @{
*/

/** @defgroup USBD_CORE_Private_TypesDefinitions
* @{
*/
/**
* @}
*/


/** @defgroup USBD_CORE_Private_Defines
* @{
*/

/**
* @}
*/


/** @defgroup USBD_CORE_Private_Macros
* @{
*/
/**
* @}
*/




/** @defgroup USBD_CORE_Private_FunctionPrototypes
* @{
*/
static uint8_t USBD_SetupStage(USB_OTG_CORE_HANDLE *pdev);
static uint8_t USBD_DataOutStage(USB_OTG_CORE_HANDLE *pdev , uint8_t epnum);
static uint8_t USBD_DataInStage(USB_OTG_CORE_HANDLE *pdev , uint8_t epnum);
static uint8_t USBD_SOF(USB_OTG_CORE_HANDLE  *pdev);
static uint8_t USBD_Reset(USB_OTG_CORE_HANDLE  *pdev);
static uint8_t USBD_Suspend(USB_OTG_CORE_HANDLE  *pdev);
static uint8_t USBD_Resume(USB_OTG_CORE_HANDLE  *pdev);
#ifdef VBUS_SENSING_ENABLED
static uint8_t USBD_DevConnected(USB_OTG_CORE_HANDLE  *pdev);
static uint8_t USBD_DevDisconnected(USB_OTG_CORE_HANDLE  *pdev);
#endif
static uint8_t USBD_IsoINIncomplete(USB_OTG_CORE_HANDLE  *pdev);
static uint8_t USBD_IsoOUTIncomplete(USB_OTG_CORE_HANDLE  *pdev);
static uint8_t USBD_RunTestMode (USB_OTG_CORE_HANDLE  *pdev) ;
/**
* @}
*/

/** @defgroup USBD_CORE_Private_Variables
* @{
*/

__IO USB_OTG_DCTL_TypeDef SET_TEST_MODE;

USBD_DCD_INT_cb_TypeDef USBD_DCD_INT_cb =
{
  USBD_DataOutStage,
  USBD_DataInStage,
  USBD_SetupStage,
  USBD_SOF,
  USBD_Reset,
  USBD_Suspend,
  USBD_Resume,
  USBD_IsoINIncomplete,
  USBD_IsoOUTIncomplete,
#ifdef VBUS_SENSING_ENABLED
USBD_DevConnected,
USBD_DevDisconnected,
#endif
};

USBD_DCD_INT_cb_TypeDef  *USBD_DCD_INT_fops = &USBD_DCD_INT_cb;
/**
* @}
*/

extern char USBPT_Is_Active; // from usb_passthrough

/** @defgroup USBD_CORE_Private_Functions
* @{
*/

/**
* @brief  USBD_Init
*         Initailizes the device stack and load the class driver
* @param  pdev: device instance
* @param  core_address: USB OTG core ID
* @param  cb: callback structure address
* @retval None
*/
void USBD_Init(USB_OTG_CORE_HANDLE *pdev,
               USB_OTG_CORE_ID_TypeDef coreID,
               USBD_Device_cb_TypeDef *cb)
{
  //USB_OTG_BSP_Init(pdev);

  /*Register class and user callbacks */
  pdev->dev.cb = cb;

  /* set USB OTG core params */
  DCD_Init(pdev , coreID);

  /* Upon Init call usr callback */
  pdev->dev.cb->UsrInit();

  /* Enable Interrupts */
  //USB_OTG_BSP_EnableInterrupt(pdev);
}

/**
* @brief  USBD_DeInit
*         Re-Initialize the device library
* @param  pdev: device instance
* @retval status: status
*/
USBD_Status USBD_DeInit(USB_OTG_CORE_HANDLE *pdev)
{
  pdev->dev.cb->ClassDeInit(pdev, 0);

  return USBD_OK;
}

/**
* @brief  USBD_SetupStage
*         Handle the setup stage
* @param  pdev: device instance
* @retval status
*/
static uint8_t USBD_SetupStage(USB_OTG_CORE_HANDLE *pdev)
{
	USB_SETUP_REQ req;

	USBD_ParseSetupRequest(pdev , &req);

	// this is a filter that passes all setup packets to the usb_passthrough code
	//*
	if (USBPT_Is_Active != 0)
	{
		if (USBPTD_SetupStage(pdev, &req) == USBD_OK) {
			return USBD_OK;
		}
	}
	//*/

	switch (req.bmRequest & 0x1F)
	{
		case USB_REQ_RECIPIENT_DEVICE:
			USBD_StdDevReq (pdev, &req);
			break;

		case USB_REQ_RECIPIENT_INTERFACE:
			USBD_StdItfReq(pdev, &req);
			break;

		case USB_REQ_RECIPIENT_ENDPOINT:
			USBD_StdEPReq(pdev, &req);
			break;

		default:
			dbg_printf(DBGMODE_ERR, "\r\n USBD_SetupStage Stall, unknown bmRequest 0x%02X, file " __FILE__ ":%d\r\n", req.bmRequest, __LINE__);
			DCD_EP_Stall(pdev , req.bmRequest & 0x80);
			break;
	}

	return USBD_OK;
}

/**
* @brief  USBD_DataOutStage
*         Handle data out stage
* @param  pdev: device instance
* @param  epnum: endpoint index
* @retval status
*/
static uint8_t USBD_DataOutStage(USB_OTG_CORE_HANDLE *pdev , uint8_t epnum)
{
  USB_OTG_EP *ep;

  if(epnum == 0)
  {
    ep = &pdev->dev.out_ep[0];
    if ( pdev->dev.device_state == USB_OTG_EP0_DATA_OUT)
    {
      if(ep->rem_data_len > ep->maxpacket)
      {
        ep->rem_data_len -=  ep->maxpacket;

        if(pdev->cfg.dma_enable == 1)
        {
          /* in slave mode this, is handled by the RxSTSQLvl ISR */
          ep->xfer_buff += ep->maxpacket;
        }
        USBD_CtlContinueRx (pdev,
                            ep->xfer_buff,
                            MIN(ep->rem_data_len ,ep->maxpacket));
      }
      else
      {
        if((pdev->dev.cb->EP0_RxReady != NULL)&&
           (pdev->dev.device_status == USB_OTG_CONFIGURED))
        {
          pdev->dev.cb->EP0_RxReady(pdev);
        }
        USBD_CtlSendStatus(pdev);
      }
    }
  }
  else if((pdev->dev.cb->DataOut != NULL)&&
          (pdev->dev.device_status == USB_OTG_CONFIGURED))
  {
    pdev->dev.cb->DataOut(pdev, epnum);
  }
  return USBD_OK;
}

/**
* @brief  USBD_DataInStage
*         Handle data in stage
* @param  pdev: device instance
* @param  epnum: endpoint index
* @retval status
*/
static uint8_t USBD_DataInStage(USB_OTG_CORE_HANDLE *pdev , uint8_t epnum)
{
  USB_OTG_EP *ep;

  if(epnum == 0)
  {
    ep = &pdev->dev.in_ep[0];
    if ( pdev->dev.device_state == USB_OTG_EP0_DATA_IN)
    {
      if(ep->rem_data_len > ep->maxpacket)
      {
        ep->rem_data_len -=  ep->maxpacket;
        if(pdev->cfg.dma_enable == 1)
        {
          /* in slave mode this, is handled by the TxFifoEmpty ISR */
          ep->xfer_buff += ep->maxpacket;
        }
        USBD_CtlContinueSendData (pdev,
                                  ep->xfer_buff,
                                  ep->rem_data_len);
      }
      else
      { /* last packet is MPS multiple, so send ZLP packet */
        if((ep->total_data_len % ep->maxpacket == 0) &&
           (ep->total_data_len >= ep->maxpacket) &&
             (ep->total_data_len < ep->ctl_data_len ))
        {

          USBD_CtlContinueSendData(pdev , NULL, 0);
          ep->ctl_data_len = 0;
        }
        else
        {
          if((pdev->dev.cb->EP0_TxSent != NULL)&&
             (pdev->dev.device_status == USB_OTG_CONFIGURED))
          {
            pdev->dev.cb->EP0_TxSent(pdev);
          }
          USBD_CtlReceiveStatus(pdev);
        }
      }
    }
    if (pdev->dev.test_mode == 1)
    {
      USBD_RunTestMode(pdev);
      pdev->dev.test_mode = 0;
    }
  }
  else if((pdev->dev.cb->DataIn != NULL)&&
          (pdev->dev.device_status == USB_OTG_CONFIGURED))
  {
    pdev->dev.cb->DataIn(pdev, epnum);
  }
  return USBD_OK;
}




/**
* @brief  USBD_RunTestMode
*         Launch test mode process
* @param  pdev: device instance
* @retval status
*/
static uint8_t  USBD_RunTestMode (USB_OTG_CORE_HANDLE  *pdev)
{
  USB_OTG_WRITE_REG32(&pdev->regs.DREGS->DCTL, SET_TEST_MODE.d32);
  return USBD_OK;
}

/**
* @brief  USBD_Reset
*         Handle Reset event
* @param  pdev: device instance
* @retval status
*/

static uint8_t USBD_Reset(USB_OTG_CORE_HANDLE  *pdev)
{
  // Open EP0 OUT
  DCD_EP_Open(pdev,
              0x00,
              USB_OTG_MAX_EP0_SIZE,
              EP_TYPE_CTRL);

  // Open EP0 IN
  DCD_EP_Open(pdev,
              0x80,
              USB_OTG_MAX_EP0_SIZE,
              EP_TYPE_CTRL);

  DCD_EP_Flush(pdev, 0x00);
  DCD_EP_Flush(pdev, 0x80);

  /* Upon Reset call usr call back */
  pdev->dev.device_status = USB_OTG_DEFAULT;
  pdev->dev.cb->DeviceReset(pdev->cfg.speed);

  return USBD_OK;
}

/**
* @brief  USBD_Resume
*         Handle Resume event
* @param  pdev: device instance
* @retval status
*/

static uint8_t USBD_Resume(USB_OTG_CORE_HANDLE  *pdev)
{
  /* Upon Resume call usr call back */
  pdev->dev.cb->DeviceResumed();
  pdev->dev.device_status = pdev->dev.device_old_status;
  pdev->dev.device_status = USB_OTG_CONFIGURED;
  return USBD_OK;
}


/**
* @brief  USBD_Suspend
*         Handle Suspend event
* @param  pdev: device instance
* @retval status
*/

static uint8_t USBD_Suspend(USB_OTG_CORE_HANDLE  *pdev)
{
  pdev->dev.device_old_status = pdev->dev.device_status;
  pdev->dev.device_status  = USB_OTG_SUSPENDED;
  /* Upon Resume call usr call back */
  pdev->dev.cb->DeviceSuspended();
  return USBD_OK;
}


/**
* @brief  USBD_SOF
*         Handle SOF event
* @param  pdev: device instance
* @retval status
*/

static uint8_t USBD_SOF(USB_OTG_CORE_HANDLE  *pdev)
{
  if(pdev->dev.cb->SOF)
  {
    pdev->dev.cb->SOF(pdev);
  }
  return USBD_OK;
}
/**
* @brief  USBD_SetCfg
*        Configure device and start the interface
* @param  pdev: device instance
* @param  cfgidx: configuration index
* @retval status
*/

USBD_Status USBD_SetCfg(USB_OTG_CORE_HANDLE  *pdev, uint8_t cfgidx)
{
  pdev->dev.cb->ClassInit(pdev, cfgidx);

  /* Upon set config call usr call back */
  pdev->dev.cb->DeviceConfigured();
  return USBD_OK;
}

/**
* @brief  USBD_ClrCfg
*         Clear current configuration
* @param  pdev: device instance
* @param  cfgidx: configuration index
* @retval status: USBD_Status
*/
USBD_Status USBD_ClrCfg(USB_OTG_CORE_HANDLE  *pdev, uint8_t cfgidx)
{
  pdev->dev.cb->ClassDeInit(pdev, cfgidx);
  return USBD_OK;
}

/**
* @brief  USBD_IsoINIncomplete
*         Handle iso in incomplete event
* @param  pdev: device instance
* @retval status
*/
static uint8_t USBD_IsoINIncomplete(USB_OTG_CORE_HANDLE  *pdev)
{
  pdev->dev.cb->IsoINIncomplete(pdev);
  return USBD_OK;
}

/**
* @brief  USBD_IsoOUTIncomplete
*         Handle iso out incomplete event
* @param  pdev: device instance
* @retval status
*/
static uint8_t USBD_IsoOUTIncomplete(USB_OTG_CORE_HANDLE  *pdev)
{
  pdev->dev.cb->IsoOUTIncomplete(pdev);
  return USBD_OK;
}

#ifdef VBUS_SENSING_ENABLED
/**
* @brief  USBD_DevConnected
*         Handle device connection event
* @param  pdev: device instance
* @retval status
*/
static uint8_t USBD_DevConnected(USB_OTG_CORE_HANDLE  *pdev)
{
  pdev->dev.cb->DeviceConnected();
  pdev->dev.connection_status = 1;
  return USBD_OK;
}

/**
* @brief  USBD_DevDisconnected
*         Handle device disconnection event
* @param  pdev: device instance
* @retval status
*/
static uint8_t USBD_DevDisconnected(USB_OTG_CORE_HANDLE  *pdev)
{
  pdev->dev.cb->DeviceDisconnected();
  pdev->dev.cb->DeInit(pdev, 0);
  pdev->dev.connection_status = 0;
  return USBD_OK;
}
#endif
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

