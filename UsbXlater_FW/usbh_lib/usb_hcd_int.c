/**
  ******************************************************************************
  * @file    usb_hcd_int.c
  * @author  MCD Application Team
  * @version V2.1.0
  * @date    19-March-2012
  * @brief   Host driver interrupt subroutines
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
#include <usbotg_lib/usb_defines.h>
#include "usb_hcd_int.h"

#if defined   (__CC_ARM) /*!< ARM Compiler */
#pragma O0
#elif defined (__GNUC__) /*!< GNU Compiler */
#pragma GCC optimize ("O0")
#elif defined  (__TASKING__) /*!< TASKING Compiler */
#pragma optimize=0

#endif /* __CC_ARM */

/** @addtogroup USB_OTG_DRIVER
* @{
*/

/** @defgroup USB_HCD_INT
* @brief This file contains the interrupt subroutines for the Host mode.
* @{
*/


/** @defgroup USB_HCD_INT_Private_Defines
* @{
*/
/**
* @}
*/


/** @defgroup USB_HCD_INT_Private_TypesDefinitions
* @{
*/
/**
* @}
*/



/** @defgroup USB_HCD_INT_Private_Macros
* @{
*/
/**
* @}
*/


/** @defgroup USB_HCD_INT_Private_Variables
* @{
*/
/**
* @}
*/


/** @defgroup USB_HCD_INT_Private_FunctionPrototypes
* @{
*/

static uint32_t USB_OTG_USBH_handle_sof_ISR(USB_OTG_CORE_HANDLE *pcore);
static uint32_t USB_OTG_USBH_handle_port_ISR(USB_OTG_CORE_HANDLE *pcore);
static uint32_t USB_OTG_USBH_handle_hc_ISR (USB_OTG_CORE_HANDLE *pcore);
static uint32_t USB_OTG_USBH_handle_hc_n_In_ISR (USB_OTG_CORE_HANDLE *pcore ,
                                                 uint32_t num);
static uint32_t USB_OTG_USBH_handle_hc_n_Out_ISR (USB_OTG_CORE_HANDLE *pcore ,
                                                  uint32_t num);
static uint32_t USB_OTG_USBH_handle_rx_qlvl_ISR (USB_OTG_CORE_HANDLE *pcore);
static uint32_t USB_OTG_USBH_handle_nptxfempty_ISR (USB_OTG_CORE_HANDLE *pcore);
static uint32_t USB_OTG_USBH_handle_ptxfempty_ISR (USB_OTG_CORE_HANDLE *pcore);
static uint32_t USB_OTG_USBH_handle_Disconnect_ISR (USB_OTG_CORE_HANDLE *pcore);
static uint32_t USB_OTG_USBH_handle_IncompletePeriodicXfer_ISR (USB_OTG_CORE_HANDLE *pcore);

/**
* @}
*/


/** @defgroup USB_HCD_INT_Private_Functions
* @{
*/

/**
* @brief  HOST_Handle_ISR
*         This function handles all USB Host Interrupts
* @param  pcore: Selected Core Peripheral
* @retval status
*/

