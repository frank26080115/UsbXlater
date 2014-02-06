#include "cereal.h"
#include "led.h"
#include "vcp.h"
#include "utilities.h"
#include "crc.h"
#include "flashfile.h"
#include "btstack/btproxy.h"
#include "btstack/hci_cereal.h"
#include <kbm2c/kbm2ctrl.h>
#include <stdlib.h>
#include <string.h>
#include <cmsis/swo.h>
#include <cmsis/core_cmFunc.h>
#include <cmsis/core_cmx.h>
#include <stm32fx/system_stm32fxxx.h>
#include <stm32fx/stm32fxxx.h>
#include <stm32fx/misc.h>
#include <stm32fx/stm32fxxx_it.h>
#include <stm32fx/peripherals.h>
#include <usbotg_lib/usb_core.h>
#include <usbotg_lib/usb_bsp.h>
#include <usbh_lib/usbh_core.h>
#include <usbh_lib/usbh_hcs.h>
#include <usbd_lib/usbd_core.h>
#include <usbh_dev/usbh_dev_manager.h>
#include <usbh_dev/usbh_dev_inc_all.h>
#include <usbd_dev/usbd_dev_inc_all.h>
#include "usb_passthrough.h"

USB_OTG_CORE_HANDLE			USB_OTG_Core_dev;
USB_OTG_CORE_HANDLE			USB_OTG_Core_host;
USBH_DEV					USBH_Dev;
volatile uint32_t			systick_1ms_cnt;
volatile uint32_t			delay_1ms_cnt;

uint8_t cmdHandler();
void run_passthrough();
void run_bootload();

