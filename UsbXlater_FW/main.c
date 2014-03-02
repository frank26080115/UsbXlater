#include "cereal.h"
#include "led.h"
#include "vcp.h"
#include "utilities.h"
#include "crc.h"
#include "flashfile.h"
#include "ds4_emu.h"
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
#include <usbh_dev/usbh_devio_manager.h>
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

	crc_init();

	swo_printf("\r\n\r\n%s\r\n", version_string());
	swo_printf("version CRC 0x%08X\r\n", version_crc());
	swo_printf("MCU-ID %08X\r\n", DBGMCU->IDCODE);
	uint32_t* uid;
	uid = (uint32_t*)(0x1FFF7A10);
	swo_printf("Unique-ID %08X%08X%08X\r\n", uid[0], uid[1], uid[2]);

	volatile int obj = 123;
	stack_at_main = (size_t)&obj;

	flashfile_init();
	//flashfile_sectorDump();

	led_1_on(); delay_ms(150); led_1_off();
	led_2_on(); delay_ms(150); led_2_off();
	led_3_on(); delay_ms(150); led_3_off();
	led_4_on(); delay_ms(150); led_4_off();

	memset(&USB_OTG_ISR_Statistics, 0, sizeof(USB_OTG_ISR_Statistics_t));

	kbm2c_init();
	ds4emu_init();
	ds4emu_loadState();

	USBD_ExPullUp_Idle();

	USB_Hub_Hardware_Init();
	USB_Hub_Hardware_Reset();

	//dbgwdg_init();

	USB_OTG_BSP_Init(0);

	USBH_DevIOManager_Init();
	USBH_Dev_AddrManager_Reset();
	USBH_InitCore(&USB_OTG_Core_host, USB_OTG_FS_CORE_ID);
	USBH_DeAllocate_AllChannel(&USB_OTG_Core_host);
	USBH_Dev.Parent = 0;
	USBH_InitDev(&USB_OTG_Core_host, &USBH_Dev, &USBH_Dev_CB_Default);

	USB_OTG_BSP_EnableInterrupt(0);

	USBH_Dev_HID_Cnt = 0;
	USBD_CDC_IsReady = 0;

	swo_printf("Free RAM: %d\r\n", freeRam());

	// TODO, this is a simplified loop without fancy functionality, remove when testing is done
	while (1)
	{
		dbgwdg_feed();

		USBH_Process(&USB_OTG_Core_host, &USBH_Dev);

		ds4emu_task();
	}

	return 0;
}

void SysTick_Handler(void)
{
	if (delay_1ms_cnt > 0) {
		delay_1ms_cnt--;
	}

	systick_1ms_cnt++;

	if ((systick_1ms_cnt % 2000) == 0)
	{
		dbg_printf(DBGMODE_DEBUG, "%d %d , ", systick_1ms_cnt, freeRam());
		dbg_printf(DBGMODE_DEBUG, "%d %d , ", proxy_stats.ds2bt_hidctrl_cnt, proxy_stats.bt2ps_hidctrl_cnt);
		dbg_printf(DBGMODE_DEBUG, "%d %d , ", proxy_stats.ds2bt_hidctrl_len, proxy_stats.bt2ps_hidctrl_len);
		dbg_printf(DBGMODE_DEBUG, "%d %d %d , ", proxy_stats.ds2bt_hidintr_cnt, proxy_stats.ds2bt_hidintr_highprior_cnt, proxy_stats.bt2ps_hidintr_cnt);
		dbg_printf(DBGMODE_DEBUG, "%d %d %d , ", proxy_stats.ds2bt_hidintr_len, proxy_stats.ds2bt_hidintr_highprior_len, proxy_stats.bt2ps_hidintr_len);
		dbg_printf(DBGMODE_DEBUG, "%d %d , ", proxy_stats.ps2bt_hidctrl_cnt, proxy_stats.bt2ds_hidctrl_cnt);
		dbg_printf(DBGMODE_DEBUG, "%d %d , ", proxy_stats.ps2bt_hidctrl_len, proxy_stats.bt2ds_hidctrl_len);
		dbg_printf(DBGMODE_DEBUG, "%d %d , ", proxy_stats.ps2bt_hidintr_cnt, proxy_stats.bt2ds_hidintr_cnt);
		dbg_printf(DBGMODE_DEBUG, "%d %d , ", proxy_stats.ps2bt_hidintr_len, proxy_stats.bt2ds_hidintr_len);
		dbg_printf(DBGMODE_DEBUG, "\r\n");
	}

	//if ((systick_1ms_cnt % 2000) == 0 && dbg_obj != 0) dbg_printf(DBGMODE_DEBUG, "sth %d %d\r\n", systick_1ms_cnt, ((USBH_DevIO_t*)dbg_obj)->state);
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
