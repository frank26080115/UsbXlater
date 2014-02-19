#include "utilities.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stm32fx/peripherals.h>
#include <cmsis/core_cmFunc.h>

uint8_t dbgmode = 0; // stores runtime debug level
uint8_t global_temp_buff[GLOBAL_TEMP_BUFF_SIZE]; // this buffer is shared between many modules, do not use within interrupts!
volatile uint32_t dbg_cnt = 0;
volatile void* dbg_obj = 0;
volatile size_t stack_at_main;

#define BOOTLOADER_ADDR 0x1FFF0000
#define BOOTLOADER_STACK 0x20001000
typedef void (*pFunction)(void);
static volatile pFunction bl_jmp_func;
/*
 * use this to activate the system boot mode bootloader of the STM
 */
void jump_to_bootloader(void)
{
	RCC_DeInit();
	SysTick->CTRL = 0;
	SysTick->LOAD = 0;
	SysTick->VAL = 0;
	RCC_SYSCLKConfig(RCC_SYSCLKSource_HSI);
	__set_PRIMASK(1); // disables interrupt
	__set_MSP(BOOTLOADER_STACK);
	bl_jmp_func = (void(*)(void))(*((uint32_t*)(BOOTLOADER_ADDR + 4)));
	bl_jmp_func();
	while(1);
}

int current_stack_depth() {
	volatile int obj = 123;
	volatile size_t cur_stack = (size_t)&obj;
	return stack_at_main - cur_stack;
}

uint16_t fletcher16(uint8_t const * data, size_t bytes)
{
	uint16_t sum1 = 0xff, sum2 = 0xff;
	while (bytes)
	{
		size_t tlen = bytes > 20 ? 20 : bytes;
		bytes -= tlen;
		do {
			sum2 += sum1 += *data++;
		} while (--tlen);
		sum1 = (sum1 & 0xff) + (sum1 >> 8);
		sum2 = (sum2 & 0xff) + (sum2 >> 8);
	}
	sum1 = (sum1 & 0xff) + (sum1 >> 8);
	sum2 = (sum2 & 0xff) + (sum2 >> 8);
	return sum2 << 8 | sum1;
}

uint8_t* print_bdaddr(uint8_t* data)
{
	sprintf(&global_temp_buff[GLOBAL_TEMP_BUFF_SIZE - (3 * 6) - 2], "%02X:%02X:%02X:%02X:%02X:%02X", data[5], data[4], data[3], data[2], data[1], data[0]);
	return &global_temp_buff[GLOBAL_TEMP_BUFF_SIZE - (3 * 6) - 2];
}

uint8_t* print_linkkey(uint8_t* data)
{
	for (int i = 0; i < 16; i++) {
		sprintf(&global_temp_buff[(GLOBAL_TEMP_BUFF_SIZE / 2) + (i * 2)], "%02X", data[i]);
	}
	return &global_temp_buff[GLOBAL_TEMP_BUFF_SIZE / 2];
}

extern void* _sbrk(int);
int freeRam()
{
	// post #9 from http://forum.pjrc.com/threads/23256-Get-Free-Memory-for-Teensy-3-0
	uint32_t stackTop;
	uint32_t heapTop;

	// current position of the stack.
	stackTop = (uint32_t) &stackTop;

	// current position of heap.
	void* hTop = malloc(1);
	heapTop = (uint32_t) hTop;
	free(hTop);

	// The difference is the free, available ram.
	return (stackTop - heapTop);// - current_stack_depth();
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{
  dbg_printf(DBGMODE_ERR, "\r\nassert failed %s %d\r\n", file, line);
  dbgwdg_stop();
  while (1) { }
}
#endif
