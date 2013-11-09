/**
  ******************************************************************************
  * @file    usbh_core.h
  * @author  MCD Application Team
  * @version V2.1.0
  * @date    19-March-2012
  * @brief   Header file for usbh_core.c
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

/* Define to prevent recursive  ----------------------------------------------*/
#ifndef __USBH_CORE_H
#define __USBH_CORE_H

/* Includes ------------------------------------------------------------------*/
#include "usb_hcd.h"
#include "usbh_def.h"
#include "usbh_conf.h"

/** @addtogroup USBH_LIB
  * @{
  */

/** @addtogroup USBH_LIB_CORE
* @{
*/

/** @defgroup USBH_CORE
  * @brief This file is the Header file for usbh_core.c
  * @{
  */


/** @defgroup USBH_CORE_Exported_Defines
  * @{
  */

#define MSC_CLASS                         0x08
#define HID_CLASS                         0x03
#define MSC_PROTOCOL                      0x50
#define CBI_PROTOCOL                      0x01


#define USBH_MAX_ERROR_COUNT                            2
#define USBH_DEVICE_ADDRESS_DEFAULT                     0
#define USBH_DEVICE_ADDRESS                             1


/**
  * @}
  */


/** @defgroup USBH_CORE_Exported_Types
  * @{
  */

typedef enum {
  USBH_OK   = 0,
  USBH_BUSY,
  USBH_STALL,
  USBH_FAIL,
  USBH_NOT_SUPPORTED,
  USBH_UNRECOVERED_ERROR,
  USBH_ERROR_SPEED_UNKNOWN,
  USBH_APPLY_DEINIT
} USBH_Status;

/* Following states are used for gState */
typedef enum {
  HOST_IDLE = 0,
  HOST_DEV_RESET_PENDING,
  HOST_DEV_DELAY,
  HOST_DEV_ATTACHED,
  HOST_DEV_DISCONNECTED,
  HOST_DETECT_DEVICE_SPEED,
  HOST_ENUMERATION,
  HOST_ENUMERATION_DELAY,
  HOST_DO_TASK,
  HOST_CTRL_XFER,
  HOST_SUSPENDED,
  HOST_ERROR_STATE
} HOST_State;

/* Following states are used for EnumerationState */
typedef enum {
  ENUM_IDLE = 0,
  ENUM_GET_FULL_DEV_DESC,
  ENUM_GET_FULL_DEV_DESC_WAIT,
  ENUM_SET_ADDR,
  ENUM_GET_CFG_DESC,
  ENUM_GET_FULL_CFG_DESC,
  ENUM_GET_MFC_STRING_DESC,
  ENUM_GET_PRODUCT_STRING_DESC,
  ENUM_GET_SERIALNUM_STRING_DESC,
  ENUM_SET_CONFIGURATION,
  ENUM_DEV_CONFIGURED
} ENUM_State;



/* Following states are used for CtrlXferStateMachine */
typedef enum {
  CTRL_IDLE =0,
  CTRL_SETUP,
  CTRL_SETUP_WAIT,
  CTRL_DATA_IN,
  CTRL_DATA_IN_WAIT,
  CTRL_DATA_OUT,
  CTRL_DATA_OUT_WAIT,
  CTRL_STATUS_IN,
  CTRL_STATUS_IN_WAIT,
  CTRL_STATUS_OUT,
  CTRL_STATUS_OUT_WAIT,
  CTRL_ERROR,
  CTRL_STALLED,
  CTRL_COMPLETE
}
CTRL_State;

typedef enum {
  USBH_USR_NO_RESP   = 0,
  USBH_USR_RESP_OK = 1,
}
USBH_USR_Status;

/* Following states are used for RequestState */
typedef enum {
  CMD_IDLE =0,
  CMD_SEND,
  CMD_WAIT
} CMD_State;



typedef struct _Ctrl
{
  uint8_t               hc_num_in;
  uint8_t               hc_num_out;
  uint8_t               ep0size;
  uint8_t               *buff;
  uint16_t              length;
  uint8_t               errorcount;
  uint16_t              timer;
  uint16_t              timeout;
  CTRL_STATUS           status;
  USB_Setup_TypeDef     setup;
  CTRL_State            state;

} USBH_Ctrl_TypeDef;



