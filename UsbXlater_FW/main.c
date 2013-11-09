#include "cereal.h"
#include "led.h"
#include "vcp.h"
#include "kbm2ctrl.h"
#include "utilities.h"
#include <cmsis/swo.h>
#include <cmsis/core_cmFunc.h>
#include <stm32f2/system_stm32f2xx.h>
#include <stm32f2/stm32f2xx.h>
#include <stm32f2/misc.h>
#include <stm32f2/stm32fxxx_it.h>
#include <stm32f2/stm32f2xx_rcc.h>
#include <stm32f2/stm32f2xx_gpio.h>
#include <usbotg_lib/usb_core.h>
#include <usbotg_lib/usb_bsp.h>
#include <usbh_lib/usbh_core.h>
#include <usbd_lib/usbd_core.h>
#include <usbh_dev/usbh_dev_manager.h>
#include <usbh_dev/usbh_dev_inc_all.h>
#include <usbd_dev/usbd_dev_inc_all.h>

USB_OTG_CORE_HANDLE			USB_OTG_Core_dev;
USB_OTG_CORE_HANDLE			USB_OTG_Core_host;
USBH_DEV					USBH_Dev;
static volatile uint16_t	hub_found_timer;
static volatile uint16_t	hid_found_timer;
static volatile char		send_ds3_report;

int main(void)
{
	SysTick_Config(SystemCoreClock / 1000); // every millisecond
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
	__enable_irq();

	led_init();

	cereal_init();
	swo_init();

	dbgmode = DBGMODE_SWO
				| DBGMODE_ERR
				| DBGMODE_TRACE
				| DBGMODE_INFO
				| DBGMODE_DEBUG
				;

	cereal_printf("hello world\r\n");
	swo_printf("Compiled on " __DATE__ ", " __TIME__", ");
	#ifdef __GNUC__
	swo_printf("GNU C %d", __GNUC__);
	#ifdef __GNUC_MINOR__
	swo_printf(".%d", __GNUC_MINOR__);
	#ifdef __GNUC_PATCHLEVEL__
	swo_printf(".%d", __GNUC_PATCHLEVEL__);
	#endif
	#endif
	swo_printf("\r\n");
	#else
	swo_printf("unknown compiler\r\n");
	#endif
	#ifndef __FUNCTION__
	#define __FUNCTION__
	#endif

	led_1_on(); delay_ms(200); led_1_off();
	led_2_on(); delay_ms(200); led_2_off();
	led_3_on(); delay_ms(200); led_3_off();
	led_4_on(); delay_ms(200); led_4_off();

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
	GPIO_InitTypeDef gi;

	// this is the pin connected to the device side pull-up resistor
	// we do not need it, so we set it into input mode
	GPIO_StructInit(&gi);
	//*
	gi.GPIO_Mode = GPIO_Mode_IN;
	gi.GPIO_Pin = GPIO_Pin_5;
	gi.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOC, &gi);
	//*/
	/*
	gi.GPIO_Mode = GPIO_Mode_OUT;
	gi.GPIO_Pin = GPIO_Pin_5;
	GPIO_Init(GPIOC, &gi);
	GPIO_SetBits(GPIOC, GPIO_Pin_5);
	*/

	USB_Hub_Hardware_Init();
	USB_Hub_Hardware_Reset();
	USBH_Dev_AddrManager_Reset();
	hub_found_timer = 0;
	USB_OTG_BSP_Init(0);
	USBH_InitCore(&USB_OTG_Core_host, USB_OTG_FS_CORE_ID);
	USBH_Dev.Parent = 0;
	USBH_InitDev(&USB_OTG_Core_host, &USBH_Dev, &USBH_Dev_CB_Default);
	USBD_Init(&USB_OTG_Core_dev, USB_OTG_HS_CORE_ID, &USBD_Dev_DS3_cb);
	USB_OTG_BSP_EnableInterrupt(0);
	hid_found_timer = 1;
	USBH_Dev_Has_HID = 0;
	USBD_CDC_IsReady = 0;

	while (1)
	{
		USBH_Process(&USB_OTG_Core_host, &USBH_Dev);

		if (hub_found_timer > 2 && (USBH_Dev.gState == HOST_IDLE || USBH_Dev.gState == HOST_ERROR_STATE)) {
			USB_Hub_Hardware_Reset();
			hub_found_timer = 0;
			//dbg_printf(DBGMODE_ERR, "\r\n No hub found, performing hub HW reset... \r\n");
		}

		if (hid_found_timer > 50 && USBH_Dev_Has_HID == 0) {
			// no devices found in 5 seconds, then use VCP mode
			//dbg_printf(DBGMODE_TRACE, "\r\n No Connected HID Devices, Starting VCP Mode \r\n");
			//break;
		}

		if (send_ds3_report != 0)
		{
			send_ds3_report = 0;
			kbm2c_report(&USB_OTG_Core_dev);
		}
	}

	GPIO_ResetBits(GPIOC, GPIO_Pin_5);
	DCD_DevDisconnect(&USB_OTG_Core_dev);
	USBD_DeInit(&USB_OTG_Core_dev);
	hid_found_timer = 1;
	while (hid_found_timer < 5) USBH_Process(&USB_OTG_Core_host, &USBH_Dev);
	GPIO_SetBits(GPIOC, GPIO_Pin_5);
	USBD_Init(&USB_OTG_Core_dev, USB_OTG_HS_CORE_ID, &USBD_Dev_CDC_cb);
	DCD_DevConnect(&USB_OTG_Core_dev);
	while (vcp_isReady() == 0)
	{
		USBH_Process(&USB_OTG_Core_host, &USBH_Dev);
	}
	vcp_printf("\r\n hello world \r\n");
	while (vcp_peek() < 0)
	{
		USBH_Process(&USB_OTG_Core_host, &USBH_Dev);
	}
	vcp_printf("\r\n welcome \r\n");
	while (1)
	{
		USBH_Process(&USB_OTG_Core_host, &USBH_Dev);
	}

	jump_to_bootloader();

	return 0;
}

volatile uint8_t	systick_100ms_cnt;
volatile uint32_t	delay_1ms_cnt;

void SysTick_Handler(void)
{
	if (delay_1ms_cnt > 0) {
		delay_1ms_cnt--;
	}

	if ((systick_100ms_cnt % 10) == 0) {
		send_ds3_report = 1;
	}

	if (systick_100ms_cnt >= 100) {
		led_100ms_event();
		systick_100ms_cnt = 0;
		hub_found_timer++;
		if (hid_found_timer > 0 && hid_found_timer < 255) hid_found_timer++;
	}
	else {
		systick_100ms_cnt++;
	}

	if (USBH_Dev_Reset_Timer > 0 && USBH_Dev_Reset_Timer < 255) {
		USBH_Dev_Reset_Timer++;
	}

	if (kbm2c_mouseTimeSince > 0) {
		kbm2c_mouseTimeSince--;
	}
	else {
		kbm2c_mouseDidNotMove();
	}
}

