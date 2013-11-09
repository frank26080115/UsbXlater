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
static volatile char		send_ds3_report;
volatile uint32_t			systick_1ms_cnt;
volatile uint32_t			delay_1ms_cnt;

void USBD_PullUp_Idle();
void USBD_PullUp_On();
void USBD_PullUp_Off();

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

	USBD_PullUp_Idle();

	USB_Hub_Hardware_Init();
	USB_Hub_Hardware_Reset();

	USB_OTG_BSP_Init(0);

	USBH_Dev_AddrManager_Reset();
	USBH_InitCore(&USB_OTG_Core_host, USB_OTG_FS_CORE_ID);
	USBH_Dev.Parent = 0;
	USBH_InitDev(&USB_OTG_Core_host, &USBH_Dev, &USBH_Dev_CB_Default);

	USBD_Init(&USB_OTG_Core_dev, USB_OTG_HS_CORE_ID, &USBD_Dev_DS3_cb);
	DCD_DevDisconnect(&USB_OTG_Core_dev);

	USB_OTG_BSP_EnableInterrupt(0);

	while (systick_1ms_cnt < 100) ; // force disconnection and reconnection
	DCD_DevConnect(&USB_OTG_Core_dev);
	while (systick_1ms_cnt < 100) ;

	USBH_Dev_Has_HID = 0;
	USBD_CDC_IsReady = 0;

	enum host_intf_state_ {
		HISTATE_NONE,
		HISTATE_READY,
		HISTATE_PASSTHROUGH,
	} host_intf_state;

	enum dev_intf_state_ {
		DISTATE_NONE,
		DISTATE_CONNECTED,
		DISTATE_CTRLER_PS3,
		DISTATE_VCP,
		DISTATE_PASSTHROUGH,
	} dev_intf_state;

	host_intf_state = HISTATE_NONE;
	dev_intf_state = DISTATE_NONE;

	while (1)
	{
		USBH_Process(&USB_OTG_Core_host, &USBH_Dev);

		if (host_intf_state == HISTATE_NONE && USBH_Dev.gState == HOST_DO_TASK) {
			host_intf_state = HISTATE_READY;
		}

		if (dev_intf_state == DISTATE_NONE && USB_OTG_Core_dev.dev.device_status == USB_OTG_CONFIGURED) {
			dev_intf_state = DISTATE_CONNECTED;
		}

		if (dev_intf_state == DISTATE_CONNECTED && USBD_Host_Is_PS3 != 0) {
			// TODO: handle PS4 and Xbox
			dbg_printf(DBGMODE_DEBUG, "\r\n Entering CTRLER Mode \r\n");
			dev_intf_state = DISTATE_CTRLER_PS3;
		}

		if (systick_1ms_cnt > 5000 && USBD_Host_Is_PS3 == 0 && (dev_intf_state != DISTATE_VCP && dev_intf_state != DISTATE_PASSTHROUGH)) {
			dev_intf_state = DISTATE_VCP;

			// force a disconnection
			USBD_PullUp_Off();
			DCD_DevDisconnect(&USB_OTG_Core_dev);

			// setup new handler callbacks
			USBD_DeInit(&USB_OTG_Core_dev);
			USBD_Init(&USB_OTG_Core_dev, USB_OTG_HS_CORE_ID, &USBD_Dev_CDC_cb);
			DCD_DevDisconnect(&USB_OTG_Core_dev);

			// disconnection delay, super long because Windows might not re-enumerate if it is too short
			delay_1ms_cnt = 200;
			while (delay_1ms_cnt > 0) {
				USBH_Process(&USB_OTG_Core_host, &USBH_Dev);
			}

			// force a reconnection to reenumerate as CDC
			USBD_PullUp_On();
			DCD_DevConnect(&USB_OTG_Core_dev);

			dbg_printf(DBGMODE_TRACE, "\r\n Entering VCP Mode \r\n");
		}

		/*
		if (vcp_isReady())
		{
			int16_t c = vcp_rx();
			if (c >= 0) {
				vcp_tx(c);
			}
		}
		//*/

		if (host_intf_state == HISTATE_READY && dev_intf_state == DISTATE_NONE && systick_1ms_cnt > 5000)
		{
			host_intf_state = HISTATE_PASSTHROUGH;
			dev_intf_state = DISTATE_PASSTHROUGH;

			dbg_printf(DBGMODE_TRACE, "\r\n Entering Pass-Through Mode \r\n");

			// force a disconnection
			HCD_ResetPort(&USB_OTG_Core_host);
			USBH_DeInit(&USB_OTG_Core_host, &USBH_Dev);
			USBD_PullUp_Off();
			DCD_DevDisconnect(&USB_OTG_Core_dev);
			USBD_DeInit(&USB_OTG_Core_dev);

			// small disconnection delay
			delay_1ms_cnt = 200;
			while (delay_1ms_cnt > 0) USBH_Process(&USB_OTG_Core_host, &USBH_Dev);

			USBPT_Init(&USBH_Dev);
		}

		if (host_intf_state == HISTATE_PASSTHROUGH && dev_intf_state == DISTATE_PASSTHROUGH) {
			USBPT_Work();
			// there is no way to exit out of pass-through mode
		}

		if (host_intf_state == HISTATE_NONE && dev_intf_state == DISTATE_NONE && systick_1ms_cnt > 5000)
		{
			// we'll assume the user wants to go into the raw bootloader if nothing is happening
			// the device interface is the power source, so this is extremely rare in
			// normal usage situations
			dbg_printf(DBGMODE_TRACE, "\r\n Entering Bootloader Mode \r\n");

			// small delay for trace message
			delay_1ms_cnt = 200;
			while (delay_1ms_cnt > 0) ;

			jump_to_bootloader();
		}

		if (send_ds3_report != 0)
		{
			send_ds3_report = 0;
			if (dev_intf_state == DISTATE_CTRLER_PS3) {
				kbm2c_report(&USB_OTG_Core_dev);
			}
			else {
				// TODO: this happens every 10ms, what should we do here?
			}
		}

		if (host_intf_state == HISTATE_READY && dev_intf_state == DISTATE_CTRLER_PS3) {
			// this is the main operating mode
			// we go to a tighter loop for better performance
			dbg_printf(DBGMODE_DEBUG, "\r\n Entering High Performance Loop \r\n");
			break;
		}
	}

	// this loop is much tighter
	while (1)
	{
		USBH_Process(&USB_OTG_Core_host, &USBH_Dev);
		if (send_ds3_report != 0)
		{
			send_ds3_report = 0;
			kbm2c_report(&USB_OTG_Core_dev);
		}
	}

	return 0;
}