uint32_t USBH_OTG_ISR_Handler (USB_OTG_CORE_HANDLE *pcore)
{
  USB_OTG_GINTSTS_TypeDef  gintsts;
  uint32_t retval = 0;

  gintsts.d32 = 0;

  /* Check if HOST Mode */
  if (USB_OTG_IsHostMode(pcore))
  {
    gintsts.d32 = USB_OTG_ReadCoreItr(pcore);
    if (!gintsts.d32)
    {
      return 0;
    }

    if (gintsts.b.sofintr)
    {
      retval |= USB_OTG_USBH_handle_sof_ISR (pcore);
    }

    if (gintsts.b.rxstsqlvl)
    {
      retval |= USB_OTG_USBH_handle_rx_qlvl_ISR (pcore);
    }

    if (gintsts.b.nptxfempty)
    {
      retval |= USB_OTG_USBH_handle_nptxfempty_ISR (pcore);
    }

    if (gintsts.b.ptxfempty)
    {
      retval |= USB_OTG_USBH_handle_ptxfempty_ISR (pcore);
    }

    if (gintsts.b.hcintr)
    {
      retval |= USB_OTG_USBH_handle_hc_ISR (pcore);
    }

    if (gintsts.b.portintr)
    {
      retval |= USB_OTG_USBH_handle_port_ISR (pcore);
    }

    if (gintsts.b.disconnect)
    {
      retval |= USB_OTG_USBH_handle_Disconnect_ISR (pcore);

    }

    if (gintsts.b.incomplisoout)
    {
      retval |= USB_OTG_USBH_handle_IncompletePeriodicXfer_ISR (pcore);
    }


  }
  return retval;
}

/**
* @brief  USB_OTG_USBH_handle_hc_ISR
*         This function indicates that one or more host channels has a pending
* @param  pcore: Selected Core Peripheral
* @retval status
*/
static uint32_t USB_OTG_USBH_handle_hc_ISR (USB_OTG_CORE_HANDLE *pcore)
{
  USB_OTG_HAINT_TypeDef        haint;
  USB_OTG_HCCHAR_TypeDef       hcchar;
  uint32_t i = 0;
  uint32_t retval = 0;

  /* Clear appropriate bits in HCINTn to clear the interrupt bit in
  * GINTSTS */

  haint.d32 = USB_OTG_ReadHostAllChannels_intr(pcore);

  for (i = 0; i < pcore->cfg.host_channels ; i++)
  {
    if (haint.b.chint & (1 << i))
    {
      hcchar.d32 = USB_OTG_READ_REG32(&pcore->regs.HC_REGS[i]->HCCHAR);

      if (hcchar.b.epdir)
      {
        retval |= USB_OTG_USBH_handle_hc_n_In_ISR (pcore, i);
      }
      else
      {
        retval |=  USB_OTG_USBH_handle_hc_n_Out_ISR (pcore, i);
      }
    }
  }

  return retval;
}

/**
* @brief  USB_OTG_otg_hcd_handle_sof_intr
*         Handles the start-of-frame interrupt in host mode.
* @param  pcore: Selected Core Peripheral
* @retval status
*/
static uint32_t USB_OTG_USBH_handle_sof_ISR (USB_OTG_CORE_HANDLE *pcore)
{
  USB_OTG_GINTSTS_TypeDef      gintsts;
  gintsts.d32 = 0;

  USBH_HCD_INT_fops->SOF(pcore);

  /* Clear interrupt */
  gintsts.b.sofintr = 1;
  USB_OTG_WRITE_REG32(&pcore->regs.GREGS->GINTSTS, gintsts.d32);

  return 1;
}

