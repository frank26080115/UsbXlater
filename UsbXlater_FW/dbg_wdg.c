#include "dbg_wdg.h"
#include <utilities.h>
#include <stdint.h>
#include <misc.h>
#include <stm32fx/stm32fxxx.h>
#include <stm32fx/stm32fxxx_it.h>
#include <stm32fx/peripherals.h>

#define DBGWDG_TIMx_IRQn TIM4_IRQn
#define DBGWDG_RCC_APB1Periph_TIMx RCC_APB1Periph_TIM4

void dbgwdg_init()
{
	RCC_APB1PeriphClockCmd(DBGWDG_RCC_APB1Periph_TIMx, ENABLE);

	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	TIM_TimeBaseStructure.TIM_Period = 0xFFFFFF;
	TIM_TimeBaseStructure.TIM_Prescaler = 2000; // adjust this, it is in milliseconds
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(DBGWDG_TIMx, &TIM_TimeBaseStructure);
	DBGWDG_TIMx->CNT = 1;
	DBGWDG_TIMx->SR = 0;

	TIM_Cmd(DBGWDG_TIMx, ENABLE);

	// wait for one interrupt to arrive first, then clear it
	// this is because TIM_TimeBaseInit forces at least one interrupt
	while (DBGWDG_TIMx->SR == 0);
	DBGWDG_TIMx->CNT = 1;
	DBGWDG_TIMx->SR = 0;

	// now allow the vector to actually execute
	TIM_ITConfig(DBGWDG_TIMx, TIM_IT_Update, ENABLE);
	NVIC_InitTypeDef ni;
	ni.NVIC_IRQChannel = DBGWDG_TIMx_IRQn;
	ni.NVIC_IRQChannelPreemptionPriority = 0;
	ni.NVIC_IRQChannelSubPriority = 1;
	ni.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&ni);
}

void dbgwdg_stop()
{
	TIM_Cmd(DBGWDG_TIMx, DISABLE);
	TIM_ITConfig(DBGWDG_TIMx, TIM_IT_Update, DISABLE);
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
	dbg_printf(DBGMODE_ERR, "\r\n WDG Handler (%d)\r\n", systick_1ms_cnt);
	dbg_printf(DBGMODE_ERR, "r0: 0x%08X, r1: 0x%08X, r2: 0x%08X, r3: 0x%08X,", r0, r1, r2, r3);
	dbg_printf(DBGMODE_ERR, " r12: 0x%08X\r\nLR: 0x%08X, PC: 0x%08X, PSR: 0x%08X, \r\n", r12, lr, pc, psr);
	volatile int cntdn = 0x7FFFFF;
	while (cntdn--) __NOP();
	NVIC_SystemReset();
	while (1);
}

void TIM4_IRQHandler(void) __attribute__( ( naked ) );
void TIM4_IRQHandler()
{
	//DBGWDG_TIMx->SR &= ~(DBGWDG_TIMx->SR);
	//led_3_tog();
	//return;
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
