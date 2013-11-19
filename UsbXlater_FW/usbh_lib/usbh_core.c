/**
  ******************************************************************************
  * @file    usbh_core.c
  * @author  MCD Application Team
  * @version V2.1.0
  * @date    19-March-2012
  * @brief   This file implements the functions for the core state machine process
  *          the enumeration and the control transfer process
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

#include "usbh_ioreq.h"
#include <usbotg_lib/usb_bsp.h>
#include "usbh_hcs.h"
#include "usbh_stdreq.h"
#include "usbh_core.h"
#include "usb_hcd_int.h"
#include <usbh_dev/usbh_dev_inc_all.h>
#include <usbh_dev/usbh_dev_manager.h>
#include <vcp.h>


/** @addtogroup USBH_LIB
  * @{
  */

/** @addtogroup USBH_LIB_CORE
* @{
*/

/** @defgroup USBH_CORE
  * @brief TThis file handles the basic enumaration when a device is connected
  *          to the host.
  * @{
  */

/** @defgroup USBH_CORE_Private_TypesDefinitions
  * @{
  */
uint8_t USBH_Disconnected (USB_OTG_CORE_HANDLE *pcore);
uint8_t USBH_Connected (USB_OTG_CORE_HANDLE *pcore);
uint8_t USBH_SOF (USB_OTG_CORE_HANDLE *pcore);

USBH_HCD_INT_cb_TypeDef USBH_HCD_INT_cb =
{
	USBH_SOF,
	USBH_Connected,
	USBH_Disconnected,
};

USBH_HCD_INT_cb_TypeDef  *USBH_HCD_INT_fops = &USBH_HCD_INT_cb;
/**
  * @}
  */


/** @defgroup USBH_CORE_Private_Defines
  * @{
  */
/**
  * @}
  */


/** @defgroup USBH_CORE_Private_Macros
  * @{
  */
/**
  * @}
  */

/** @defgroup USBH_CORE_Private_Variables
  * @{
  */
/**
  * @}
  */

/** @defgroup USBH_CORE_Private_FunctionPrototypes
  * @{
  */
static USBH_Status USBH_HandleEnum(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev);
USBH_Status USBH_HandleControl (USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev);

/**
  * @}
  */


/** @defgroup USBH_CORE_Private_Functions
  * @{
  */


/**
  * @brief  USBH_Connected
  *         USB Connect callback function from the Interrupt.
  * @param  selected device
  * @retval Status
*/
uint8_t USBH_Connected (USB_OTG_CORE_HANDLE *pcore)
{
	pcore->host.ConnSts = 1;
	return 0;
}

/**
* @brief  USBH_Disconnected
*         USB Disconnect callback function from the Interrupt.
* @param  selected device
* @retval Status
*/

uint8_t USBH_Disconnected (USB_OTG_CORE_HANDLE *pcore)
{
	pcore->host.ConnSts = 0;
	return 0;
}

/**
  * @brief  USBH_SOF
  *         USB SOF callback function from the Interrupt.
  * @param  selected device
  * @retval Status
  */

uint8_t USBH_SOF (USB_OTG_CORE_HANDLE *pcore)
{
	/* This callback could be used to implement a scheduler process */
	return 0;
}
/**
  * @brief  USBH_Init
  *         Host hardware and stack initializations
  * @retval None
  */
void USBH_InitCore(USB_OTG_CORE_HANDLE *pcore,
               USB_OTG_CORE_ID_TypeDef coreID
			   )
{

	/* Hardware Init */
	//USB_OTG_BSP_Init(pcore);

	/* configure GPIO pin used for switching VBUS power */
	USB_OTG_BSP_ConfigVBUS(0);

	/* Start the USB OTG core */
	 HCD_Init(pcore , coreID);

	/* Enable Interrupts */
	//USB_OTG_BSP_EnableInterrupt(pcore);
}

/**
  * @brief  USBH_Init
  *         Host hardware and stack initializations
  * @param  class_cb: Class callback structure address
  * @param  usr_cb: User callback structure address
  * @retval None
  */
void USBH_InitDev(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, USBH_Device_cb_TypeDef *cb)
{
	/* Host de-initializations */
	USBH_DeInit(pcore, pdev);
	/*Register class and user callbacks */
	pdev->cb = cb;
	/* Upon Init call usr call back */
	((USBH_Device_cb_TypeDef*)pdev->cb)->Init(pcore, pdev);
}

