#include <stm32fx/stm32fxxx.h>
#include <stm32fx/system_stm32fxxx.h>
#include <stm32fx/misc.h>
#include <stm32fx/peripherals.h>
#include <cmsis/core_cmx.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

void swo_init()
{
	uint32_t SWOSpeed
		//= 6000000; //6000kbps, default for JLinkSWOViewer
		= 2000000; //2000kbps, default for ST Link Utility
	uint32_t SWOPrescaler = (SystemCoreClock / SWOSpeed) - 1; // SWOSpeed in Hz
	CoreDebug->DEMCR = CoreDebug_DEMCR_TRCENA_Msk;
	DBGMCU->CR = DBGMCU_CR_TRACE_IOEN | DBGMCU_CR_DBG_STANDBY | DBGMCU_CR_DBG_STOP | DBGMCU_CR_DBG_SLEEP;
	*((volatile unsigned *)(ITM_BASE + 0x400F0)) = 0x00000002; // "Selected PIN Protocol Register": Select which protocol to use for trace output (2: SWO)
	*((volatile unsigned *)(ITM_BASE + 0x40010)) = SWOPrescaler; // "Async Clock Prescaler Register". Scale the baud rate of the asynchronous output
	*((volatile unsigned *)(ITM_BASE + 0x00FB0)) = 0xC5ACCE55; // ITM Lock Access Register, C5ACCE55 enables more write access to Control Register 0xE00 :: 0xFFC
	ITM->TCR = ITM_TCR_TraceBusID_Msk | ITM_TCR_TXENA_Msk | ITM_TCR_SYNCENA_Msk | ITM_TCR_ITMENA_Msk; // ITM Trace Control Register
	ITM->TPR = ITM_TPR_PRIVMASK_Msk; // ITM Trace Privilege Register
	ITM->TER = 0x00000001; // ITM Trace Enable Register. Enabled tracing on stimulus ports. One bit per stimulus port.
	*((volatile unsigned *)(ITM_BASE + 0x01000)) = 0x400003FE; // DWT_CTRL
	*((volatile unsigned *)(ITM_BASE + 0x40304)) = 0x00000100; // Formatter and Flush Control Register
}

// may not be needed
void swo_deinit()
{
	volatile uint32_t i = 0xFFF;
	while (ITM->PORT[0].u32 == 0 && i--); // wait for any pending transmission
	CoreDebug->DEMCR = 0;
	DBGMCU->CR = 0;
	*((volatile unsigned *)(ITM_BASE + 0x00FB0)) = 0xC5ACCE55; // ITM Lock Access Register, C5ACCE55 enables more write access to Control Register 0xE00 :: 0xFFC
	ITM->TCR = 0;
	ITM->TPR = 0;
	ITM->TER = 0;
}

// function mostly copied from ITM_SendChar
static inline void swo_sendchar(uint8_t x)
{
	if (
		(CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) &&    // if debugger is attached (does it really work?)
		(CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk)  &&      // Trace enabled
		(ITM->TCR & ITM_TCR_ITMENA_Msk)                  &&      // ITM enabled
		(ITM->TER & (1UL << 0))                                  // ITM Port #0 enabled
	)
	{
		volatile uint32_t i = 0xFFF; // implement a timeout
		while (ITM->PORT[0].u32 == 0 && i--); // wait for next
		if (ITM->PORT[0].u32 != 0 && i != 0) {
			ITM->PORT[0].u8 = (uint8_t) x; // send
		}
	}
}

void swo_printf(char * format, ...)
{
	int i, j;
	#ifndef va_list
	#define va_list __gnuc_va_list
	#endif

	va_list args;
	va_start(args, format);
	j = vsprintf(global_temp_buff, format, args);
	va_end(args);
	for (i = 0; i < j; i++) {
		uint8_t ch = global_temp_buff[i];
		swo_sendchar(ch);
	}
}
