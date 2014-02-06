#include "hci_cereal.h"
#include <ringbuffer.h>
#include <utilities.h>
#include <stm32fx/stm32fxxx.h>
#include <stm32fx/misc.h>
#include <stm32fx/peripherals.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <btstack/hci_cmds.h>

static volatile ringbuffer_t hcicereal_incoming;
static volatile ringbuffer_t hcicereal_outgoing;
volatile h4scan_state_t h4scan_state;
volatile uint32_t hcicereal_lastRxTimestamp;

void hcicereal_init()
{
	ringbuffer_init(&hcicereal_incoming, malloc(HCICEREAL_INCOMING_MAX_SIZE), HCICEREAL_INCOMING_MAX_SIZE);
	#ifdef ENABLE_HCICEREAL_BUFFERED_TX
	ringbuffer_init(&hcicereal_outgoing, malloc(HCICEREAL_OUTGOING_MAX_SIZE), HCICEREAL_OUTGOING_MAX_SIZE);
	#endif

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

	if (HCICEREAL_FLOW_CONTROL == USART_HardwareFlowControl_CTS || HCICEREAL_FLOW_CONTROL == USART_HardwareFlowControl_RTS_CTS)
		GPIO_PinAFConfig(GPIOA, GPIO_PinSource0, GPIO_AF_USART2);
#ifndef ENABLE_HCICEREAL_MANUAL_RTS
	if (HCICEREAL_FLOW_CONTROL == USART_HardwareFlowControl_RTS || HCICEREAL_FLOW_CONTROL == USART_HardwareFlowControl_RTS_CTS)
		GPIO_PinAFConfig(GPIOA, GPIO_PinSource1, GPIO_AF_USART2);
#endif

	GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);

	GPIO_InitTypeDef gi;
	GPIO_StructInit(&gi);

	gi.GPIO_Mode = GPIO_Mode_AF;
	gi.GPIO_Speed = GPIO_Speed_50MHz;
	gi.GPIO_PuPd = GPIO_PuPd_NOPULL;

	gi.GPIO_Pin = GPIO_Pin_2;
	GPIO_Init(GPIOA, &gi);
	gi.GPIO_Pin = GPIO_Pin_3;
	GPIO_Init(GPIOA, &gi);

	if (HCICEREAL_FLOW_CONTROL == USART_HardwareFlowControl_CTS || HCICEREAL_FLOW_CONTROL == USART_HardwareFlowControl_RTS_CTS) {
		gi.GPIO_Pin = GPIO_Pin_0;
		GPIO_Init(GPIOA, &gi);
	}

#ifndef ENABLE_HCICEREAL_MANUAL_RTS
	if (HCICEREAL_FLOW_CONTROL == USART_HardwareFlowControl_RTS || HCICEREAL_FLOW_CONTROL == USART_HardwareFlowControl_RTS_CTS) {
		gi.GPIO_Pin = GPIO_Pin_1;
		GPIO_Init(GPIOA, &gi);
	}
#endif

#ifdef ENABLE_HCICEREAL_MANUAL_RTS
	gi.GPIO_Mode = GPIO_Mode_OUT;
	gi.GPIO_Pin = GPIO_Pin_1;
	GPIO_Init(GPIOA, &gi);
#endif

	USART_InitTypeDef ui;
	USART_StructInit(&ui);
	ui.USART_BaudRate = HCICEREAL_BAUD_RATE;
	ui.USART_HardwareFlowControl = HCICEREAL_FLOW_CONTROL;
	USART_Init(HCICEREAL_USARTx, &ui);

	USART_Cmd(HCICEREAL_USARTx, ENABLE);

	USART_ITConfig(HCICEREAL_USARTx, USART_IT_RXNE, ENABLE);

	NVIC_InitTypeDef ni;
	ni.NVIC_IRQChannel = USART2_IRQn;
	ni.NVIC_IRQChannelPreemptionPriority = 0;
	ni.NVIC_IRQChannelSubPriority = 0;
	ni.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&ni);

	h4scan_state = H4SCAN_UNINIT;
	hcicereal_lastRxTimestamp = systick_1ms_cnt;
}