void SysTick_Handler(void)
{
	if (delay_1ms_cnt > 0) {
		delay_1ms_cnt--;
	}

	if ((systick_1ms_cnt % 10) == 0) {
		send_ds3_report = 1;
	}

	if ((systick_1ms_cnt % 100) == 0) {
		led_100ms_event();
	}

	systick_1ms_cnt++;
}

// an external pull-up resistor is implemented because soft-disconnect doesn't work too well

void USBD_PullUp_Idle()
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
	GPIO_InitTypeDef gi;
	GPIO_StructInit(&gi);
	gi.GPIO_Mode = GPIO_Mode_IN;
	gi.GPIO_Pin = GPIO_Pin_5;
	gi.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOC, &gi);
}

void USBD_PullUp_On()
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
	GPIO_InitTypeDef gi;
	GPIO_StructInit(&gi);
	gi.GPIO_Mode = GPIO_Mode_OUT;
	gi.GPIO_Pin = GPIO_Pin_5;
	GPIO_Init(GPIOC, &gi);
	GPIO_SetBits(GPIOC, GPIO_Pin_5);
}

void USBD_PullUp_Off()
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
	GPIO_InitTypeDef gi;
	GPIO_StructInit(&gi);
	gi.GPIO_Mode = GPIO_Mode_OUT;
	gi.GPIO_Pin = GPIO_Pin_5;
	GPIO_Init(GPIOC, &gi);
	GPIO_ResetBits(GPIOC, GPIO_Pin_5);
}
