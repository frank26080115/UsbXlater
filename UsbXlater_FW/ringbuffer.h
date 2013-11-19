/*
 * UsbXlater - by Frank Zhao
 *
 * Ring buffer utilities used for communication
 * code borrowed from LUFA
 *
 */

#ifndef ringbuffer_h_
#define ringbuffer_h_

#include <stdint.h>
#include <stdbool.h>
#include <core_cmFunc.h>

typedef struct
{
	volatile uint8_t*	in;		// current storage location in the circular buffer
	volatile uint8_t*	out;	// current retrieval location in the circular buffer
	volatile uint8_t*	start;	// pointer to the start of the buffer's underlying storage array
	volatile uint8_t*	end;	// pointer to the end of the buffer's underlying storage array
	volatile uint8_t	size;	// size of the buffer's underlying storage array
	volatile uint8_t	count;	// number of bytes currently stored in the buffer
	volatile uint8_t	flag;
	volatile uint8_t	ready;  // must be 0xAB
} ringbuffer_t;

static inline void ringbuffer_init(ringbuffer_t* buffer, uint8_t* const dataptr, const uint16_t size);
static inline void ringbuffer_init(ringbuffer_t* buffer, uint8_t* const dataptr, const uint16_t size)
{
	buffer->in     = dataptr;
	buffer->out    = dataptr;
	buffer->start  = &dataptr[0];
	buffer->end    = &dataptr[size];
	buffer->size   = size;
	buffer->count  = 0;
	buffer->flag   = 0;
	buffer->ready  = 0xAB;
}

static inline void ringbuffer_push(ringbuffer_t* buffer, const uint8_t data);
static inline void ringbuffer_push(ringbuffer_t* buffer, const uint8_t data)
{
	__disable_irq();

	*buffer->in = data;

	if (++buffer->in == buffer->end)
		buffer->in = buffer->start;

	buffer->count++;

	__enable_irq();
}

static inline uint8_t ringbuffer_pop(ringbuffer_t* buffer);
static inline uint8_t ringbuffer_pop(ringbuffer_t* buffer)
{
	__disable_irq();

	uint8_t data = *buffer->out;

	if (++buffer->out == buffer->end)
		buffer->out = buffer->start;

	buffer->count--;

	__enable_irq();

	return data;
}

static inline uint8_t ringbuffer_peek(ringbuffer_t* const buffer);
static inline uint8_t ringbuffer_peek(ringbuffer_t* const buffer)
{
	return *buffer->out;
}

static inline uint16_t ringbuffer_getcount(ringbuffer_t* const buffer);
static inline uint16_t ringbuffer_getcount(ringbuffer_t* const buffer)
{
	uint8_t count;
	__disable_irq();
	count = buffer->count;
	__enable_irq();
	return count;
}

static inline uint16_t ringbuffer_getfreecount(ringbuffer_t* const buffer);
static inline uint16_t ringbuffer_getfreecount(ringbuffer_t* const buffer)
{
	uint8_t tmp;
	__disable_irq();
	tmp = buffer->size - buffer->count;
	__enable_irq();
	return tmp;
}

static inline bool ringbuffer_isempty(ringbuffer_t* const buffer);
static inline bool ringbuffer_isempty(ringbuffer_t* const buffer)
{
	return (ringbuffer_getcount(buffer) == 0);
}

static inline uint16_t ringbuffer_isfull(ringbuffer_t* const buffer);
static inline uint16_t ringbuffer_isfull(ringbuffer_t* const buffer)
{
	bool tmp;
	__disable_irq();
	tmp = buffer->size <= buffer->count;
	__enable_irq();
	return tmp;
}

static inline void ringbuffer_flush(ringbuffer_t* const buffer);
static inline void ringbuffer_flush(ringbuffer_t* const buffer)
{
	__disable_irq();
	buffer->count = 0;
	buffer->in = buffer->start;
	buffer->out = buffer->start;
	__enable_irq();
}

#endif