/**
* @brief  USB_OTG_USBH_handle_Disconnect_ISR
*         Handles disconnect event.
* @param  pcore: Selected Core Peripheral
* @retval status
*/
static uint32_t USB_OTG_USBH_handle_Disconnect_ISR (USB_OTG_CORE_HANDLE *pcore)
{
  USB_OTG_GINTSTS_TypeDef      gintsts;

  gintsts.d32 = 0;

  USBH_HCD_INT_fops->DevDisconnected(pcore);

  /* Clear interrupt */
  gintsts.b.disconnect = 1;
  USB_OTG_WRITE_REG32(&pcore->regs.GREGS->GINTSTS, gintsts.d32);

  return 1;
}
#if defined ( __ICCARM__ ) /*!< IAR Compiler */
#pragma optimize = none
#endif /* __CC_ARM */
/**
* @brief  USB_OTG_USBH_handle_nptxfempty_ISR
*         Handles non periodic tx fifo empty.
* @param  pcore: Selected Core Peripheral
* @retval status
*/
static uint32_t USB_OTG_USBH_handle_nptxfempty_ISR (USB_OTG_CORE_HANDLE *pcore)
{
  USB_OTG_GINTMSK_TypeDef      intmsk;
  USB_OTG_HNPTXSTS_TypeDef     hnptxsts;
  uint16_t                     len_words , len;

  hnptxsts.d32 = USB_OTG_READ_REG32(&pcore->regs.GREGS->HNPTXSTS);

  len_words = (pcore->host.hc[hnptxsts.b.nptxqtop.chnum].xfer_len + 3) / 4;

  while ((hnptxsts.b.nptxfspcavail > len_words)&&
         (pcore->host.hc[hnptxsts.b.nptxqtop.chnum].xfer_len != 0))
  {

    len = hnptxsts.b.nptxfspcavail * 4;

    if (len > pcore->host.hc[hnptxsts.b.nptxqtop.chnum].xfer_len)
    {
      /* Last packet */
      len = pcore->host.hc[hnptxsts.b.nptxqtop.chnum].xfer_len;

      intmsk.d32 = 0;
      intmsk.b.nptxfempty = 1;
      USB_OTG_MODIFY_REG32( &pcore->regs.GREGS->GINTMSK, intmsk.d32, 0);
    }

    len_words = (pcore->host.hc[hnptxsts.b.nptxqtop.chnum].xfer_len + 3) / 4;

    USB_OTG_WritePacket (pcore , pcore->host.hc[hnptxsts.b.nptxqtop.chnum].xfer_buff, hnptxsts.b.nptxqtop.chnum, len);

    pcore->host.hc[hnptxsts.b.nptxqtop.chnum].xfer_buff  += len;
    pcore->host.hc[hnptxsts.b.nptxqtop.chnum].xfer_len   -= len;
    pcore->host.hc[hnptxsts.b.nptxqtop.chnum].xfer_count  += len;

    hnptxsts.d32 = USB_OTG_READ_REG32(&pcore->regs.GREGS->HNPTXSTS);
  }

  return 1;
}
#if defined ( __ICCARM__ ) /*!< IAR Compiler */
#pragma optimize = none
#endif /* __CC_ARM */
/**
* @brief  USB_OTG_USBH_handle_ptxfempty_ISR
*         Handles periodic tx fifo empty
* @param  pcore: Selected Core Peripheral
* @retval status
*/
static uint32_t USB_OTG_USBH_handle_ptxfempty_ISR (USB_OTG_CORE_HANDLE *pcore)
{
  USB_OTG_GINTMSK_TypeDef      intmsk;
  USB_OTG_HPTXSTS_TypeDef      hptxsts;
  uint16_t                     len_words , len;

  hptxsts.d32 = USB_OTG_READ_REG32(&pcore->regs.HREGS->HPTXSTS);

  len_words = (pcore->host.hc[hptxsts.b.ptxqtop.chnum].xfer_len + 3) / 4;

  while ((hptxsts.b.ptxfspcavail > len_words)&&
         (pcore->host.hc[hptxsts.b.ptxqtop.chnum].xfer_len != 0))
  {

    len = hptxsts.b.ptxfspcavail * 4;

    if (len > pcore->host.hc[hptxsts.b.ptxqtop.chnum].xfer_len)
    {
      len = pcore->host.hc[hptxsts.b.ptxqtop.chnum].xfer_len;
      /* Last packet */
      intmsk.d32 = 0;
      intmsk.b.ptxfempty = 1;
      USB_OTG_MODIFY_REG32( &pcore->regs.GREGS->GINTMSK, intmsk.d32, 0);
    }

    len_words = (pcore->host.hc[hptxsts.b.ptxqtop.chnum].xfer_len + 3) / 4;

    USB_OTG_WritePacket (pcore , pcore->host.hc[hptxsts.b.ptxqtop.chnum].xfer_buff, hptxsts.b.ptxqtop.chnum, len);

    pcore->host.hc[hptxsts.b.ptxqtop.chnum].xfer_buff  += len;
    pcore->host.hc[hptxsts.b.ptxqtop.chnum].xfer_len   -= len;
    pcore->host.hc[hptxsts.b.ptxqtop.chnum].xfer_count  += len;

    hptxsts.d32 = USB_OTG_READ_REG32(&pcore->regs.HREGS->HPTXSTS);
  }

  return 1;
}

