#include "cereal.h"
#include "led.h"
#include "vcp.h"
#include "utilities.h"
#include <kbm2c/kbm2ctrl.h>
#include <stdlib.h>
#include <string.h>
#include <cmsis/swo.h>
#include <cmsis/core_cmFunc.h>
#include <cmsis/core_cm3.h>
#include <stm32f2/system_stm32f2xx.h>
#include <stm32f2/stm32f2xx.h>
#include <stm32f2/misc.h>
#include <stm32f2/stm32fxxx_it.h>
#include <stm32f2/stm32f2xx_rcc.h>
#include <stm32f2/stm32f2xx_gpio.h>
#include <usbotg_lib/usb_core.h>
#include <usbotg_lib/usb_bsp.h>
#include <usbh_lib/usbh_core.h>
#include <usbh_lib/usbh_hcs.h>
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
volatile uint32_t			run_cnt;

uint8_t cmdHandler();

int main(void)
{
	SysTick_Config(SystemCoreClock / 1000); // every millisecond
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
	__enable_irq();

	led_init();

	cereal_init();
	swo_init();

	// this step determines the default output method of debug
	// and it selects the active debug level
	dbgmode = DBGMODE_SWO
				| DBGMODE_ERR
				| DBGMODE_TRACE
				| DBGMODE_INFO
				| DBGMODE_DEBUG
				;

	cereal_printf("\r\n\r\nhello world\r\n");
	swo_printf("\r\n\r\nCompiled on " __DATE__ ", " __TIME__", ");
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

	kbm2c_init();

	USBD_ExPullUp_Idle();

	USB_Hub_Hardware_Init();
	USB_Hub_Hardware_Reset();

	USB_OTG_BSP_Init(0);

	USBH_Dev_AddrManager_Reset();
	USBH_InitCore(&USB_OTG_Core_host, USB_OTG_FS_CORE_ID);
	USBH_DeAllocate_AllChannel(&USB_OTG_Core_host);
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
		run_cnt++;

		USBH_Process(&USB_OTG_Core_host, &USBH_Dev);

		if (host_intf_state == HISTATE_NONE && USBH_Dev.gState == HOST_DO_TASK) {
			host_intf_state = HISTATE_READY;
		}

		if (dev_intf_state == DISTATE_NONE && USB_OTG_Core_dev.dev.device_status == USB_OTG_CONFIGURED) {
			dev_intf_state = DISTATE_CONNECTED;
		}

		if (dev_intf_state == DISTATE_CONNECTED && USBD_Host_Is_PS3 != 0) {
			// TODO: handle PS4 and Xbox
			dbg_printf(DBGMODE_TRACE, "Entering CTRLER Mode\r\n");
			dev_intf_state = DISTATE_CTRLER_PS3;
		}

		if (systick_1ms_cnt > 5000 && USBD_Host_Is_PS3 == 0 && (dev_intf_state != DISTATE_VCP && dev_intf_state != DISTATE_PASSTHROUGH)) {
			dev_intf_state = DISTATE_VCP;

			// force a disconnection
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

		if (host_intf_state == HISTATE_READY && dev_intf_state == DISTATE_CTRLER_PS3) {
			// this is the main operating mode
			// we go to a tighter loop for better performance
			dbg_printf(DBGMODE_TRACE, "Entering High Performance Controller Mode Loop \r\n");
			break;
		}
	}

	// this loop is much tighter
	while (1)
	{
		run_cnt++;

		USBH_Process(&USB_OTG_Core_host, &USBH_Dev);
		kbm2c_task(0);
	}

	return 0;
}

void prvGetRegistersFromStack2( uint32_t *pulFaultStackAddress )
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
	dbg_printf(DBGMODE_ERR, "\r\n Freeze Handler\r\n");
	dbg_printf(DBGMODE_ERR, "r0: 0x%08X, r1: 0x%08X, r2: 0x%08X, r3: 0x%08X,", r0, r1, r2, r3);
	dbg_printf(DBGMODE_ERR, " r12: 0x%08X\r\nLR: 0x%08X, PC: 0x%08X, PSR: 0x%08X, \r\n", r12, lr, pc, psr);
}

void SysTick_Handler(void)
{
	static uint32_t prevRunCnt;
	static uint32_t freezeCnt;
	if (systick_1ms_cnt > 5000)
	{
		if (prevRunCnt == 0) {
			prevRunCnt = run_cnt;
		}
		if (prevRunCnt == run_cnt)
		{
			freezeCnt++;
			if (freezeCnt > 100)
			{
				// possible freeze
				led_1_on();led_2_on();led_3_on();led_4_on();
				__asm volatile
				(
					" tst lr, #4                                                \n"
					" ite eq                                                    \n"
					" mrseq r0, msp                                             \n"
					" mrsne r0, psp                                             \n"
					" ldr r1, [r0, #24]                                         \n"
					" ldr r2, handler6_address_const                            \n"
					" bx r2                                                     \n"
					" handler6_address_const: .word prvGetRegistersFromStack2    \n"
				);
			}
		}
		else {
			prevRunCnt = run_cnt;
			freezeCnt = 0;
		}
	}

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
	while (delay_1ms_cnt > 0) ;

	USBPT_Init(&USBH_Dev);

	while (1) USBPT_Work();
}

void run_bootload()
{
	dbg_printf(DBGMODE_TRACE, "\r\n Entering Bootloader Mode\r\n");

	// small delay for trace message
	delay_1ms_cnt = 200;
	while (delay_1ms_cnt > 0) ;

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