/**
  * @brief  USBH_DeInit
  *         Re-Initialize Host
  * @param  None
  * @retval status: USBH_Status
  */
USBH_Status USBH_DeInit(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev)
{
	/* Software Init */

	pdev->gState = HOST_IDLE;
	pdev->gStateBkp = HOST_IDLE;
	pdev->EnumState = ENUM_IDLE;
	pdev->RequestState = CMD_SEND;

	pdev->Control.state = CTRL_SETUP;
	pdev->Control.ep0size = USB_OTG_MAX_EP0_SIZE;

	USBH_Dev_FreeAddress(pdev->device_prop.address);

	pdev->device_prop.address = USBH_DEVICE_ADDRESS_DEFAULT;
	pdev->device_prop.speed = HPRT0_PRTSPD_FULL_SPEED;

	pdev->total_err = 0;

	USBH_Free_Channel(pcore, &(pdev->Control.hc_num_in));
	USBH_Free_Channel(pcore, &(pdev->Control.hc_num_out));

	return USBH_OK;
}

/**
* @brief  USBH_Process
*         USB Host core main state machine process
* @param  None
* @retval None
*/
void USBH_Process(USB_OTG_CORE_HANDLE *pcore , USBH_DEV *pdev)
{
	static uint8_t rootDisconnectedCnt;

	char isDevRoot = pdev->Parent == 0;

	volatile USBH_Status status = USBH_FAIL;

	/* check for Host port events */
	if ((HCD_IsDeviceConnected(pcore) == 0) && (pdev->gState != HOST_IDLE))
	{
		if(pdev->gState != HOST_DEV_DISCONNECTED)
		{
			rootDisconnectedCnt++;
			if (rootDisconnectedCnt > 5) {
				dbg_printf(DBGMODE_DEBUG, "Root device disconnected! \r\n");
				HCD_ResetPort(pcore);
				pdev->gState = HOST_DEV_DISCONNECTED;
			}
			else {
				delay_ms(1);
				return;
			}
		}
	}
	else
	{
		rootDisconnectedCnt = 0;
	}

	switch (pdev->gState)
	{

	case HOST_IDLE :
	if (isDevRoot)
	{
		if (HCD_IsDeviceConnected(pcore))
		{
			if (USBH_Dev_Reset_Timer == 0) {
				USBH_Dev_Reset_Timer = HCD_GetCurrentFrame(pcore);
			}
			else if ((HCD_GetCurrentFrame(pcore) - USBH_Dev_Reset_Timer) > 150) {
				pdev->gState = HOST_DEV_ATTACHED;
				dbg_printf(DBGMODE_TRACE, "USBH new device attached HOST_DEV_ATTACHED \r\n");
			}
		}
		else
		{
			USBH_Dev_Reset_Timer = 0;
		}
	}
		break;
	case HOST_DEV_RESET_PENDING :
		// does not apply to root hubs
		// once the reset is done, the hub should receive an interrupt-in packet indicating, which is where the new state HOST_DEV_DELAY is set
		break;
	case HOST_DEV_DELAY :
		if ((HCD_GetCurrentFrame(pcore) - USBH_Dev_Reset_Timer) > 150) {
			pdev->gState = HOST_DEV_ATTACHED;
		}
		break;
	case HOST_DEV_ATTACHED :

		if (USBH_Open_Channel(	pcore,
							&(pdev->Control.hc_num_in),
							0x80,
							pdev->device_prop.address, // still 0 at this point
							pdev->device_prop.speed,
							EP_TYPE_CTRL,
							pdev->Control.ep0size) != HC_OK) {
			dbg_printf(DBGMODE_ERR, "Unable to open control-in-EP\r\n");
			break;
		}
		if (USBH_Open_Channel(	pcore,
							&(pdev->Control.hc_num_out),
							0x00,
							pdev->device_prop.address, // still 0 at this point
							pdev->device_prop.speed,
							EP_TYPE_CTRL,
							pdev->Control.ep0size) != HC_OK) {
			dbg_printf(DBGMODE_ERR, "Unable to open control-out-EP\r\n");
			break;
		}
		((USBH_Device_cb_TypeDef*)pdev->cb)->DeviceAttached(pcore, pdev);

		/* Reset USB Device */
		if (isDevRoot)
		{
			HCD_ResetPort(pcore);

			((USBH_Device_cb_TypeDef*)pdev->cb)->ResetDevice(pcore, pdev);
				/*	Wait for USB USBH_ISR_PrtEnDisableChange()
				Host is Now ready to start the Enumeration
				*/
			pdev->device_prop.speed = HCD_GetCurrentSpeed(pcore);
			((USBH_Device_cb_TypeDef*)pdev->cb)->DeviceSpeedDetected(pcore, pdev, pdev->device_prop.speed);

			pdev->gState = HOST_ENUMERATION;
		}
		else
		{
			pdev->gState = HOST_ENUMERATION_DELAY;
			USBH_Dev_Reset_Timer = HCD_GetCurrentFrame(pcore);
		}
		break;

	case HOST_ENUMERATION_DELAY:
		if ((HCD_GetCurrentFrame(pcore) - USBH_Dev_Reset_Timer) > 150) {
			pdev->gState = HOST_ENUMERATION;
		}
	break;

	case HOST_ENUMERATION:
		/* Check for enumeration status */
		if ( USBH_HandleEnum(pcore , pdev) == USBH_OK)
		{
			/* The function shall return USBH_OK when full enumeration is complete */

			/* user callback for end of device basic enumeration */
			((USBH_Device_cb_TypeDef*)pdev->cb)->EnumerationDone(pcore, pdev);

			pdev->gState = HOST_DO_TASK;

			dbg_printf(DBGMODE_TRACE, "Dev %d now entering DO_TASK \r\n", pdev->device_prop.address);
		}
		break;

	case HOST_DO_TASK:
		status = ((USBH_Device_cb_TypeDef*)pdev->cb)->Task(pcore, pdev);

		if (status != USBH_OK)
		{
			USBH_ErrorHandle(pcore, pdev, status);
		}
		break;

	case HOST_CTRL_XFER:
		/* process control transfer state machine */
		USBH_HandleControl(pcore, pdev);
		break;

	case HOST_SUSPENDED:
		break;

	case HOST_ERROR_STATE:
		/* Re-Initilaize Host for new Enumeration */
		((USBH_Device_cb_TypeDef*)pdev->cb)->DeInit(pcore, pdev);
		((USBH_Device_cb_TypeDef*)pdev->cb)->DeInitDev(pcore, pdev);
		USBH_DeInit(pcore, pdev);
		break;

	case HOST_DEV_DISCONNECTED :

	// only applies to root device, because memory needs to be freed for child devices

		/* Manage User disconnect operations*/
		if (pdev->cb != 0)
		{
			if (((USBH_Device_cb_TypeDef*)pdev->cb)->DeviceDisconnected != 0) {
				((USBH_Device_cb_TypeDef*)pdev->cb)->DeviceDisconnected(pcore, pdev);
			}
		}

		/* Re-Initilaize Host for new Enumeration */
		USBH_DeInit(pcore, pdev);

		if (pdev->cb != 0)
		{
			if (((USBH_Device_cb_TypeDef*)pdev->cb)->DeInit != 0) {
				((USBH_Device_cb_TypeDef*)pdev->cb)->DeInit(pcore, pdev);
			}
			if (((USBH_Device_cb_TypeDef*)pdev->cb)->DeInitDev != 0) {
				((USBH_Device_cb_TypeDef*)pdev->cb)->DeInitDev(pcore, pdev);
			}
		}

		if (isDevRoot) {
			USBH_DeAllocate_AllChannel(pcore);
			USBH_Dev_Reset_Timer = 0;
			pdev->cb = &USBH_Dev_CB_Default;
		}
		pdev->gState = HOST_IDLE;

		break;

	default :
		break;
	}

}


