#ifndef CEREAL_H_
#define CEREAL_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#define CEREAL_BAUD_RATE 57600UL // 57600 for RealTerm default

#define CEREAL_INCOMING_MAX_SIZE 64
#define CEREAL_OUTGOING_MAX_SIZE 64

#define CEREAL_USARTx USART1

#define ENABLE_CEREAL_BUFFERED_TX

void cereal_init();
void cereal_tx(uint8_t x);
void cereal_wait();
void cereal_wait_tx(uint8_t x);
void USART1_IRQHandler(void);
int16_t cereal_rx();
int16_t cereal_peek();
uint8_t cereal_wait_rx();
char cereal_rx_flag();
void cereal_rx_clear_flag();
void cereal_rx_flush();
void cereal_printf(char * format, ...);

#endif