typedef struct _DeviceProp
{

  uint8_t                           address;
  uint8_t                           speed;
  USBH_DevDesc_TypeDef              Dev_Desc;
  USBH_CfgDesc_TypeDef              Cfg_Desc;
  USBH_InterfaceDesc_TypeDef        Itf_Desc[USBH_MAX_NUM_INTERFACES];
  USBH_EpDesc_TypeDef               Ep_Desc[USBH_MAX_NUM_INTERFACES][USBH_MAX_NUM_ENDPOINTS];
  USBH_HIDDesc_TypeDef              HID_Desc;

} USBH_Device_TypeDef;



typedef struct _USBH_USR_PROP
{


}
USBH_Usr_cb_TypeDef;

typedef struct _Host_TypeDef
{
  HOST_State            gState;       /*  Host State Machine Value */
  HOST_State            gStateBkp;    /* backup of previous State machine value */
  ENUM_State            EnumState;    /* Enumeration state Machine */
  CMD_State             RequestState;
  USBH_Ctrl_TypeDef     Control;

  USBH_Device_TypeDef   device_prop;

  uint8_t               port_num;

  void* cb;

  void* Usr_Data;
  void* Parent;
} USBH_DEV, *pUSBH_HOST;

typedef struct _USBH_Device_cb
{
  void         (*DeInitDev)\
    (USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev);
  USBH_Status  (*Task)\
    (USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev);

  void (*Init)(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev);       /* HostLibInitialized */
  void (*DeInit)(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev);       /* HostLibInitialized */
  void (*DeviceAttached)(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev);           /* DeviceAttached */
  void (*ResetDevice)(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev);
  void (*DeviceDisconnected)(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev);
  void (*OverCurrentDetected)(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev);
  void (*DeviceSpeedDetected)(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev, uint8_t DeviceSpeed);          /* DeviceSpeed */
  void (*DeviceDescAvailable)(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev, void *);    /* DeviceDescriptor is available */
  void (*DeviceAddressAssigned)(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev);  /* Address is assigned to USB Device */
  void (*ConfigurationDescAvailable)(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev,
                                     USBH_CfgDesc_TypeDef *,
                                     USBH_InterfaceDesc_TypeDef *,
                                     USBH_EpDesc_TypeDef **);
  /* Configuration Descriptor available */
  void (*ManufacturerString)(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev, void *);     /* ManufacturerString*/
  void (*ProductString)(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev, void *);          /* ProductString*/
  void (*SerialNumString)(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev, void *);        /* SerialNubString*/
  void (*EnumerationDone)(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev);           /* Enumeration finished */
  void (*DeviceNotSupported)(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev); /* Device is not supported*/
  void (*UnrecoveredError)(USB_OTG_CORE_HANDLE *pcore , USBH_DEV* pdev);

} USBH_Device_cb_TypeDef;

/**
  * @}
  */



/** @defgroup USBH_CORE_Exported_Macros
  * @{
  */

/**
  * @}
  */

/** @defgroup USBH_CORE_Exported_Variables
  * @{
  */

/**
  * @}
  */

/** @defgroup USBH_CORE_Exported_FunctionsPrototype
  * @{
  */
void USBH_InitCore(USB_OTG_CORE_HANDLE *pcore, USB_OTG_CORE_ID_TypeDef coreID);
void USBH_InitDev(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, USBH_Device_cb_TypeDef *cb);

USBH_Status USBH_DeInit(USB_OTG_CORE_HANDLE *pcore,
                        USBH_DEV *pdev);
void USBH_Process(USB_OTG_CORE_HANDLE *pcore ,
                  USBH_DEV *pdev);
void USBH_ErrorHandle(USB_OTG_CORE_HANDLE *pcore, USBH_DEV *pdev, USBH_Status errType);

/**
  * @}
  */

#endif /* __USBH_CORE_H */
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