int hcicereal_set_baudrate(uint32_t baudrate)
{
	uint32_t tmpreg = 0x00, apbclock = 0x00;
	uint32_t integerdivider = 0x00;
	uint32_t fractionaldivider = 0x00;
	RCC_ClocksTypeDef RCC_ClocksStatus;

	RCC_GetClocksFreq(&RCC_ClocksStatus);

	apbclock = ((HCICEREAL_USARTx == USART1) || (HCICEREAL_USARTx == USART6)) ? RCC_ClocksStatus.PCLK2_Frequency : RCC_ClocksStatus.PCLK1_Frequency;

	// Determine the integer part
	if ((HCICEREAL_USARTx->CR1 & USART_CR1_OVER8) != 0) {
		// Integer part computing in case Oversampling mode is 8 Samples
		integerdivider = ((25 * apbclock) / (2 * (baudrate)));
	}
	else {
		// Integer part computing in case Oversampling mode is 16 Samples
		integerdivider = ((25 * apbclock) / (4 * (baudrate)));
	}
	tmpreg = (integerdivider / 100) << 4;

	// Determine the fractional part
	fractionaldivider = integerdivider - (100 * (tmpreg >> 4));

	// Implement the fractional part in the register
	if ((HCICEREAL_USARTx->CR1 & USART_CR1_OVER8) != 0) {
		tmpreg |= ((((fractionaldivider * 8) + 50) / 100)) & ((uint8_t)0x07);
	}
	else {
		tmpreg |= ((((fractionaldivider * 16) + 50) / 100)) & ((uint8_t)0x0F);
	}

	uint16_t oldBRR = HCICEREAL_USARTx->BRR & 0xFFFF;

	if (oldBRR != (uint16_t)tmpreg) // only change if different
	{
		// wait for byte to be transmitted
		while (USART_GetFlagStatus(HCICEREAL_USARTx, USART_FLAG_TC) == RESET);
		// wait for RX to idle
		hcicereal_lastRxTimestamp = systick_1ms_cnt;
		while ((systick_1ms_cnt - hcicereal_lastRxTimestamp) < 10);

		HCICEREAL_USARTx->BRR = (uint16_t)tmpreg;
	}

	return 1;
}

static inline void hcicereal_ReadyToSend() {
	GPIOA->BSRRH = GPIO_Pin_1;
}

static inline void hcicereal_NotReadyToSend() {
	GPIOA->BSRRL = GPIO_Pin_1;
}

inline void hcicereal_tx_raw(uint8_t x) {
	HCICEREAL_USARTx->DR = x;
}

inline char hcicereal_tx_is_busy() {
	return hcicereal_outgoing.flag != 0;
}

inline uint8_t hcicereal_rx_raw() {
	return (uint8_t)HCICEREAL_USARTx->DR;
}

void hcicereal_tx(uint8_t x)
{
	#ifdef ENABLE_HCICEREAL_BUFFERED_TX
	if (!ringbuffer_isfull(&hcicereal_outgoing))
	{
		ringbuffer_push(&hcicereal_outgoing, x);
		if (hcicereal_outgoing.flag == 0) {
			hcicereal_tx_raw(ringbuffer_pop(&hcicereal_outgoing));
			hcicereal_outgoing.flag = 1;
			USART_ITConfig(HCICEREAL_USARTx, USART_IT_TXE, ENABLE);
		}
	}
	#else
	hcicereal_tx_raw(x);
	while (USART_GetFlagStatus(HCICEREAL_USARTx, USART_FLAG_TXE) == RESET) ;
	#endif
}

void hcicereal_wait()
{
	#ifdef ENABLE_HCICEREAL_BUFFERED_TX
	while (ringbuffer_isfull(&hcicereal_outgoing)) ;
	#else
	while (USART_GetFlagStatus(HCICEREAL_USARTx, USART_FLAG_TXE) == RESET) ;
	#endif
}

void hcicereal_wait_tx(uint8_t x)
{
	hcicereal_wait();
	hcicereal_tx(x);
}

