/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
/**                                                                         **/
/** ubcsp.h                                                                 **/
/**                                                                         **/
/** MicroBCSP - a very low cost implementation of the BCSP protocol         **/
/**                                                                         **/
/*****************************************************************************/

#include "ubcspcfg.h"
#include <stdint.h>
#include <btstack/hci_cereal.h>

/*****************************************************************************/
/**                                                                         **/
/** ubcsp_packet                                                            **/
/**                                                                         **/
/** This is description of a bcsp packet for the upper layer                **/
/**                                                                         **/
/*****************************************************************************/

struct ubcsp_packet
{
	uint8_t channel;		/* Which Channel this packet is to/from */
	uint8_t reliable;		/* Is this packet reliable */
	uint8_t use_crc;		/* Does this packet use CRC data protection */
	uint16_t length;		/* What is the length of the payload data */
	uint8_t *payload;		/* The payload data itself - size of length */
};

/*****************************************************************************/
/**                                                                         **/
/** ubcsp_configuration                                                     **/
/**                                                                         **/
/** This is the main configuration of the ubcsp engine                      **/
/** All state variables are stored in this structure                        **/
/**                                                                         **/
/*****************************************************************************/

enum ubcsp_link_establishment_state
{
	ubcsp_le_uninitialized,
	ubcsp_le_initialized,
	ubcsp_le_active
};

enum ubcsp_link_establishment_packet
{
	ubcsp_le_sync,
	ubcsp_le_sync_resp,
	ubcsp_le_conf,
	ubcsp_le_conf_resp,
	ubcsp_le_none
};

struct ubcsp_configuration
{
	uint8_t link_establishment_state;
	uint8_t link_establishment_resp;
	uint8_t link_establishment_packet;

	uint8_t sequence_number:3;
	uint8_t ack_number:3;
	uint8_t send_ack;
	struct ubcsp_packet *send_packet;
	struct ubcsp_packet *receive_packet;

	uint8_t receive_header_checksum;
	uint8_t receive_slip_escape;
	int32_t receive_index;

	uint8_t send_header_checksum;
#ifdef UBCSP_CRC
	uint8_t need_send_crc;
	uint16_t send_crc;
#endif
	uint8_t send_slip_escape;

	uint8_t *send_ptr;
	int32_t send_size;
	uint8_t *next_send_ptr;
	int32_t next_send_size;

	int8_t delay;
};

/*****************************************************************************/
/**                                                                         **/
/** ubcsp_poll sets activity from an OR of these flags                      **/
/**                                                                         **/
/*****************************************************************************/

#define UBCSP_PACKET_SENT 0x01
#define UBCSP_PACKET_RECEIVED 0x02
#define UBCSP_PEER_RESET 0x04
#define UBCSP_PACKET_ACK 0x08

/*****************************************************************************/
/**                                                                         **/
/** This is the functional interface for ucbsp                              **/
/**                                                                         **/
/*****************************************************************************/

void ubcsp_initialize (void);
void ubcsp_send_packet (struct ubcsp_packet *send_packet);
void ubcsp_receive_packet (struct ubcsp_packet *receive_packet);
uint8_t ubcsp_poll (uint8_t *activity);

/*****************************************************************************/
/**                                                                         **/
/** Slip Escape Values                                                      **/
/**                                                                         **/
/*****************************************************************************/

#define SLIP_FRAME 0xC0
#define SLIP_ESCAPE 0xDB
#define SLIP_ESCAPE_FRAME 0xDC
#define SLIP_ESCAPE_ESCAPE 0xDD

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

/*****************************************************************************/
/**                                                                         **/
/** These functions need to be linked into your system                      **/
/**                                                                         **/
/*****************************************************************************/

/*****************************************************************************/
/**                                                                         **/
/** put_uart outputs a single octet over the UART Tx line                   **/
/**                                                                         **/
/*****************************************************************************/

extern void hcicereal_tx (uint8_t);

/*****************************************************************************/
/**                                                                         **/
/** get_uart receives a single octet over the UART Rx line                  **/
/** if no octet is available, then this returns 0                           **/
/** if an octet was read, then this is returned in the argument and         **/
/**   the function returns 1                                                **/
/**                                                                         **/
/*****************************************************************************/

extern uint8_t hcicereal_get (uint8_t *);

/*****************************************************************************/
/**                                                                         **/
/** These defines should be changed to your systems concept of 100ms        **/
/**                                                                         **/
/*****************************************************************************/

#define UBCSP_POLL_TIME_IMMEDIATE   0
#define UBCSP_POLL_TIME_DELAY       25

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