int main(void)
{
	SysTick_Config(SystemCoreClock / 1000); // every millisecond
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
	__enable_irq();

	led_init();

	//cereal_init(CEREAL_BAUD_RATE);
	swo_init();

	// this step determines the default output method of debug
	// and it selects the active debug level
	dbgmode = DBGMODE_SWO
				| DBGMODE_ERR
				| DBGMODE_TRACE
				| DBGMODE_INFO
				| DBGMODE_DEBUG
				;

	//cereal_printf("\r\n\r\nhello world\r\n");
	swo_printf("\r\n\r\nUsbXlater, UID %08X\r\n", DBGMCU->IDCODE);
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
	swo_printf("Free RAM: %d\r\n", freeRam());

	crc_init();

	/*
	// TODO, this is a simplified loop without fancy functionality, remove when testing is done
	btproxy_init_RN42HCI();
	while (1)
	{
		btproxy_task();
	}
	//*/

	flashfile_init();
	//flashfile_sectorDump();

	led_1_on(); delay_ms(150); led_1_off();
	led_2_on(); delay_ms(150); led_2_off();
	led_3_on(); delay_ms(150); led_3_off();
	led_4_on(); delay_ms(150); led_4_off();

	kbm2c_init();

	USBD_ExPullUp_Idle();

	USB_Hub_Hardware_Init();
	USB_Hub_Hardware_Reset();

	//dbgwdg_init();

	USB_OTG_BSP_Init(0);

	USBH_Dev_AddrManager_Reset();
	USBH_InitCore(&USB_OTG_Core_host, USB_OTG_FS_CORE_ID);
	USBH_DeAllocate_AllChannel(&USB_OTG_Core_host);
	USBH_Dev.Parent = 0;
	USBH_InitDev(&USB_OTG_Core_host, &USBH_Dev, &USBH_Dev_CB_Default);

	USBD_Init(&USB_OTG_Core_dev, USB_OTG_HS_CORE_ID, &USBD_Dev_DS4_cb);
	DCD_DevDisconnect(&USB_OTG_Core_dev);

	USB_OTG_BSP_EnableInterrupt(0);

	delay_1ms_cnt = 200;
	while (delay_1ms_cnt > 100) dbgwdg_feed(); // force disconnection and reconnection
	DCD_DevConnect(&USB_OTG_Core_dev);
	while (delay_1ms_cnt > 0) dbgwdg_feed();

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
		DISTATE_CTRLER_PS4,
		DISTATE_VCP,
		DISTATE_PASSTHROUGH,
	} dev_intf_state;

	host_intf_state = HISTATE_NONE;
	dev_intf_state = DISTATE_NONE;

	// TODO, this is a simplified loop without fancy functionality, remove when testing is done
	while (1)
	{
		dbgwdg_feed();

		USBH_Process(&USB_OTG_Core_host, &USBH_Dev);
	}

	while (1)
	{
		dbgwdg_feed();

		USBH_Process(&USB_OTG_Core_host, &USBH_Dev);

		if (host_intf_state == HISTATE_NONE && USBH_Dev.gState == HOST_DO_TASK) {
			host_intf_state = HISTATE_READY;
		}

		if (dev_intf_state == DISTATE_NONE && USB_OTG_Core_dev.dev.device_status == USB_OTG_CONFIGURED) {
			dev_intf_state = DISTATE_CONNECTED;
		}

		if (dev_intf_state == DISTATE_CONNECTED && USBD_Host_Is_PS3 != 0) {
			dbg_printf(DBGMODE_TRACE, "Entering PS3 CTRLER Mode\r\n");
			dev_intf_state = DISTATE_CTRLER_PS3;
		}

		if (dev_intf_state == DISTATE_CONNECTED && USBD_Host_Is_PS4 != 0) {
			dbg_printf(DBGMODE_TRACE, "Entering PS4 CTRLER Mode\r\n");
			dev_intf_state = DISTATE_CTRLER_PS4;
		}

		if (systick_1ms_cnt > 999000 && USBD_Host_Is_PS3 == 0 && USBD_Host_Is_PS4 == 0 && (dev_intf_state != DISTATE_VCP && dev_intf_state != DISTATE_PASSTHROUGH))
		{
			dev_intf_state = DISTATE_VCP;

			// force a disconnection
			DCD_DevDisconnect(&USB_OTG_Core_dev);

			// setup new handler callbacks
			USBD_DeInit(&USB_OTG_Core_dev);
			USBD_Init(&USB_OTG_Core_dev, USB_OTG_HS_CORE_ID, &USBD_Dev_CDC_cb);
			DCD_DevDisconnect(&USB_OTG_Core_dev);

			// disconnection delay, super long because Windows might not re-enumerate if it is too short
			uint32_t t = systick_1ms_cnt;
			while ((systick_1ms_cnt - t) < 200) ;

			// force a reconnection to reenumerate as CDC
			DCD_DevConnect(&USB_OTG_Core_dev);

			dbg_printf(DBGMODE_TRACE, "Entering VCP Mode\r\n");
		}

		uint8_t ret = cmdHandler();
		if (ret == 1) {
			run_passthrough();
		}
		else if (ret == 2) {
			run_bootload();
		}
		else if (ret == 3) {
			NVIC_SystemReset();
		}

		if (host_intf_state == HISTATE_NONE && dev_intf_state == DISTATE_NONE && systick_1ms_cnt > 5000)
		{
			// we'll assume the user wants to go into the raw bootloader if nothing is happening
			// the device interface is the power source, so this is extremely rare in
			// normal usage situations
			run_bootload();
		}

		kbm2c_task(0);

		if (host_intf_state == HISTATE_READY && (dev_intf_state == DISTATE_CTRLER_PS3 || dev_intf_state == DISTATE_CTRLER_PS4)) {
			// this is the main operating mode
			// we go to a tighter loop for better performance
			dbg_printf(DBGMODE_TRACE, "Entering High Performance Controller Mode Loop \r\n");
			break;
		}
	}

	// this loop is much tighter
	while (1)
	{
		dbgwdg_feed();
		USBH_Process(&USB_OTG_Core_host, &USBH_Dev);
		kbm2c_task(0);
	}

	return 0;
}

