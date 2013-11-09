#include "cereal.h"
#include "ringbuffer.h"

#include <stm32f2/stm32f2xx.h>
#include <stm32f2/misc.h>
#include <stm32f2/stm32f2xx_rcc.h>
#include <stm32f2/stm32f2xx_gpio.h>
#include <stm32f2/stm32f2xx_usart.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

static volatile ringbuffer_t cereal_incoming;
static volatile ringbuffer_t cereal_outgoing;

void cereal_init()
{
	ringbuffer_init(&cereal_incoming, malloc(CEREAL_INCOMING_MAX_SIZE), CEREAL_INCOMING_MAX_SIZE);
	#ifdef ENABLE_CEREAL_BUFFERED_TX
	ringbuffer_init(&cereal_outgoing, malloc(CEREAL_OUTGOING_MAX_SIZE), CEREAL_OUTGOING_MAX_SIZE);
	#endif

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

	GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);

	GPIO_InitTypeDef gi;
	GPIO_StructInit(&gi);
	gi.GPIO_Pin = GPIO_Pin_9;
	gi.GPIO_Mode = GPIO_Mode_AF;
	gi.GPIO_Speed = GPIO_Speed_50MHz;
	gi.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(GPIOA, &gi);
	gi.GPIO_Pin = GPIO_Pin_10;
	GPIO_Init(GPIOA, &gi);

	USART_InitTypeDef ui;
	USART_StructInit(&ui);
	ui.USART_BaudRate = CEREAL_BAUD_RATE;
	USART_Init(CEREAL_USARTx, &ui);

	USART_Cmd(CEREAL_USARTx, ENABLE);

	USART_ITConfig(CEREAL_USARTx, USART_IT_RXNE, ENABLE);

	NVIC_InitTypeDef ni;
	ni.NVIC_IRQChannel = USART1_IRQn;
	ni.NVIC_IRQChannelPreemptionPriority = 0;
	ni.NVIC_IRQChannelSubPriority = 0;
	ni.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&ni);
}

inline void cereal_tx_raw(uint8_t x) {
	CEREAL_USARTx->DR = x;
}

inline uint8_t cereal_rx_raw() {
	return (uint8_t)CEREAL_USARTx->DR;
}

void cereal_tx(uint8_t x)
{
	#ifdef ENABLE_CEREAL_BUFFERED_TX
	if (!ringbuffer_isfull(&cereal_outgoing))
	{
		ringbuffer_push(&cereal_outgoing, x);
		if (cereal_outgoing.flag == 0) {
			cereal_tx_raw(ringbuffer_pop(&cereal_outgoing));
			cereal_outgoing.flag = 1;
			USART_ITConfig(CEREAL_USARTx, USART_IT_TXE, ENABLE);
		}
	}
	#else
	cereal_tx_raw(x);
	while (USART_GetFlagStatus(CEREAL_USARTx, USART_FLAG_TXE) == RESET) ;
	#endif
}

void cereal_wait()
{
	#ifdef ENABLE_CEREAL_BUFFERED_TX
	while (ringbuffer_isfull(&cereal_outgoing)) ;
	#else
	while (USART_GetFlagStatus(CEREAL_USARTx, USART_FLAG_TXE) == RESET) ;
	#endif
}

void cereal_wait_tx(uint8_t x)
{
	cereal_wait();
	cereal_tx(x);
}

void USART1_IRQHandler(void)
{
	if (USART_GetITStatus(CEREAL_USARTx, USART_IT_RXNE) != RESET)
	{
		uint8_t c = cereal_rx_raw();
		if (!ringbuffer_isfull(&cereal_incoming))
		{
			ringbuffer_push(&cereal_incoming, c);
			if (c == 0 || c == '\r' || c == '\n') {
				cereal_incoming.flag = 1;
			}
		}
		else
		{
			cereal_incoming.flag = 1;
		}
	}

	if (USART_GetITStatus(CEREAL_USARTx, USART_IT_TXE) != RESET )
	{
		#ifdef ENABLE_CEREAL_BUFFERED_TX
		if (!ringbuffer_isempty(&cereal_outgoing)) {
			cereal_tx_raw(ringbuffer_pop(&cereal_outgoing));
		}
		else {
			cereal_outgoing.flag = 0;
			USART_ITConfig(CEREAL_USARTx, USART_IT_TXE, DISABLE);
		}
		#endif
	}
}

int16_t cereal_rx()
{
	if (!ringbuffer_isempty(&cereal_incoming)) {
		return ringbuffer_pop(&cereal_incoming);
	}
	else {
		return -1;
	}
}

int16_t cereal_peek()
{
	if (!ringbuffer_isempty(&cereal_incoming)) {
		return ringbuffer_peek(&cereal_incoming);
	}
	else {
		return -1;
	}
}

uint8_t cereal_wait_rx()
{
	while (ringbuffer_isempty(&cereal_incoming)) ;
	return ringbuffer_pop(&cereal_incoming);
}

char cereal_rx_flag()
{
	return cereal_incoming.flag;
}

void cereal_rx_clear_flag()
{
	cereal_incoming.flag = 0;
}

void cereal_rx_flush()
{
	ringbuffer_flush(&cereal_incoming);
}

void cereal_printf(char * format, ...)
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
		if (ch == '\b') {
			cereal_wait();
		}
		else {
			cereal_tx(ch);
		}
	}
}