/**
  * @brief  USBH_ErrorHandle
  *         This function handles the Error on Host side.
  * @param  errType : Type of Error or Busy/OK state
  * @retval None
  */
void USBH_ErrorHandle(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, USBH_Status errType)
{
	dbg_printf(DBGMODE_ERR, "USBH_ErrorHandle 0x%02X\r\n", errType);

	/* Error unrecovered or not supported device speed */
	if ( (errType == USBH_ERROR_SPEED_UNKNOWN) ||
			 (errType == USBH_UNRECOVERED_ERROR) )
	{
		((USBH_Device_cb_TypeDef*)pdev->cb)->UnrecoveredError(pcore, pdev);
		pdev->gState = HOST_ERROR_STATE;
	}
	/* USB host restart requested from application layer */
	else if(errType == USBH_APPLY_DEINIT)
	{
		pdev->gState = HOST_ERROR_STATE;
		/* user callback for initalization */
		((USBH_Device_cb_TypeDef*)pdev->cb)->Init(pcore, pdev);
	}
}


/**
  * @brief  USBH_HandleEnum
  *         This function includes the complete enumeration process
  * @param  pcore: Selected Core Peripheral
  * @retval USBH_Status
  */
static USBH_Status USBH_HandleEnum(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev)
{
  char isDevRoot = pdev->Parent == 0;
  USBH_Status Status = USBH_BUSY;
  uint8_t Local_Buffer[64];

  switch (pdev->EnumState)
  {
  case ENUM_IDLE:
    /* Get Device Desc for only 1st 8 bytes : To get EP0 MaxPacketSize */
    if ( USBH_Get_DevDesc(pcore , pdev, 8) == USBH_OK)
    {
      pdev->Control.ep0size = pdev->device_prop.Dev_Desc.bMaxPacketSize;

      pdev->EnumState = ENUM_GET_FULL_DEV_DESC;
      if (isDevRoot != 0) {
        HCD_ResetPort(pcore);
        /* modify control channels configuration for MaxPacket size */
        USBH_Modify_Channel (pcore,
                             pdev->Control.hc_num_out,
                             0,
                             0,
                             0,
                             pdev->Control.ep0size);

        USBH_Modify_Channel (pcore,
                             pdev->Control.hc_num_in,
                             0,
                             0,
                             0,
                             pdev->Control.ep0size);
      }
      else {
        pdev->EnumState = ENUM_GET_FULL_DEV_DESC_WAIT;
        USBH_Dev_Reset_Timer = HCD_GetCurrentFrame(pcore);
      }
    }
    break;

  case ENUM_GET_FULL_DEV_DESC_WAIT:
    if ((HCD_GetCurrentFrame(pcore) - USBH_Dev_Reset_Timer) > 150) {
      USBH_Modify_Channel (pcore,
                           pdev->Control.hc_num_out,
                           0,
                           0,
                           0,
                           pdev->Control.ep0size);

      USBH_Modify_Channel (pcore,
                           pdev->Control.hc_num_in,
                           0,
                           0,
                           0,
                           pdev->Control.ep0size);
      pdev->EnumState = ENUM_GET_FULL_DEV_DESC;
    }
  break;

  case ENUM_GET_FULL_DEV_DESC:
    /* Get FULL Device Desc  */
    if ( USBH_Get_DevDesc(pcore, pdev, USB_DEVICE_DESC_SIZE) == USBH_OK)
    {
      /* user callback for device descriptor available */
      ((USBH_Device_cb_TypeDef*)pdev->cb)->DeviceDescAvailable(pcore, pdev, &pdev->device_prop.Dev_Desc);
      int8_t possibleAddr = USBH_Dev_GetAvailAddress(pcore, pdev);
      if (possibleAddr > 0) {
        pdev->EnumState = ENUM_SET_ADDR;
        pdev->device_prop.address = possibleAddr;
        vcp_printf("V%04XP%04XA%d:C%d:DevDesc:\r\n", pdev->device_prop.Dev_Desc.idVendor, pdev->device_prop.Dev_Desc.idProduct, pdev->device_prop.address, USB_DEVICE_DESC_SIZE);
        for (int k = 0; k < USB_DEVICE_DESC_SIZE; k++) {
          vcp_printf(" 0x%02X", pcore->host.Rx_Buffer[k]);
        }
        vcp_printf("\r\n");
      }
      else {
        dbg_printf(DBGMODE_ERR, "no available device addresses\r\n");
        return USBH_FAIL;
      }
    }
    break;

  case ENUM_SET_ADDR:
    /* set address */
    if ( USBH_SetAddress(pcore, pdev, pdev->device_prop.address) == USBH_OK)
    {
      USB_OTG_BSP_mDelay(2);

      /* user callback for device address assigned */
      ((USBH_Device_cb_TypeDef*)pdev->cb)->DeviceAddressAssigned(pcore, pdev);
      pdev->EnumState = ENUM_GET_CFG_DESC;

      /* modify control channels to update device address */
      USBH_Modify_Channel (pcore,
                           pdev->Control.hc_num_in,
                           pdev->device_prop.address,
                           0,
                           0,
                           0);

      USBH_Modify_Channel (pcore,
                           pdev->Control.hc_num_out,
                           pdev->device_prop.address,
                           0,
                           0,
                           0);

    }
    break;

  case ENUM_GET_CFG_DESC:
    /* get standard configuration descriptor */
    if ( USBH_Get_CfgDesc(pcore,
                          pdev,
                          USB_CONFIGURATION_DESC_SIZE) == USBH_OK)
    {
      pdev->EnumState = ENUM_GET_FULL_CFG_DESC;
    }
    break;

  case ENUM_GET_FULL_CFG_DESC:
    /* get FULL config descriptor (config, interface, endpoints) */
    if (USBH_Get_CfgDesc(pcore,
                         pdev,
                         pdev->device_prop.Cfg_Desc.wTotalLength) == USBH_OK)
    {
      /* User callback for configuration descriptors available */
      ((USBH_Device_cb_TypeDef*)pdev->cb)->ConfigurationDescAvailable(pcore, pdev, &pdev->device_prop.Cfg_Desc,
                                                                                    pdev->device_prop.Itf_Desc,
                                                                                    pdev->device_prop.Ep_Desc[0]);

      vcp_printf("V%04XP%04XA%d:C%d:ConfigDesc:\r\n", pdev->device_prop.Dev_Desc.idVendor, pdev->device_prop.Dev_Desc.idProduct, pdev->device_prop.address, pdev->device_prop.Cfg_Desc.wTotalLength);
		for (int k = 0; k < pdev->device_prop.Cfg_Desc.wTotalLength; k++) {
			vcp_printf(" 0x%02X", pcore->host.Rx_Buffer[k]);
		}
		vcp_printf("\r\n");

      pdev->EnumState = ENUM_GET_MFC_STRING_DESC; // commented out to skip
      //pdev->EnumState = ENUM_SET_CONFIGURATION;
    }
    break;

  case ENUM_GET_MFC_STRING_DESC:
    if (pdev->device_prop.Dev_Desc.iManufacturer != 0)
    { /* Check that Manufacturer String is available */

      if ( USBH_Get_StringDesc(pcore,
                               pdev,
                               pdev->device_prop.Dev_Desc.iManufacturer,
                               Local_Buffer ,
                               0xff) == USBH_OK)
      {
        /* User callback for Manufacturing string */
        ((USBH_Device_cb_TypeDef*)pdev->cb)->ManufacturerString(pcore, pdev, Local_Buffer);
        pdev->EnumState = ENUM_GET_PRODUCT_STRING_DESC;
      }
    }
    else
    {
      ((USBH_Device_cb_TypeDef*)pdev->cb)->ManufacturerString(pcore, pdev, "N/A");
      pdev->EnumState = ENUM_GET_PRODUCT_STRING_DESC;
    }
    break;

  case ENUM_GET_PRODUCT_STRING_DESC:
    if (pdev->device_prop.Dev_Desc.iProduct != 0)
    { /* Check that Product string is available */
      if ( USBH_Get_StringDesc(pcore,
                               pdev,
                               pdev->device_prop.Dev_Desc.iProduct,
                               Local_Buffer,
                               0xff) == USBH_OK)
      {
        /* User callback for Product string */
        ((USBH_Device_cb_TypeDef*)pdev->cb)->ProductString(pcore, pdev, Local_Buffer);
        pdev->EnumState = ENUM_GET_SERIALNUM_STRING_DESC;
      }
    }
    else
    {
      ((USBH_Device_cb_TypeDef*)pdev->cb)->ProductString(pcore, pdev, "N/A");
      pdev->EnumState = ENUM_GET_SERIALNUM_STRING_DESC;
    }
    break;

  case ENUM_GET_SERIALNUM_STRING_DESC:
    if (pdev->device_prop.Dev_Desc.iSerialNumber != 0)
    { /* Check that Serial number string is available */
      if ( USBH_Get_StringDesc(pcore,
                               pdev,
                               pdev->device_prop.Dev_Desc.iSerialNumber,
                               Local_Buffer,
                               0xff) == USBH_OK)
      {
        /* User callback for Serial number string */
        ((USBH_Device_cb_TypeDef*)pdev->cb)->SerialNumString(pcore, pdev, Local_Buffer);
        pdev->EnumState = ENUM_SET_CONFIGURATION;
      }
    }
    else
    {
      ((USBH_Device_cb_TypeDef*)pdev->cb)->SerialNumString(pcore, pdev, "N/A");
      pdev->EnumState = ENUM_SET_CONFIGURATION;
    }
    break;

  case ENUM_SET_CONFIGURATION:
    /* set configuration  (default config) */
    if (USBH_SetCfg(pcore,
                    pdev,
                    pdev->device_prop.Cfg_Desc.bConfigurationValue) == USBH_OK)
    {
      pdev->EnumState = ENUM_DEV_CONFIGURED;
    }
    break;


  case ENUM_DEV_CONFIGURED:
    /* user callback for enumeration done */
    Status = USBH_OK;
    dbg_printf(DBGMODE_TRACE, "ENUM_DEV_CONFIGURED Succeeded\r\n");
    break;

  default:
    break;
  }
  return Status;
}


