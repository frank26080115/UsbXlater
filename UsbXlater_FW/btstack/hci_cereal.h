#ifndef HCI_CEREAL_H_
#define HCI_CEREAL_H_

#include <stdint.h>
#include <stdlib.h>

#define HCICEREAL_BAUD_RATE 115200UL // 115200 for RN-42 default, 38400 for BlueCore4 default

#define HCICEREAL_INCOMING_MAX_SIZE 254
#define HCICEREAL_OUTGOING_MAX_SIZE 254

#define HCICEREAL_USARTx USART2

#define ENABLE_HCICEREAL_BUFFERED_TX

#define ENABLE_HCICEREAL_H4SCAN

#define HCICEREAL_FLOW_CONTROL USART_HardwareFlowControl_RTS_CTS

#define ENABLE_HCICEREAL_MANUAL_RTS

typedef enum
{
	H4SCAN_UNINIT = 0,
	H4SCAN_HAS_EVENT,
	H4SCAN_HAS_EVENT_CODE_HW_ERR,
	H4SCAN_HAS_EVENT_PARAM_LENGTH,
	H4SCAN_HAS_HW_ERR_CODE,
	H4SCAN_STOP_SCANNING,
}
h4scan_state_t;
extern volatile h4scan_state_t h4scan_state;
extern volatile uint32_t hcicereal_lastRxTimestamp;

void hcicereal_init();
void hcicereal_tx(uint8_t x);
void hcicereal_wait();
void hcicereal_wait_tx(uint8_t x);
char hcicereal_tx_is_busy();
void USART2_IRQHandler(void);
int16_t hcicereal_rx();
uint8_t hcicereal_get(uint8_t*);
int16_t hcicereal_peek();
uint8_t hcicereal_wait_rx();
char hcicereal_rx_flag();
void hcicereal_rx_clear_flag();
void hcicereal_rx_flush();
int hcicereal_set_baudrate(uint32_t baudrate);

#endif
