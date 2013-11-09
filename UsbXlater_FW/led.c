#include "led.h"

#include <stm32f2/Stm32f2xx.h>
#include <stm32f2/Stm32f2xx_gpio.h>
#include <stm32f2/Stm32f2xx_rcc.h>

volatile led_blink_pattern_t led_blink_pattern[4];

void led_init()
{
	for (int i = 0; i < 4; i++)
	{
		led_blink_pattern[i].on_time = 0;
		led_blink_pattern[i].off_time = 0;
		led_blink_pattern[i].on_time_remaining = 0;
		led_blink_pattern[i].off_time_remaining = 0;
		led_blink_pattern[i].reload = 0;
	}

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

	GPIO_InitTypeDef initer;
	GPIO_StructInit(&initer);
	initer.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9;
	initer.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_Init(GPIOC, &initer);

	for (int i = 1; i <= 4; i++)
	{
		led_off(i);
	}
}

void led_set_blink(uint8_t idx, unsigned int on_time, unsigned int off_time, void (*reload)(uint8_t i, void* pattern)) {
	idx--;
	led_blink_pattern[idx].on_time = on_time;
	led_blink_pattern[idx].off_time = off_time;
	led_blink_pattern[idx].reload = reload;
}
