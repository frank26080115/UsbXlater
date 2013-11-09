/*
 * UsbXlater - by Frank Zhao
 *
 * This takes care of virtual COM port communications
 * if the VCP device isn't initialized or is not connected to a host
 * then nothing will happen when you try to use any of these functions
 * they will return immediately
 *
 */

#include "vcp.h"
#include "usbd_dev/usbd_dev_cdc.h"
#include "ringbuffer.h"
#include "utilities.h"
#include <stdio.h>
#include <core_cmInstr.h>

void vcp_tx(uint8_t c)
{
	if (USBD_CDC_IsReady == 0) return;

	volatile uint32_t timeout = 0x7FFFFFFF;
	while (ringbuffer_isfull(&USBD_CDC_D2H_FIFO) && timeout--) __NOP();
	if (ringbuffer_isfull(&USBD_CDC_D2H_FIFO) == 0) {
		ringbuffer_push(&USBD_CDC_D2H_FIFO, c);
	}
}

void vcp_printf(char * format, ...)
{
	if (USBD_CDC_IsReady == 0) return;

	int i, j;
	#ifndef va_list
	#define va_list __gnuc_va_list
	#endif

	va_list args;
	va_start(args, format);
	j = vsprintf(global_temp_buff, format, args);
	va_end(args);
	for (i = 0; i < j; i++) {
		vcp_tx(global_temp_buff[i]);
	}
}

char vcp_isReady() { return USBD_CDC_IsReady; }

int16_t vcp_rx()
{
	if (USBD_CDC_IsReady == 0) return -1;
	if (ringbuffer_isempty(&USBD_CDC_H2D_FIFO)) return -1;
	return ringbuffer_pop(&USBD_CDC_H2D_FIFO);
}

int16_t vcp_peek()
{
	if (USBD_CDC_IsReady == 0) return -1;
	if (ringbuffer_isempty(&USBD_CDC_H2D_FIFO)) return -1;
	return ringbuffer_peek(&USBD_CDC_H2D_FIFO);
}
