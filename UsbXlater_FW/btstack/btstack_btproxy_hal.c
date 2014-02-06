#include "hal_cpu.h"
#include "hal_tick.h"
#include <cmsis/core_cmFunc.h>

void hal_cpu_disable_irqs(void) {
	__disable_irq();
}

void hal_cpu_enable_irqs(void) {
	__enable_irq();
}

void hal_cpu_enable_irqs_and_sleep(void) {
	__enable_irq();
}

void hal_tick_init(void) {
}

void hal_dummy_tick_handler(void) {
}

void (*hal_tick_handler)(void) = 0;

void hal_tick_set_handler(void (*tick_handler)(void)) {
	hal_tick_handler = tick_handler;
}

int hal_tick_get_tick_period_in_ms(void) {
	return 1;
}

void hal_uart_dma_init(void) { }
void hal_uart_dma_set_block_received( void (*block_handler)(void)) { }
void hal_uart_dma_set_block_sent( void (*block_handler)(void)) { }
void hal_uart_dma_set_csr_irq_handler( void (*csr_irq_handler)(void)) { }
int  hal_uart_dma_set_baud(uint32_t baud) { return 0; }
void hal_uart_dma_send_block(const uint8_t *buffer, uint16_t length) { }
void hal_uart_dma_receive_block(uint8_t *buffer, uint16_t len) { }
void hal_uart_dma_set_sleep(uint8_t sleep) { }
