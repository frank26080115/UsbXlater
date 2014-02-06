/**
  ******************************************************************************
  * @file    stm32fxxx_it.c
  * @author  MCD Application Team
  * @version V2.1.0
  * @date    19-March-2012
  * @brief   This file includes the interrupt handlers for the application
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

/*
 * UsbXlater - by Frank Zhao
 *
 * this file is how ST likes to organize interrupt handlers
 * the handlers do not have to be in this file, but some of the default ones are here
 *
 */

/* Includes ------------------------------------------------------------------*/
#include <usbotg_lib/usb_bsp.h>
#include <usbh_lib/usb_hcd_int.h>
#include <usbh_lib/usbh_core.h>
#include <cmsis/core_cmx.h>
#include "stm32fxxx_it.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

static volatile uint8_t fault_source;

/* Private function prototypes -----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

/******************************************************************************/
/*             Cortex-M Processor Exceptions Handlers                         */
/******************************************************************************/

/*
 * hardfault debugging assistant: http://www.freertos.org/Debugging-Hard-Faults-On-Cortex-M-Microcontrollers.html
 */

void prvGetRegistersFromStack( uint32_t *pulFaultStackAddress )
{
	/* These are volatile to try and prevent the compiler/linker optimising them
	away as the variables never actually get used.  If the debugger won't show the
	values of the variables, make them global my moving their declaration outside
	of this function. */
	volatile uint32_t r0;
	volatile uint32_t r1;
	volatile uint32_t r2;
	volatile uint32_t r3;
	volatile uint32_t r12;
	volatile uint32_t lr; /* Link register. */
	volatile uint32_t pc; /* Program counter. */
	volatile uint32_t psr;/* Program status register. */

	r0 = pulFaultStackAddress[ 0 ];
	r1 = pulFaultStackAddress[ 1 ];
	r2 = pulFaultStackAddress[ 2 ];
	r3 = pulFaultStackAddress[ 3 ];

	r12 = pulFaultStackAddress[ 4 ];
	lr  = pulFaultStackAddress[ 5 ];
	pc  = pulFaultStackAddress[ 6 ];
	psr = pulFaultStackAddress[ 7 ];

	/* When the following line is hit, the variables contain the register values. */
	dbg_printf(DBGMODE_ERR, "\r\n Exception Handler, source: %d\r\n", fault_source);
	dbg_printf(DBGMODE_ERR, "r0: 0x%08X, r1: 0x%08X, r2: 0x%08X, r3: 0x%08X,", r0, r1, r2, r3);
	dbg_printf(DBGMODE_ERR, " r12: 0x%08X\r\nLR: 0x%08X, PC: 0x%08X, PSR: 0x%08X, \r\n", r12, lr, pc, psr);
	for( ;; ) { led_1_on(); led_2_on(); led_3_on(); led_4_on(); };
}

/**
  * @brief  NMI_Handler
  *         This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
	dbg_printf(DBGMODE_DEBUG, "\r\n NMI_Handler, file " __FILE__ ":%d\r\n", __LINE__);
}

/**
  * @brief  HardFault_Handler
  *         This function handles Hard Fault exception.
  * @param  None
  * @retval None
  */
void HardFault_Handler(void)
{
  fault_source = 1;
  __asm volatile
  (
        " tst lr, #4                                                \n"
        " ite eq                                                    \n"
        " mrseq r0, msp                                             \n"
        " mrsne r0, psp                                             \n"
        " ldr r1, [r0, #24]                                         \n"
        " ldr r2, handler2_address_const                            \n"
        " bx r2                                                     \n"
        " handler2_address_const: .word prvGetRegistersFromStack    \n"
  );
  /* Go to infinite loop when Hard Fault exception occurs */
  while (1) { led_1_on(); led_2_on(); led_3_on(); led_4_on(); }
}

/**
  * @brief  MemManage_Handler
  *         This function handles Memory Manage exception.
  * @param  None
  * @retval None
  */
void MemManage_Handler(void)
{
  fault_source = 2;
  __asm volatile
  (
        " tst lr, #4                                                \n"
        " ite eq                                                    \n"
        " mrseq r0, msp                                             \n"
        " mrsne r0, psp                                             \n"
        " ldr r1, [r0, #24]                                         \n"
        " ldr r2, handler3_address_const                            \n"
        " bx r2                                                     \n"
        " handler3_address_const: .word prvGetRegistersFromStack    \n"
  );
  while (1) { led_1_on(); led_2_on(); led_3_on(); led_4_on(); }
}

/**
  * @brief  BusFault_Handler
  *         This function handles Bus Fault exception.
  * @param  None
  * @retval None
  */
void BusFault_Handler(void)
{
  fault_source = 3;
  __asm volatile
  (
        " tst lr, #4                                                \n"
        " ite eq                                                    \n"
        " mrseq r0, msp                                             \n"
        " mrsne r0, psp                                             \n"
        " ldr r1, [r0, #24]                                         \n"
        " ldr r2, handler4_address_const                            \n"
        " bx r2                                                     \n"
        " handler4_address_const: .word prvGetRegistersFromStack    \n"
  );
  while (1) { led_1_on(); led_2_on(); led_3_on(); led_4_on(); }
}