void USART2_IRQHandler(void)
{
	if (USART_GetITStatus(HCICEREAL_USARTx, USART_IT_RXNE) != RESET)
	{
		hcicereal_lastRxTimestamp = systick_1ms_cnt;
		uint8_t c = hcicereal_rx_raw();
		if (!ringbuffer_isfull(&hcicereal_incoming)) {
			ringbuffer_push(&hcicereal_incoming, c);
		}
		else {
			hcicereal_incoming.flag = 1;
		}

		#ifdef ENABLE_HCICEREAL_MANUAL_RTS
		if (ringbuffer_getcount(&hcicereal_incoming) > (HCICEREAL_INCOMING_MAX_SIZE - 8)) {
			hcicereal_NotReadyToSend();
		}
	#endif

#ifdef ENABLE_HCICEREAL_H4SCAN
		if (c == HCI_EVENT_PACKET && h4scan_state == H4SCAN_UNINIT) {
			h4scan_state = H4SCAN_HAS_EVENT;
		}
		else if (c == HCI_EVENT_HARDWARE_ERROR && h4scan_state == H4SCAN_HAS_EVENT) {
			h4scan_state = H4SCAN_HAS_EVENT_CODE_HW_ERR;
		}
		else if (c > 0 && h4scan_state == H4SCAN_HAS_EVENT_CODE_HW_ERR) {
			h4scan_state = H4SCAN_HAS_EVENT_PARAM_LENGTH;
		}
		else if (c == 0xFE && h4scan_state == H4SCAN_HAS_EVENT_PARAM_LENGTH) {
			h4scan_state = H4SCAN_HAS_HW_ERR_CODE;
		}
		else if (h4scan_state != H4SCAN_STOP_SCANNING) {
			h4scan_state = H4SCAN_UNINIT;
			if (c == HCI_EVENT_PACKET) {
				h4scan_state = H4SCAN_HAS_EVENT;
			}
		}
#endif

	}

	if (USART_GetITStatus(HCICEREAL_USARTx, USART_IT_TXE) != RESET )
	{
		#ifdef ENABLE_HCICEREAL_BUFFERED_TX
		if (!ringbuffer_isempty(&hcicereal_outgoing)) {
			hcicereal_tx_raw(ringbuffer_pop(&hcicereal_outgoing));
		}
		else {
			hcicereal_outgoing.flag = 0;
			USART_ITConfig(HCICEREAL_USARTx, USART_IT_TXE, DISABLE);
		}
		#endif
	}
}

int16_t hcicereal_rx()
{
	if (!ringbuffer_isempty(&hcicereal_incoming))
	{
		uint8_t c = ringbuffer_pop(&hcicereal_incoming);
#ifdef ENABLE_HCICEREAL_MANUAL_RTS
		if (ringbuffer_getcount(&hcicereal_incoming) < 8) {
			hcicereal_ReadyToSend();
		}
#endif
		return c;
	}
	else
	{
		return -1;
	}
}

int16_t hcicereal_peek()
{
	if (!ringbuffer_isempty(&hcicereal_incoming)) {
		return ringbuffer_peek(&hcicereal_incoming);
	}
	else {
		return -1;
	}
}

uint8_t hcicereal_wait_rx()
{
	while (ringbuffer_isempty(&hcicereal_incoming)) ;
	uint8_t c = ringbuffer_pop(&hcicereal_incoming);
#ifdef ENABLE_HCICEREAL_MANUAL_RTS
	if (ringbuffer_getcount(&hcicereal_incoming) < 8) {
		hcicereal_ReadyToSend();
	}
#endif
	return c;
}

uint8_t hcicereal_get(uint8_t* x)
{
	int16_t c = hcicereal_rx();
	if (c < 0) {
		return 0;
	}
	*x = (uint8_t)c;
	return 1;
}

char hcicereal_rx_flag()
{
	return hcicereal_incoming.flag;
}

void hcicereal_rx_clear_flag()
{
	hcicereal_incoming.flag = 0;
}

void hcicereal_rx_flush()
{
	ringbuffer_flush(&hcicereal_incoming);
#ifdef ENABLE_HCICEREAL_MANUAL_RTS
	hcicereal_ReadyToSend();
#endif
}