/**
  * @brief  USBH_HandleControl
  *         Handles the USB control transfer state machine
  * @param  pcore: Selected Core Peripheral
  * @retval Status
  */
USBH_Status USBH_HandleControl (USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev)
{
  char isDevRoot = pdev->Parent == 0;
  uint8_t direction;
  USBH_Status status = USBH_BUSY;
  URB_STATE URB_Status = URB_IDLE;

  pdev->Control.status = CTRL_START;


  switch (pdev->Control.state)
  {
  case CTRL_SETUP:
    /* send a SETUP packet */
    USBH_CtlSendSetup (pcore,
	                   pdev->Control.setup.d8 ,
	                   pdev->Control.hc_num_out);
    pdev->Control.state = CTRL_SETUP_WAIT;
    pdev->Control.timer = HCD_GetCurrentFrame(pcore);
    pdev->Control.timeout = 50;
    break;

  case CTRL_SETUP_WAIT:
    URB_Status = HCD_GetURB_State(pcore , pdev->Control.hc_num_out);
    /* case SETUP packet sent successfully */
    if(URB_Status == URB_DONE)
    {
      direction = (pdev->Control.setup.b.bmRequestType & USB_REQ_DIR_MASK);

      /* check if there is a data stage */
      if (pdev->Control.setup.b.wLength.w != 0 )
      {
        pdev->Control.timeout = DATA_STAGE_TIMEOUT;
        if (direction == USB_D2H)
        {
          /* Data Direction is IN */
          pdev->Control.state = CTRL_DATA_IN;
        }
        else
        {
          /* Data Direction is OUT */
          pdev->Control.state = CTRL_DATA_OUT;
        }
      }
      /* No DATA stage */
      else
      {
        pdev->Control.timeout = NODATA_STAGE_TIMEOUT;

        /* If there is No Data Transfer Stage */
        if (direction == USB_D2H)
        {
          /* Data Direction is IN */
          pdev->Control.state = CTRL_STATUS_OUT;
        }
        else
        {
          /* Data Direction is OUT */
          pdev->Control.state = CTRL_STATUS_IN;
        }
      }
      /* Set the delay timer to enable timeout for data stage completion */
      pdev->Control.timer = HCD_GetCurrentFrame(pcore);
    }
    else if(URB_Status == URB_ERROR)
    {
      dbg_printf(DBGMODE_ERR, "USBH_HandleControl CTRL_SETUP_WAIT URB_Status == URB_ERROR \r\n");
      pdev->Control.state = CTRL_ERROR;
      pdev->Control.status = CTRL_XACTERR;
    }
    else if (URB_Status == URB_IDLE && (HCD_GetCurrentFrame(pcore) - pdev->Control.timer) > pdev->Control.timeout)
    {
      dbg_printf(DBGMODE_ERR, "USBH_HandleControl CTRL_SETUP_WAIT Timeout \r\n");
      pdev->Control.state = CTRL_SETUP; // retry
    }
    else if (URB_Status == URB_IDLE)
    {
      //dbg_printf(DBGMODE_TRACE, "URB_Status == URB_IDLE, %d \r\n", (HCD_GetCurrentFrame(pcore) - pdev->Control.timer));
    }
    break;

  case CTRL_DATA_IN:
    /* Issue an IN token */
    USBH_CtlReceiveData(pcore,
                        pdev->Control.buff,
                        pdev->Control.length,
                        pdev->Control.hc_num_in);

    pdev->Control.state = CTRL_DATA_IN_WAIT;
    break;

  case CTRL_DATA_IN_WAIT:

    URB_Status = HCD_GetURB_State(pcore , pdev->Control.hc_num_in);

    /* check is DATA packet transfered successfully */
    if  (URB_Status == URB_DONE)
    {
      pdev->Control.state = CTRL_STATUS_OUT;
    }

    /* manage error cases*/
    if  (URB_Status == URB_STALL)
    {
      pdev->gState =   pdev->gStateBkp; // In stall case, return to previous machine state
      dbg_printf(DBGMODE_ERR, "USBH_HandleControl CTRL_DATA_IN_WAIT URB_Status == URB_STALL \r\n");
      return USBH_STALL;
    }
    else if (URB_Status == URB_ERROR)
    {
      /* Device error */
      pdev->Control.state = CTRL_ERROR;
      dbg_printf(DBGMODE_ERR, "USBH_HandleControl CTRL_DATA_IN_WAIT URB_Status == URB_ERROR \r\n");
    }
    else if ((HCD_GetCurrentFrame(pcore)- pdev->Control.timer) > pdev->Control.timeout)
    {
      /* timeout for IN transfer */
      pdev->Control.state = CTRL_ERROR;
      dbg_printf(DBGMODE_ERR, "USBH_HandleControl CTRL_DATA_IN_WAIT Timeout \r\n");
    }
    break;

  case CTRL_DATA_OUT:
    /* Start DATA out transfer (only one DATA packet)*/
    pcore->host.hc[pdev->Control.hc_num_out].toggle_out = 1;

    USBH_CtlSendData (pcore,
                      pdev->Control.buff,
                      pdev->Control.length ,
                      pdev->Control.hc_num_out);





    pdev->Control.state = CTRL_DATA_OUT_WAIT;
    break;

  case CTRL_DATA_OUT_WAIT:

    URB_Status = HCD_GetURB_State(pcore , pdev->Control.hc_num_out);
    if  (URB_Status == URB_DONE)
    { /* If the Setup Pkt is sent successful, then change the state */
      pdev->Control.timer = HCD_GetCurrentFrame(pcore);
      pdev->Control.state = CTRL_STATUS_IN;
    }

    /* handle error cases */
    else if  (URB_Status == URB_STALL)
    {
      /* In stall case, return to previous machine state*/
      pdev->gState =   pdev->gStateBkp;
      pdev->Control.state = CTRL_STALLED;
      dbg_printf(DBGMODE_ERR, "USBH_HandleControl CTRL_DATA_OUT_WAIT URB_Status == URB_STALL \r\n");
      return USBH_STALL;
    }
    else if  (URB_Status == URB_NOTREADY)
    {
      /* Nack received from device */
      pdev->Control.state = CTRL_DATA_OUT;
      dbg_printf(DBGMODE_DEBUG, "USBH_HandleControl CTRL_DATA_OUT_WAIT URB_Status == URB_NOTREADY \r\n");
    }
    else if (URB_Status == URB_ERROR)
    {
      /* device error */
      pdev->Control.state = CTRL_ERROR;
      dbg_printf(DBGMODE_ERR, "USBH_HandleControl CTRL_DATA_OUT_WAIT URB_Status == URB_ERROR \r\n");
    }
    break;


  case CTRL_STATUS_IN:
    /* Send 0 bytes out packet */
    USBH_CtlReceiveData (pcore,
                         0,
                         0,
                         pdev->Control.hc_num_in);

    pdev->Control.state = CTRL_STATUS_IN_WAIT;

    break;

  case CTRL_STATUS_IN_WAIT:

    URB_Status = HCD_GetURB_State(pcore , pdev->Control.hc_num_in);

    if  ( URB_Status == URB_DONE)
    { /* Control transfers completed, Exit the State Machine */
      pdev->gState =   pdev->gStateBkp;
      pdev->Control.state = CTRL_COMPLETE;
      return USBH_OK;
    }

    else if (URB_Status == URB_ERROR)
    {
      pdev->Control.state = CTRL_ERROR;
      dbg_printf(DBGMODE_ERR, "USBH_HandleControl CTRL_STATUS_IN_WAIT URB_Status == URB_ERROR \r\n");
    }

    else if((HCD_GetCurrentFrame(pcore) - pdev->Control.timer) > pdev->Control.timeout)
    {
      pdev->Control.state = CTRL_ERROR;
      dbg_printf(DBGMODE_ERR, "USBH_HandleControl CTRL_STATUS_IN_WAIT Timeout \r\n");
    }
     else if(URB_Status == URB_STALL)
    {
      /* Control transfers completed, Exit the State Machine */
      pdev->gState =   pdev->gStateBkp;
      pdev->Control.status = CTRL_STALL;
      status = USBH_NOT_SUPPORTED;
      dbg_printf(DBGMODE_ERR, "USBH_HandleControl CTRL_STATUS_IN_WAIT URB_Status == URB_STALL \r\n");
    }
    break;

  case CTRL_STATUS_OUT:
    pcore->host.hc[pdev->Control.hc_num_out].toggle_out ^= 1;
    USBH_CtlSendData (pcore,
                      0,
                      0,
                      pdev->Control.hc_num_out);

    pdev->Control.state = CTRL_STATUS_OUT_WAIT;
    break;

  case CTRL_STATUS_OUT_WAIT:

    URB_Status = HCD_GetURB_State(pcore , pdev->Control.hc_num_out);
    if  (URB_Status == URB_DONE)
    {
      pdev->gState =   pdev->gStateBkp;
      pdev->Control.state = CTRL_COMPLETE;
      return USBH_OK;
    }
    else if  (URB_Status == URB_NOTREADY)
    {
      pdev->Control.state = CTRL_STATUS_OUT;
      dbg_printf(DBGMODE_ERR, "USBH_HandleControl CTRL_STATUS_OUT_WAIT URB_Status == URB_NOTREADY \r\n");
    }
    else if (URB_Status == URB_ERROR)
    {
      pdev->Control.state = CTRL_ERROR;
      dbg_printf(DBGMODE_ERR, "USBH_HandleControl CTRL_STATUS_OUT_WAIT URB_Status == URB_ERROR \r\n");
    }
    break;

  case CTRL_ERROR:
    /*
    After a halt condition is encountered or an error is detected by the
    host, a control endpoint is allowed to recover by accepting the next Setup
    PID; i.e., recovery actions via some other pipe are not required for control
    endpoints. For the Default Control Pipe, a device reset will ultimately be
    required to clear the halt or error condition if the next Setup PID is not
    accepted.
    */
    if (++ pdev->Control.errorcount <= USBH_MAX_ERROR_COUNT)
    {
      /* Do the transmission again, starting from SETUP Packet */
      pdev->Control.state = CTRL_SETUP;
    }
    else
    {
      pdev->Control.status = CTRL_FAIL;
      pdev->gState =   pdev->gStateBkp;

      status = USBH_FAIL;
      dbg_printf(DBGMODE_ERR, "USBH_HandleControl CTRL_ERROR Reached USBH_MAX_ERROR_COUNT \r\n");
      pdev->total_err++;
      if (pdev->total_err > 16) {
        pdev->gState = HOST_DEV_DISCONNECTED;
        dbg_printf(DBGMODE_ERR, "Too many errors for device, pretending to disconnect... \r\n");
      }
    }
    break;

  case CTRL_COMPLETE:
    status = USBH_OK;
    if (pdev->total_err > 0)
      pdev->total_err--;
    break;

  case CTRL_IDLE:
    status = USBH_OK;
    break;
  default:
    dbg_printf(DBGMODE_ERR, "USBH_HandleControl unknown pdev->Control.state = 0x%02X\r\n", pdev->Control.state);
    break;
  }
  return status;
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




