#ifndef VCP_H_
#define VCP_H_

#include <stdint.h>
#include <stdio.h>

void vcp_tx(uint8_t);
void vcp_printf(char * format, ...);
char vcp_isReady();
int16_t vcp_rx();
int16_t vcp_peek();

extern char USBD_CDC_IsReady;

#endif
