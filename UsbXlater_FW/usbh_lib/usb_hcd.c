/**
  ******************************************************************************
  * @file    usb_hcd.c
  * @author  MCD Application Team
  * @version V2.1.0
  * @date    19-March-2012
  * @brief   Host Interface Layer
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
#include <usbotg_lib/usb_core.h>
#include "usb_hcd.h"
#include <usbotg_lib/usb_conf.h>
#include <usbotg_lib/usb_bsp.h>


/** @addtogroup USB_OTG_DRIVER
  * @{
  */

/** @defgroup USB_HCD
  * @brief This file is the interface between EFSL ans Host mass-storage class
  * @{
  */


/** @defgroup USB_HCD_Private_Defines
  * @{
  */
/**
  * @}
  */


/** @defgroup USB_HCD_Private_TypesDefinitions
  * @{
  */
/**
  * @}
  */



/** @defgroup USB_HCD_Private_Macros
  * @{
  */
/**
  * @}
  */


/** @defgroup USB_HCD_Private_Variables
  * @{
  */
/**
  * @}
  */


/** @defgroup USB_HCD_Private_FunctionPrototypes
  * @{
  */
/**
  * @}
  */


/** @defgroup USB_HCD_Private_Functions
  * @{
  */

/**
  * @brief  HCD_Init
  *         Initialize the HOST portion of the driver.
  * @param  pcore: Selected Core Peripheral
  * @param  base_address: OTG base address
  * @retval Status
  */
uint32_t HCD_Init(USB_OTG_CORE_HANDLE *pcore ,
                  USB_OTG_CORE_ID_TypeDef coreID)
{
  uint8_t i = 0;
  pcore->host.ConnSts = 0;

  for (i= 0; i< USB_OTG_MAX_TX_FIFOS; i++)
  {
  pcore->host.ErrCnt[i]  = 0;
  pcore->host.XferCnt[i]   = 0;
  pcore->host.HC_Status[i]   = HC_IDLE;
  }
  pcore->host.hc[0].max_packet  = 8;

  USB_OTG_SelectCore(pcore, coreID);
#ifndef DUAL_ROLE_MODE_ENABLED
  USB_OTG_DisableGlobalInt(pcore);
  USB_OTG_CoreInit(pcore);

  /* Force Host Mode*/
  USB_OTG_SetCurrentMode(pcore , HOST_MODE);
  USB_OTG_CoreInitHost(pcore);
  USB_OTG_EnableGlobalInt(pcore);
#endif

  return 0;
}


/**
  * @brief  HCD_GetCurrentSpeed
  *         Get Current device Speed.
  * @param  pcore : Selected Peripheral Core
  * @retval Status
  */

uint32_t HCD_GetCurrentSpeed (USB_OTG_CORE_HANDLE *pcore)
{
    USB_OTG_HPRT0_TypeDef  HPRT0;
    HPRT0.d32 = USB_OTG_READ_REG32(pcore->regs.HPRT0);

    return HPRT0.b.prtspd;
}

/**
  * @brief  HCD_ResetPort
  *         Issues the reset command to device
  * @param  pcore : Selected Peripheral Core
  * @retval Status
  */
uint32_t HCD_ResetPort(USB_OTG_CORE_HANDLE *pcore)
{
  /*
  Before starting to drive a USB reset, the application waits for the OTG
  interrupt triggered by the debounce done bit (DBCDNE bit in OTG_FS_GOTGINT),
  which indicates that the bus is stable again after the electrical debounce
  caused by the attachment of a pull-up resistor on DP (FS) or DM (LS).
  */

  USB_OTG_ResetPort(pcore);
  return 0;
}

/**
  * @brief  HCD_IsDeviceConnected
  *         Check if the device is connected.
  * @param  pcore : Selected Peripheral Core
  * @retval Device connection status. 1 -> connected and 0 -> disconnected
  *
  */
uint32_t HCD_IsDeviceConnected(USB_OTG_CORE_HANDLE *pcore)
{
  return (pcore->host.ConnSts);
}

/**
  * @brief  HCD_GetCurrentFrame
  *         This function returns the frame number for sof packet
  * @param  pcore : Selected Peripheral Core
  * @retval Frame number
  *
  */
uint32_t HCD_GetCurrentFrame (USB_OTG_CORE_HANDLE *pcore)
{
 return (USB_OTG_READ_REG32(&pcore->regs.HREGS->HFNUM) & 0xFFFF) ;
}

/**
  * @brief  HCD_GetURB_State
  *         This function returns the last URBstate
  * @param  pcore: Selected Core Peripheral
  * @retval URB_STATE
  *
  */
URB_STATE HCD_GetURB_State (USB_OTG_CORE_HANDLE *pcore , uint8_t ch_num)
{
  return pcore->host.URB_State[ch_num] ;
}

/**
  * @brief  HCD_GetXferCnt
  *         This function returns the last URBstate
  * @param  pcore: Selected Core Peripheral
  * @retval No. of data bytes transferred
  *
  */
uint32_t HCD_GetXferCnt (USB_OTG_CORE_HANDLE *pcore, uint8_t ch_num)
{
  return pcore->host.XferCnt[ch_num] ;
}



/**
  * @brief  HCD_GetHCState
  *         This function returns the HC Status
  * @param  pcore: Selected Core Peripheral
  * @retval HC_STATUS
  *
  */
HC_STATUS HCD_GetHCState (USB_OTG_CORE_HANDLE *pcore ,  uint8_t ch_num)
{
  return pcore->host.HC_Status[ch_num] ;
}

/**
  * @brief  HCD_HC_Init
  *         This function prepare a HC and start a transfer
  * @param  pcore: Selected Core Peripheral
  * @param  hc_num: Channel number
  * @retval status
  */
uint32_t HCD_HC_Init (USB_OTG_CORE_HANDLE *pcore , uint8_t hc_num)
{
  return USB_OTG_HC_Init(pcore, hc_num);
}

/**
  * @brief  HCD_SubmitRequest
  *         This function prepare a HC and start a transfer
  * @param  pcore: Selected Core Peripheral
  * @param  hc_num: Channel number
  * @retval status
  */
uint32_t HCD_SubmitRequest (USB_OTG_CORE_HANDLE *pcore , uint8_t hc_num)
{

  pcore->host.URB_State[hc_num] = URB_IDLE;
  pcore->host.hc[hc_num].xfer_count = 0 ;
  return USB_OTG_HC_StartXfer(pcore, hc_num);
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