void SysTick_Handler(void)
{
	if (delay_1ms_cnt > 0) {
		delay_1ms_cnt--;
	}

	systick_1ms_cnt++;

	//if (hal_tick_handler != 0) hal_tick_handler();
}

void run_passthrough()
{
	dbg_printf(DBGMODE_TRACE, "\r\n Entering Pass-Through Mode\r\n");

	// force a disconnection
	HCD_ResetPort(&USB_OTG_Core_host);
	USBH_DeInit(&USB_OTG_Core_host, &USBH_Dev);
	USBH_DeAllocate_AllChannel(&USB_OTG_Core_host);
	DCD_DevDisconnect(&USB_OTG_Core_dev);
	USBD_DeInit(&USB_OTG_Core_dev);

	// force full reset of both cores
	USB_OTG_CoreReset(&USB_OTG_Core_host);
	USB_OTG_CoreReset(&USB_OTG_Core_dev);

	// small disconnection delay
	delay_1ms_cnt = 200;
	while (delay_1ms_cnt > 0) dbgwdg_feed();

	USBPT_Init(&USBH_Dev);

	while (1) {
		dbgwdg_feed();
		USBPT_Work();
	}
}

void run_bootload()
{
	dbg_printf(DBGMODE_TRACE, "\r\n Entering Bootloader Mode\r\n");

	// small delay for trace message
	delay_1ms_cnt = 200;
	while (delay_1ms_cnt > 0) dbgwdg_feed();

	dbgwdg_stop();

	jump_to_bootloader();
}

// return 0 to signal "nothing to handle", return a code if the caller should handle something
uint8_t cmdHandler()
{
	static char cmdBuff[32];
	static int cmdBuffIdx = 0;

	int16_t c = vcp_rx();
	if (c < 0) {
		c = cereal_rx();
	}

	if (c >= 0)
	{
		if (c == '\r' || c == '\n' || c == '\0')
		{
			if (strcmp(cmdBuff, "passthrough") == 0)
			{
				return 1;
			}
			else if (strcmp(cmdBuff, "bootload") == 0)
			{
				return 2;
			}
			else if (strcmp(cmdBuff, "reset") == 0)
			{
				return 3;
			}
			else
			{
				dbgwdg_feed();
				vcp_printf("\r\nunknown command: \"%s\"\r\n", cmdBuff);
				cereal_printf("\r\nunknown command: \"%s\"\r\n", cmdBuff);
			}

			cmdBuffIdx = 0;
			return 0;
		}
		else
		{
			if (cmdBuffIdx < 32)
			{
				cmdBuff[cmdBuffIdx] = c;
				cmdBuffIdx++;
				cmdBuff[cmdBuffIdx] = 0;
			}
		}
	}

	return 0;
}

// an external pull-up resistor is implemented because soft-disconnect doesn't work too well

void USBD_ExPullUp_Idle()
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
	GPIO_InitTypeDef gi;
	GPIO_StructInit(&gi);
	gi.GPIO_Mode = GPIO_Mode_IN;
	gi.GPIO_Pin = GPIO_Pin_5;
	gi.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOC, &gi);
}

void USBD_ExPullUp_On()
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
	GPIO_InitTypeDef gi;
	GPIO_StructInit(&gi);
	gi.GPIO_Mode = GPIO_Mode_OUT;
	gi.GPIO_Pin = GPIO_Pin_5;
	GPIO_Init(GPIOC, &gi);
	GPIO_SetBits(GPIOC, GPIO_Pin_5);
}

void USBD_ExPullUp_Off()
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
	GPIO_InitTypeDef gi;
	GPIO_StructInit(&gi);
	gi.GPIO_Mode = GPIO_Mode_OUT;
	gi.GPIO_Pin = GPIO_Pin_5;
	GPIO_Init(GPIOC, &gi);
	GPIO_ResetBits(GPIOC, GPIO_Pin_5);
}
