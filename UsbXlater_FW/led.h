#ifndef LED_H_
#define LED_H_

#include <stm32fx/stm32fxxx.h>
#include <stdint.h>

typedef struct
{
	volatile unsigned int on_time;
	volatile unsigned int off_time;
	volatile unsigned int on_time_remaining;
	volatile unsigned int off_time_remaining;
	void (*reload)(uint8_t i, void* pattern);
}
led_blink_pattern_t;

extern volatile led_blink_pattern_t led_blink_pattern[4];

#define LED_1_GPIOx		GPIOC
#define LED_1_PIN		6
#define LED_2_GPIOx		GPIOC
#define LED_2_PIN		7
#define LED_3_GPIOx		GPIOC
#define LED_3_PIN		8
#define LED_4_GPIOx		GPIOC
#define LED_4_PIN		9

void led_init();

static inline void led_100ms_event();
void led_set_blink(uint8_t idx, unsigned int on_time, unsigned int off_time, void (*reload)(uint8_t i, void* pattern));

static inline void led_1_on() {
	LED_1_GPIOx->ODR |= (uint32_t)((uint32_t)1 << (uint32_t)LED_1_PIN);
}

static inline void led_2_on() {
	LED_2_GPIOx->ODR |= (uint32_t)((uint32_t)1 << (uint32_t)LED_2_PIN);
}

static inline void led_3_on() {
	LED_3_GPIOx->ODR |= (uint32_t)((uint32_t)1 << (uint32_t)LED_3_PIN);
}

static inline void led_4_on() {
	LED_4_GPIOx->ODR |= (uint32_t)((uint32_t)1 << (uint32_t)LED_4_PIN);
}

static inline void led_1_off() {
	LED_1_GPIOx->ODR &= ~(uint32_t)((uint32_t)1 << (uint32_t)LED_1_PIN);
}

static inline void led_2_off() {
	LED_2_GPIOx->ODR &= ~(uint32_t)((uint32_t)1 << (uint32_t)LED_2_PIN);
}

static inline void led_3_off() {
	LED_3_GPIOx->ODR &= ~(uint32_t)((uint32_t)1 << (uint32_t)LED_3_PIN);
}

static inline void led_4_off() {
	LED_4_GPIOx->ODR &= ~(uint32_t)((uint32_t)1 << (uint32_t)LED_4_PIN);
}

static inline void led_1_tog() {
	LED_1_GPIOx->ODR ^= (uint32_t)((uint32_t)1 << (uint32_t)LED_1_PIN);
}

static inline void led_2_tog() {
	LED_2_GPIOx->ODR ^= (uint32_t)((uint32_t)1 << (uint32_t)LED_2_PIN);
}

static inline void led_3_tog() {
	LED_3_GPIOx->ODR ^= (uint32_t)((uint32_t)1 << (uint32_t)LED_3_PIN);
}

static inline void led_4_tog() {
	LED_4_GPIOx->ODR ^= (uint32_t)((uint32_t)1 << (uint32_t)LED_4_PIN);
}

static inline char led_1_is_on() {
	return (LED_1_GPIOx->ODR & (uint32_t)((uint32_t)1 << (uint32_t)LED_1_PIN)) != 0;
}

static inline char led_2_is_on() {
	return (LED_2_GPIOx->ODR & (uint32_t)((uint32_t)1 << (uint32_t)LED_2_PIN)) != 0;
}

static inline char led_3_is_on() {
	return (LED_3_GPIOx->ODR & (uint32_t)((uint32_t)1 << (uint32_t)LED_3_PIN)) != 0;
}

static inline char led_4_is_on() {
	return (LED_4_GPIOx->ODR & (uint32_t)((uint32_t)1 << (uint32_t)LED_4_PIN)) != 0;
}

static inline void led_1_set(char x)
{
	if (x != 0) {
		led_1_on();
	}
	else {
		led_1_off();
	}
}

static inline void led_2_set(char x)
{
	if (x != 0) {
		led_2_on();
	}
	else {
		led_2_off();
	}
}

static inline void led_3_set(char x)
{
	if (x != 0) {
		led_3_on();
	}
	else {
		led_3_off();
	}
}

static inline void led_4_set(char x)
{
	if (x != 0) {
		led_4_on();
	}
	else {
		led_4_off();
	}
}

static inline void led_tog(uint8_t idx)
{
	switch (idx)
	{
		case 1:
			led_1_tog();
			break;
		case 2:
			led_2_tog();
			break;
		case 3:
			led_3_tog();
			break;
		case 4:
			led_4_tog();
			break;
		default:
			break;
	}
}

static inline void led_off(uint8_t idx)
{
	switch (idx)
	{
		case 1:
			led_1_off();
			break;
		case 2:
			led_2_off();
			break;
		case 3:
			led_3_off();
			break;
		case 4:
			led_4_off();
			break;
		default:
			break;
	}
}

static inline void led_set(uint8_t idx, char x)
{
	switch (idx)
	{
		case 1:
			led_1_set(x);
			break;
		case 2:
			led_2_set(x);
			break;
		case 3:
			led_3_set(x);
			break;
		case 4:
			led_4_set(x);
			break;
		default:
			break;
	}
}

static inline void led_on(uint8_t idx)
{
	switch (idx)
	{
		case 1:
			led_1_on();
			break;
		case 2:
			led_2_on();
			break;
		case 3:
			led_3_on();
			break;
		case 4:
			led_4_on();
			break;
		default:
			break;
	}
}

static inline void led_100ms_event()
{
	for (int i = 0; i < 4; i++)
	{
		led_blink_pattern_t* pattern = (led_blink_pattern_t*)&led_blink_pattern[i];
		if (pattern->on_time > 0 && pattern->on_time_remaining > 0)
		{
			pattern->on_time_remaining--;
		}
		else if (pattern->on_time > 0 && pattern->on_time_remaining <= 0)
		{
			pattern->off_time_remaining = pattern->off_time;
			if (pattern->off_time_remaining > 0) {
				led_off(i + 1);
				pattern->off_time_remaining--;
			}
		}
		else if (pattern->off_time > 0 && pattern->off_time_remaining > 0)
		{
			pattern->off_time_remaining--;
		}
		else if (pattern->off_time > 0 && pattern->off_time_remaining <= 0)
		{
			if (pattern->reload != 0) {
				pattern->reload(i, pattern);
			}
			pattern->on_time_remaining = pattern->on_time;
			if (pattern->on_time_remaining > 0) {
				led_on(i + 1);
				pattern->on_time_remaining--;
			}
		}
	}
}

#endif
