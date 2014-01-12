#ifndef CRC_H_
#define CRC_H_

#include <stdint.h>

#define PERFORM_CRC_TEST

extern uint32_t (*crc32_prefered)(uint8_t*, int);
void crc32_reset();
void crc_init();

#endif
