/**
  ******************************************************************************
  * @file    usbh_hcs.c
  * @author  MCD Application Team
  * @version V2.1.0
  * @date    19-March-2012
  * @brief   This file implements functions for opening and closing host channels
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
#include "usbh_hcs.h"

/** @addtogroup USBH_LIB
  * @{
  */

/** @addtogroup USBH_LIB_CORE
* @{
*/

/** @defgroup USBH_HCS
  * @brief This file includes opening and closing host channels
  * @{
  */

/** @defgroup USBH_HCS_Private_Defines
  * @{
  */
/**
  * @}
  */

/** @defgroup USBH_HCS_Private_TypesDefinitions
  * @{
  */
/**
  * @}
  */


/** @defgroup USBH_HCS_Private_Macros
  * @{
  */
/**
  * @}
  */


/** @defgroup USBH_HCS_Private_Variables
  * @{
  */

/**
  * @}
  */


/** @defgroup USBH_HCS_Private_FunctionPrototypes
  * @{
  */
static int16_t USBH_GetFreeChannel (USB_OTG_CORE_HANDLE *pcore);
static int16_t USBH_Alloc_Channel (USB_OTG_CORE_HANDLE *pcore, uint8_t ep_addr);
/**
  * @}
  */


/** @defgroup USBH_HCS_Private_Functions
  * @{
  */



/**
  * @brief  USBH_Open_Channel
  *         Open a  pipe
  * @param  pcore : Selected Peripheral Core
  * @param  hc_num: Host channel Number
  * @param  dev_address: USB Device address allocated to attached device
  * @param  speed : USB device speed (Full/Low)
  * @param  ep_type: end point type (Bulk/int/ctl)
  * @param  mps: max pkt size
  * @retval Status
  */
int8_t USBH_Open_Channel  (USB_OTG_CORE_HANDLE *pcore,
                            int8_t* hc_num,
                            uint8_t ep_num,
                            uint8_t dev_address,
                            uint8_t speed,
                            uint8_t ep_type,
                            uint16_t mps)
{

  int16_t possibleHc = USBH_Alloc_Channel(pcore, ep_num);
  *hc_num = possibleHc;
  if (possibleHc < 0) return HC_ERROR;
  pcore->host.hc[possibleHc].ep_num     = pcore->host.channel[possibleHc]  & 0x7F;
  pcore->host.hc[possibleHc].ep_is_in   = (pcore->host.channel[possibleHc] & 0x80 ) == 0x80;
  pcore->host.hc[possibleHc].dev_addr   = dev_address;
  pcore->host.hc[possibleHc].ep_type    = ep_type;
  pcore->host.hc[possibleHc].max_packet = mps;
  pcore->host.hc[possibleHc].speed      = speed;
  pcore->host.hc[possibleHc].toggle_in  = 0;
  pcore->host.hc[possibleHc].toggle_out = 0;
  if(speed == HPRT0_PRTSPD_HIGH_SPEED)
  {
    pcore->host.hc[possibleHc].do_ping = 1;
  }

#ifdef USBH_HCS_ENABLE_DEBUG
  dbg_printf(DBGMODE_DEBUG, "USBH_Open_Channel, HC %d, ep# %d, is_in %d, dev_addr %d, \r\n",
		  *hc_num,
		  pcore->host.hc[possibleHc].ep_num,
		  pcore->host.hc[possibleHc].ep_is_in,
		  pcore->host.hc[possibleHc].dev_addr);
#endif

  USB_OTG_HC_Init(pcore, possibleHc);

  return HC_OK;

}

/**
  * @brief  USBH_Modify_Channel
  *         Modify a  pipe
  * @param  pcore : Selected Peripheral Core
  * @param  hc_num: Host channel Number
  * @param  dev_address: USB Device address allocated to attached device
  * @param  speed : USB device speed (Full/Low)
  * @param  ep_type: end point type (Bulk/int/ctl)
  * @param  mps: max pkt size
  * @retval Status
  */
int8_t USBH_Modify_Channel(USB_OTG_CORE_HANDLE *pcore,
                            int8_t hc_num,
                            uint8_t dev_address,
                            uint8_t speed,
                            uint8_t ep_type,
                            uint16_t mps)
{

  if (hc_num < 0) return HC_ERROR;
  if(dev_address != 0)
  {
    pcore->host.hc[hc_num].dev_addr = dev_address;
  }

  if((pcore->host.hc[hc_num].max_packet != mps) && (mps != 0))
  {
    pcore->host.hc[hc_num].max_packet = mps;
  }

  if((pcore->host.hc[hc_num].speed != speed ) && (speed != 0 ))
  {
    pcore->host.hc[hc_num].speed = speed;
  }

  USB_OTG_HC_Init(pcore, hc_num);
  return HC_OK;

}

/**
  * @brief  USBH_Alloc_Channel
  *         Allocate a new channel for the pipe
  * @param  ep_addr: End point for which the channel to be allocated
  * @retval hc_num: Host channel number
  */
static int16_t USBH_Alloc_Channel (USB_OTG_CORE_HANDLE *pcore, uint8_t ep_addr)
{
  int16_t hc_num;

  hc_num =  USBH_GetFreeChannel(pcore);

#ifdef USBH_HCS_ENABLE_DEBUG
  dbg_printf(DBGMODE_DEBUG, "USBH_Alloc_Channel HC allocated: %d\r\n", hc_num);
#endif

  if (hc_num >= 0)
  {
    pcore->host.channel[hc_num] = HC_USED | ep_addr;
  }
  return hc_num;
}

/**
  * @brief  USBH_Free_Pipe
  *         Free the USB host channel and halts it
  * @param  idx: Channel number to be freed
  * @retval Status
  */
int8_t USBH_Free_Channel  (USB_OTG_CORE_HANDLE *pcore, int8_t* idx)
{

  if((*idx) >= 0)
  {
    USB_OTG_HC_Halt(pcore, *idx);
    pcore->host.channel[(*idx)] &= HC_USED_MASK;
  }
  (*idx) = -1;
  return USBH_OK;
}


/**
  * @brief  USBH_DeAllocate_AllChannel
  *         Free all USB host channel
* @param  pcore : core instance
  * @retval Status
  */
uint8_t USBH_DeAllocate_AllChannel  (USB_OTG_CORE_HANDLE *pcore)
{
   uint8_t idx;

   for (idx = 0; idx < HC_MAX ; idx++)
   {
	 pcore->host.channel[idx] = 0;
   }
   return USBH_OK;
}

/**
  * @brief  USBH_GetFreeChannel
  *         Get a free channel number for allocation to a device endpoint
  * @param  None
  * @retval idx: Free Channel number
  */
static int16_t USBH_GetFreeChannel (USB_OTG_CORE_HANDLE *pcore)
{
  uint8_t idx = 0;

  for (idx = 0 ; idx < HC_MAX ; idx++)
  {
    if ((pcore->host.channel[idx] & HC_USED) == 0)
    {
#ifdef USBH_HCS_ENABLE_DEBUG
       dbg_printf(DBGMODE_DEBUG, "USBH_GetFreeChannel HC allocated: %d\r\n", idx);
#endif
       return idx;
    }
  }
  return HC_ERROR;
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

/**
* @}
*/

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/