/**
* @brief  USB_OTG_USBH_handle_port_ISR
*         This function determines which interrupt conditions have occurred
* @param  pcore: Selected Core Peripheral
* @retval status
*/
#if defined ( __ICCARM__ ) /*!< IAR Compiler */
#pragma optimize = none
#endif /* __CC_ARM */
static uint32_t USB_OTG_USBH_handle_port_ISR (USB_OTG_CORE_HANDLE *pcore)
{
  USB_OTG_HPRT0_TypeDef  hprt0;
  USB_OTG_HPRT0_TypeDef  hprt0_dup;
  USB_OTG_HCFG_TypeDef   hcfg;
  uint32_t do_reset = 0;
  uint32_t retval = 0;

  hcfg.d32 = 0;
  hprt0.d32 = 0;
  hprt0_dup.d32 = 0;

  hprt0.d32 = USB_OTG_READ_REG32(pcore->regs.HPRT0);
  hprt0_dup.d32 = USB_OTG_READ_REG32(pcore->regs.HPRT0);

  /* Clear the interrupt bits in GINTSTS */

  hprt0_dup.b.prtena = 0;
  hprt0_dup.b.prtconndet = 0;
  hprt0_dup.b.prtenchng = 0;
  hprt0_dup.b.prtovrcurrchng = 0;

  /* Port Connect Detected */
  if (hprt0.b.prtconndet)
  {

    hprt0_dup.b.prtconndet = 1;
    USBH_HCD_INT_fops->DevConnected(pcore);
    retval |= 1;
  }

  /* Port Enable Changed */
  if (hprt0.b.prtenchng)
  {
    hprt0_dup.b.prtenchng = 1;

    if (hprt0.b.prtena == 1)
    {

      USBH_HCD_INT_fops->DevConnected(pcore);

      if ((hprt0.b.prtspd == HPRT0_PRTSPD_LOW_SPEED) ||
          (hprt0.b.prtspd == HPRT0_PRTSPD_FULL_SPEED))
      {

        hcfg.d32 = USB_OTG_READ_REG32(&pcore->regs.HREGS->HCFG);

        if (hprt0.b.prtspd == HPRT0_PRTSPD_LOW_SPEED)
        {
          USB_OTG_WRITE_REG32(&pcore->regs.HREGS->HFIR, 6000 );
          if (hcfg.b.fslspclksel != HCFG_6_MHZ)
          {
            if(pcore->cfg.phy_itface  == USB_OTG_EMBEDDED_PHY)
            {
              USB_OTG_InitFSLSPClkSel(pcore ,HCFG_6_MHZ );
            }
            do_reset = 1;
          }
        }
        else
        {

          USB_OTG_WRITE_REG32(&pcore->regs.HREGS->HFIR, 48000 );
          if (hcfg.b.fslspclksel != HCFG_48_MHZ)
          {
            USB_OTG_InitFSLSPClkSel(pcore ,HCFG_48_MHZ );
            do_reset = 1;
          }
        }
      }
      else
      {
        do_reset = 1;
      }
    }
  }
  /* Overcurrent Change Interrupt */
  if (hprt0.b.prtovrcurrchng)
  {
    hprt0_dup.b.prtovrcurrchng = 1;
    retval |= 1;
  }
  if (do_reset)
  {
    USB_OTG_ResetPort(pcore);
  }
  /* Clear Port Interrupts */
  USB_OTG_WRITE_REG32(pcore->regs.HPRT0, hprt0_dup.d32);

  return retval;
}
#if defined ( __ICCARM__ ) /*!< IAR Compiler */
#pragma optimize = none
#endif /* __CC_ARM */
/**
* @brief  USB_OTG_USBH_handle_hc_n_Out_ISR
*         Handles interrupt for a specific Host Channel
* @param  pcore: Selected Core Peripheral
* @param  hc_num: Channel number
* @retval status
*/
uint32_t USB_OTG_USBH_handle_hc_n_Out_ISR (USB_OTG_CORE_HANDLE *pcore , uint32_t num)
{

  USB_OTG_HCINTn_TypeDef     hcint;
  USB_OTG_HCINTMSK_TypeDef  hcintmsk;
  USB_OTG_HC_REGS *hcreg;
  USB_OTG_HCCHAR_TypeDef     hcchar;

  hcreg = pcore->regs.HC_REGS[num];
  hcint.d32 = USB_OTG_READ_REG32(&hcreg->HCINT);
  hcintmsk.d32 = USB_OTG_READ_REG32(&hcreg->HCINTMSK);
  hcint.d32 = hcint.d32 & hcintmsk.d32;

  hcchar.d32 = USB_OTG_READ_REG32(&pcore->regs.HC_REGS[num]->HCCHAR);

  if (hcint.b.ahberr)
  {
    CLEAR_HC_INT(hcreg ,ahberr);
    UNMASK_HOST_INT_CHH (num);
  }
  else if (hcint.b.ack)
  {
    CLEAR_HC_INT(hcreg , ack);
  }
  else if (hcint.b.frmovrun)
  {
    UNMASK_HOST_INT_CHH (num);
    USB_OTG_HC_Halt(pcore, num);
    CLEAR_HC_INT(hcreg ,frmovrun);
  }
  else if (hcint.b.xfercompl)
  {
    pcore->host.ErrCnt[num] = 0;
    UNMASK_HOST_INT_CHH (num);
    USB_OTG_HC_Halt(pcore, num);
    CLEAR_HC_INT(hcreg , xfercompl);
    pcore->host.HC_Status[num] = HC_XFRC;
  }

  else if (hcint.b.stall)
  {
    CLEAR_HC_INT(hcreg , stall);
    UNMASK_HOST_INT_CHH (num);
    USB_OTG_HC_Halt(pcore, num);
    pcore->host.HC_Status[num] = HC_STALL;
  }

  else if (hcint.b.nak)
  {
    pcore->host.ErrCnt[num] = 0;
    UNMASK_HOST_INT_CHH (num);
    USB_OTG_HC_Halt(pcore, num);
    CLEAR_HC_INT(hcreg , nak);
    pcore->host.HC_Status[num] = HC_NAK;
  }

  else if (hcint.b.xacterr)
  {
    UNMASK_HOST_INT_CHH (num);
    USB_OTG_HC_Halt(pcore, num);
    pcore->host.ErrCnt[num] ++;
    pcore->host.HC_Status[num] = HC_XACTERR;
    CLEAR_HC_INT(hcreg , xacterr);
  }
  else if (hcint.b.nyet)
  {
    pcore->host.ErrCnt[num] = 0;
    UNMASK_HOST_INT_CHH (num);
    USB_OTG_HC_Halt(pcore, num);
    CLEAR_HC_INT(hcreg , nyet);
    pcore->host.HC_Status[num] = HC_NYET;
  }
  else if (hcint.b.datatglerr)
  {

    UNMASK_HOST_INT_CHH (num);
    USB_OTG_HC_Halt(pcore, num);
    CLEAR_HC_INT(hcreg , nak);
    pcore->host.HC_Status[num] = HC_DATATGLERR;

    CLEAR_HC_INT(hcreg , datatglerr);
  }
  else if (hcint.b.chhltd)
  {
    MASK_HOST_INT_CHH (num);

    if(pcore->host.HC_Status[num] == HC_XFRC)
    {
      pcore->host.URB_State[num] = URB_DONE;

      if (hcchar.b.eptype == EP_TYPE_BULK)
      {
        pcore->host.hc[num].toggle_out ^= 1;
      }
    }
    else if(pcore->host.HC_Status[num] == HC_NAK)
    {
      pcore->host.URB_State[num] = URB_NOTREADY;
    }
    else if(pcore->host.HC_Status[num] == HC_NYET)
    {
      if(pcore->host.hc[num].do_ping == 1)
      {
        USB_OTG_HC_DoPing(pcore, num);
      }
      pcore->host.URB_State[num] = URB_NOTREADY;
    }
    else if(pcore->host.HC_Status[num] == HC_STALL)
    {
      pcore->host.URB_State[num] = URB_STALL;
    }
    else if(pcore->host.HC_Status[num] == HC_XACTERR)
    {
      if (pcore->host.ErrCnt[num] == 3)
      {
        pcore->host.URB_State[num] = URB_ERROR;
        pcore->host.ErrCnt[num] = 0;
      }
    }
    CLEAR_HC_INT(hcreg , chhltd);
  }


  return 1;
}
#if defined ( __ICCARM__ ) /*!< IAR Compiler */
#pragma optimize = none
#endif /* __CC_ARM */
/**
* @brief  USB_OTG_USBH_handle_hc_n_In_ISR
*         Handles interrupt for a specific Host Channel
* @param  pcore: Selected Core Peripheral
* @param  hc_num: Channel number
* @retval status
*/
uint32_t USB_OTG_USBH_handle_hc_n_In_ISR (USB_OTG_CORE_HANDLE *pcore , uint32_t num)
{
  USB_OTG_HCINTn_TypeDef     hcint;
  USB_OTG_HCINTMSK_TypeDef  hcintmsk;
  USB_OTG_HCCHAR_TypeDef     hcchar;
  USB_OTG_HCTSIZn_TypeDef  hctsiz;
  USB_OTG_HC_REGS *hcreg;


  hcreg = pcore->regs.HC_REGS[num];
  hcint.d32 = USB_OTG_READ_REG32(&hcreg->HCINT);
  hcintmsk.d32 = USB_OTG_READ_REG32(&hcreg->HCINTMSK);
  hcint.d32 = hcint.d32 & hcintmsk.d32;
  hcchar.d32 = USB_OTG_READ_REG32(&pcore->regs.HC_REGS[num]->HCCHAR);
  hcintmsk.d32 = 0;


  if (hcint.b.ahberr)
  {
    CLEAR_HC_INT(hcreg ,ahberr);
    UNMASK_HOST_INT_CHH (num);
  }
  else if (hcint.b.ack)
  {
    CLEAR_HC_INT(hcreg ,ack);
  }

  else if (hcint.b.stall)
  {
    UNMASK_HOST_INT_CHH (num);
    pcore->host.HC_Status[num] = HC_STALL;
    CLEAR_HC_INT(hcreg , nak);   /* Clear the NAK Condition */
    CLEAR_HC_INT(hcreg , stall); /* Clear the STALL Condition */
    hcint.b.nak = 0;           /* NOTE: When there is a 'stall', reset also nak,
                                  else, the pcore->host.HC_Status = HC_STALL
    will be overwritten by 'nak' in code below */
    USB_OTG_HC_Halt(pcore, num);
  }
  else if (hcint.b.datatglerr)
  {

    UNMASK_HOST_INT_CHH (num);
    USB_OTG_HC_Halt(pcore, num);
    CLEAR_HC_INT(hcreg , nak);
    pcore->host.HC_Status[num] = HC_DATATGLERR;
    CLEAR_HC_INT(hcreg , datatglerr);
  }

  if (hcint.b.frmovrun)
  {
    UNMASK_HOST_INT_CHH (num);
    USB_OTG_HC_Halt(pcore, num);
    CLEAR_HC_INT(hcreg ,frmovrun);
  }

  else if (hcint.b.xfercompl)
  {

    if (pcore->cfg.dma_enable == 1)
    {
      hctsiz.d32 = USB_OTG_READ_REG32(&pcore->regs.HC_REGS[num]->HCTSIZ);
      pcore->host.XferCnt[num] =  pcore->host.hc[num].xfer_len - hctsiz.b.xfersize;
    }

    pcore->host.HC_Status[num] = HC_XFRC;
    pcore->host.ErrCnt [num]= 0;
    CLEAR_HC_INT(hcreg , xfercompl);

    if ((hcchar.b.eptype == EP_TYPE_CTRL)||
        (hcchar.b.eptype == EP_TYPE_BULK))
    {
      UNMASK_HOST_INT_CHH (num);
      USB_OTG_HC_Halt(pcore, num);
      CLEAR_HC_INT(hcreg , nak);
      pcore->host.hc[num].toggle_in ^= 1;

    }
    else if(hcchar.b.eptype == EP_TYPE_INTR)
    {
      hcchar.b.oddfrm  = 1;
      USB_OTG_WRITE_REG32(&pcore->regs.HC_REGS[num]->HCCHAR, hcchar.d32);
      pcore->host.URB_State[num] = URB_DONE;
    }

  }
  else if (hcint.b.chhltd)
  {
    MASK_HOST_INT_CHH (num);

    if(pcore->host.HC_Status[num] == HC_XFRC)
    {
      pcore->host.URB_State[num] = URB_DONE;
    }

    else if (pcore->host.HC_Status[num] == HC_STALL)
    {
      pcore->host.URB_State[num] = URB_STALL;
    }

    else if((pcore->host.HC_Status[num] == HC_XACTERR) ||
            (pcore->host.HC_Status[num] == HC_DATATGLERR))
    {
      pcore->host.ErrCnt[num] = 0;
      pcore->host.URB_State[num] = URB_ERROR;

    }
    else if(hcchar.b.eptype == EP_TYPE_INTR)
    {
      pcore->host.hc[num].toggle_in ^= 1;
    }

    CLEAR_HC_INT(hcreg , chhltd);

  }
  else if (hcint.b.xacterr)
  {
    UNMASK_HOST_INT_CHH (num);
    pcore->host.ErrCnt[num] ++;
    pcore->host.HC_Status[num] = HC_XACTERR;
    USB_OTG_HC_Halt(pcore, num);
    CLEAR_HC_INT(hcreg , xacterr);

  }
  else if (hcint.b.nak)
  {
    if(hcchar.b.eptype == EP_TYPE_INTR)
    {
      UNMASK_HOST_INT_CHH (num);
      USB_OTG_HC_Halt(pcore, num);
    }
    else if  ((hcchar.b.eptype == EP_TYPE_CTRL)||
              (hcchar.b.eptype == EP_TYPE_BULK))
    {
      /* re-activate the channel  */
      hcchar.b.chen = 1;
      hcchar.b.chdis = 0;
      USB_OTG_WRITE_REG32(&pcore->regs.HC_REGS[num]->HCCHAR, hcchar.d32);
    }
    pcore->host.HC_Status[num] = HC_NAK;
    CLEAR_HC_INT(hcreg , nak);
  }


  return 1;

}

