/*
 * UsbXlater - by Frank Zhao
 *
 * Important: this file is included into every file using a command line option for GCC, not using #include
 * this makes it easy to add debug messages into files I didn't write
 *
 */

#ifndef UTILITIES_H_
#define UTILITIES_H_

#include <stdint.h>

#include <cereal.h>
#include <dbg_wdg.h>
#include <led.h>
#include <cmsis/swo.h>
#include <cmsis/core_cmInstr.h>

#define DBGMODE_ALL 0xFF
#define DBGMODE_CER 0x01
#define DBGMODE_SWO 0x02
#define DBGMODE_TRACE 0x04
#define DBGMODE_DEBUG 0x08
#define DBGMODE_ERR 0x10
#define DBGMODE_INFO 0x20
extern uint8_t dbgmode;
#define dbg_printf(m, fmt, args...)    do { if ((m & dbgmode) == 0) { break; } if ((dbgmode & DBGMODE_CER) != 0) { cereal_printf(fmt, ##args); } if ((dbgmode & DBGMODE_SWO) != 0) { swo_printf(fmt, ##args); } } while (0)
#define dbg_trace()                    do { dbg_printf(DBGMODE_SWO | DBGMODE_TRACE, "\r\ntrace: " __FILE__ " : %d\r\n", __LINE__ ); } while (0)
#define dbg_trace_exit()               do { dbg_printf(DBGMODE_SWO | DBGMODE_TRACE, "\r\ntrace: " __FILE__ " : %d (exit)\r\n", __LINE__ ); } while (0)

#define GLOBAL_TEMP_BUFF_SIZE 128
extern uint8_t global_temp_buff[GLOBAL_TEMP_BUFF_SIZE];

#define DELAY_LOOPS(x) do { uint32_t _____i = (x); while (--_____i) { asm volatile ("nop"); } } while (0)

static inline void delay_us(const uint32_t x);
static inline void delay_ms(const uint32_t x);

static inline void delay_us(const uint32_t x)
{
	volatile uint32_t count = 0;
	const uint32_t utime = (120 * x / 7);
	do
	{
		if ( ++count > utime )
		{
			return ;
		}
	}
	while (1);
}

void jump_to_bootloader(void);
uint16_t fletcher16(uint8_t const * data, size_t bytes);
uint32_t version_crc();
uint8_t* print_bdaddr(uint8_t*);
uint8_t* print_linkkey(uint8_t*);
int freeRam();

extern volatile uint32_t systick_1ms_cnt;
extern volatile uint32_t delay_1ms_cnt;
static inline void delay_ms(const uint32_t x)
{
	delay_1ms_cnt = x;
	while (delay_1ms_cnt > 0) dbgwdg_feed();
}

#ifdef  USE_FULL_ASSERT

/**
  * @brief  The assert_param macro is used for function's parameters check.
  * @param  expr: If expr is false, it calls assert_failed function
  *   which reports the name of the source file and the source
  *   line number of the call that failed.
  *   If expr is true, it returns no value.
  * @retval None
  */
  #define assert_param(expr) ((expr) ? (void)0 : assert_failed((uint8_t *)__FILE__, __LINE__))
/* Exported functions ------------------------------------------------------- */
  void assert_failed(uint8_t* file, uint32_t line);
#else
  #define assert_param(expr) ((void)0)
#endif /* USE_FULL_ASSERT */

#endif /* UTILITIES_H_ */