/**
  * @brief  UsageFault_Handler
  *         This function handles Usage Fault exception.
  * @param  None
  * @retval None
  */
void UsageFault_Handler(void)
{
  fault_source = 4;
  __asm volatile
  (
        " tst lr, #4                                                \n"
        " ite eq                                                    \n"
        " mrseq r0, msp                                             \n"
        " mrsne r0, psp                                             \n"
        " ldr r1, [r0, #24]                                         \n"
        " ldr r2, handler5_address_const                            \n"
        " bx r2                                                     \n"
        " handler5_address_const: .word prvGetRegistersFromStack    \n"
  );
  while (1) { led_1_on(); led_2_on(); led_3_on(); led_4_on(); }
}

/**
  * @brief  SVC_Handler
  *         This function handles SVCall exception.
  * @param  None
  * @retval None
  */
void SVC_Handler(void)
{
	dbg_printf(DBGMODE_ERR, "\r\n SVC_Handler, file " __FILE__ ":%d\r\n", __LINE__);
}

/**
  * @brief  DebugMon_Handler
  *         This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void)
{
	dbg_printf(DBGMODE_ERR, "\r\n DebugMon_Handler, file " __FILE__ ":%d\r\n", __LINE__);
}


/**
  * @brief  PendSV_Handler
  *         This function handles PendSVC exception.
  * @param  None
  * @retval None
  */
void PendSV_Handler(void)
{
	dbg_printf(DBGMODE_ERR, "\r\n PendSV_Handler, file " __FILE__ ":%d\r\n", __LINE__);
}

void Default_Handler_C(void)
{
	dbg_printf(DBGMODE_ERR, "\r\n Default_Handler_C, file " __FILE__ ":%d\r\n", __LINE__);
	char stopped = 0;
	for (int i = 0; i < 128; i++)
	{
		if (NVIC_GetPendingIRQ((IRQn_Type)i) != 0) {
			dbg_printf(DBGMODE_ERR, "IRQ %d is pending\r\n", i);
			NVIC_ClearPendingIRQ((IRQn_Type)i);
			stopped = 1;
		}
	}
	if (stopped == 0) {
		while (1);
	}
}

void UnknownVector_Handler(void)
{
	dbg_printf(DBGMODE_ERR, "\r\n UnknownVector_Handler, file " __FILE__ ":%d\r\n", __LINE__);
	char stopped = 0;
	for (int i = 0; i < 128; i++)
	{
		if (NVIC_GetPendingIRQ((IRQn_Type)i) != 0) {
			dbg_printf(DBGMODE_ERR, "IRQ %d is pending\r\n", i);
			NVIC_ClearPendingIRQ((IRQn_Type)i);
			stopped = 1;
		}
	}
	if (stopped == 0) {
		while (1);
	}
}

/******************************************************************************/
/*                 STM32Fxxx Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32fxxx.s).                                                */
/******************************************************************************/

#if 0

void WWDG_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void PVD_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void TAMP_STAMP_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void RTC_WKUP_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void FLASH_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void RCC_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void EXTI0_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void EXTI1_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void EXTI2_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void EXTI3_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void EXTI4_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void DMA1_Stream0_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void DMA1_Stream1_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void DMA1_Stream2_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void DMA1_Stream3_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void DMA1_Stream4_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void DMA1_Stream5_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void DMA1_Stream6_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void ADC_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void CAN1_TX_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void CAN1_RX0_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void CAN1_RX1_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void CAN1_SCE_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void EXTI9_5_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void TIM1_BRK_TIM9_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void TIM1_UP_TIM10_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void TIM1_TRG_COM_TIM11_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void TIM1_CC_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void TIM2_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void TIM3_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void I2C1_EV_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void I2C1_ER_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void I2C2_EV_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void I2C2_ER_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void SPI1_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void SPI2_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

/*
void USART2_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}
*/

void USART3_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void EXTI15_10_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void RTC_Alarm_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void OTG_FS_WKUP_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void TIM8_BRK_TIM12_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void TIM8_UP_TIM13_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void TIM8_TRG_COM_TIM14_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void TIM8_CC_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void DMA1_Stream7_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void FSMC_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void SDIO_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void TIM5_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void SPI3_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void UART4_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void UART5_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void TIM6_DAC_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void TIM7_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void DMA2_Stream0_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void DMA2_Stream1_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void DMA2_Stream2_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void DMA2_Stream3_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void DMA2_Stream4_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void ETH_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void ETH_WKUP_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void CAN2_TX_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void CAN2_RX0_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void CAN2_RX1_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void CAN2_SCE_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void DMA2_Stream5_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void DMA2_Stream6_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void DMA2_Stream7_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void USART6_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void I2C3_EV_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void I2C3_ER_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void OTG_HS_EP1_OUT_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void OTG_HS_EP1_IN_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void DCMI_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void CRYP_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void HASH_RNG_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

void FPU_IRQHandler()
{
	dbg_printf(DBGMODE_ERR, "\r\nUnhandled IRQ, file:" __FILE__ ", line: %d\r\n", __LINE__);
}

#endif

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