/**
* @brief  USB_OTG_USBH_handle_rx_qlvl_ISR
*         Handles the Rx Status Queue Level Interrupt
* @param  pcore: Selected Core Peripheral
* @retval status
*/
#if defined ( __ICCARM__ ) /*!< IAR Compiler */
#pragma optimize = none
#endif /* __CC_ARM */
static uint32_t USB_OTG_USBH_handle_rx_qlvl_ISR (USB_OTG_CORE_HANDLE *pcore)
{
  USB_OTG_GRXFSTS_TypeDef       grxsts;
  USB_OTG_GINTMSK_TypeDef       intmsk;
  USB_OTG_HCTSIZn_TypeDef       hctsiz;
  USB_OTG_HCCHAR_TypeDef        hcchar;
  __IO uint8_t                  channelnum =0;
  uint32_t                      count;

  /* Disable the Rx Status Queue Level interrupt */
  intmsk.d32 = 0;
  intmsk.b.rxstsqlvl = 1;
  USB_OTG_MODIFY_REG32( &pcore->regs.GREGS->GINTMSK, intmsk.d32, 0);

  grxsts.d32 = USB_OTG_READ_REG32(&pcore->regs.GREGS->GRXSTSP);
  channelnum = grxsts.b.chnum;
  hcchar.d32 = USB_OTG_READ_REG32(&pcore->regs.HC_REGS[channelnum]->HCCHAR);

  switch (grxsts.b.pktsts)
  {
  case GRXSTS_PKTSTS_IN:
    /* Read the data into the host buffer. */
    if ((grxsts.b.bcnt > 0) && (pcore->host.hc[channelnum].xfer_buff != (void  *)0))
    {

      USB_OTG_ReadPacket(pcore, pcore->host.hc[channelnum].xfer_buff, grxsts.b.bcnt);
      /*manage multiple Xfer */
      pcore->host.hc[grxsts.b.chnum].xfer_buff += grxsts.b.bcnt;
      pcore->host.hc[grxsts.b.chnum].xfer_count  += grxsts.b.bcnt;


      count = pcore->host.hc[channelnum].xfer_count;
      pcore->host.XferCnt[channelnum]  = count;

      hctsiz.d32 = USB_OTG_READ_REG32(&pcore->regs.HC_REGS[channelnum]->HCTSIZ);
      if(hctsiz.b.pktcnt > 0)
      {
        /* re-activate the channel when more packets are expected */
        hcchar.b.chen = 1;
        hcchar.b.chdis = 0;
        USB_OTG_WRITE_REG32(&pcore->regs.HC_REGS[channelnum]->HCCHAR, hcchar.d32);
      }
    }
    break;

  case GRXSTS_PKTSTS_IN_XFER_COMP:

  case GRXSTS_PKTSTS_DATA_TOGGLE_ERR:
  case GRXSTS_PKTSTS_CH_HALTED:
  default:
    break;
  }

  /* Enable the Rx Status Queue Level interrupt */
  intmsk.b.rxstsqlvl = 1;
  USB_OTG_MODIFY_REG32(&pcore->regs.GREGS->GINTMSK, 0, intmsk.d32);
  return 1;
}

/**
* @brief  USB_OTG_USBH_handle_IncompletePeriodicXfer_ISR
*         Handles the incomplete Periodic transfer Interrupt
* @param  pcore: Selected Core Peripheral
* @retval status
*/
#if defined ( __ICCARM__ ) /*!< IAR Compiler */
#pragma optimize = none
#endif /* __CC_ARM */
static uint32_t USB_OTG_USBH_handle_IncompletePeriodicXfer_ISR (USB_OTG_CORE_HANDLE *pcore)
{

  USB_OTG_GINTSTS_TypeDef       gintsts;
  USB_OTG_HCCHAR_TypeDef        hcchar;




  hcchar.d32 = USB_OTG_READ_REG32(&pcore->regs.HC_REGS[0]->HCCHAR);
  hcchar.b.chen = 1;
  hcchar.b.chdis = 1;
  USB_OTG_WRITE_REG32(&pcore->regs.HC_REGS[0]->HCCHAR, hcchar.d32);

  gintsts.d32 = 0;
  /* Clear interrupt */
  gintsts.b.incomplisoout = 1;
  USB_OTG_WRITE_REG32(&pcore->regs.GREGS->GINTSTS, gintsts.d32);

  return 1;
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

