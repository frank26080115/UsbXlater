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
#include <stdlib.h>
#include <string.h>
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

typedef struct
{
	uint16_t length;
	void*    data;
} ptr_item_t;

typedef struct
{
	volatile ptr_item_t*	in;		// current storage location in the circular buffer
	volatile ptr_item_t*	out;	// current retrieval location in the circular buffer
	volatile ptr_item_t*	start;	// pointer to the start of the buffer's underlying storage array
	volatile ptr_item_t*	end;	// pointer to the end of the buffer's underlying storage array
	volatile uint8_t		queueSize;	// max num of items in queue
	volatile uint16_t		dataSize;	// max size of item
	volatile uint8_t		count;	// number of bytes currently stored in the buffer
	volatile uint8_t		flag;
	volatile uint8_t		ready;  // must be 0xAB
} ptr_ringbuffer_t;

static inline void ptr_ringbuffer_init(ptr_ringbuffer_t* buffer, const uint8_t queueSize, const uint16_t dataSize);
static inline void ptr_ringbuffer_init(ptr_ringbuffer_t* buffer, const uint8_t queueSize, const uint16_t dataSize)
{
	ptr_item_t*      dataptr = calloc(queueSize, sizeof(ptr_item_t));
	buffer->in     = dataptr;
	buffer->out    = dataptr;
	buffer->start  = &dataptr[0];
	buffer->end    = &dataptr[queueSize];
	buffer->queueSize = queueSize;
	buffer->dataSize  = dataSize;
	buffer->count  = 0;
	buffer->flag   = 0;
	for (int i = 0; i < queueSize; i++) {
		dataptr[0].data = malloc(dataSize);
	}
	buffer->ready  = 0xAB;
}

static inline void ptr_ringbuffer_push(ptr_ringbuffer_t* buffer, const void* data, uint16_t size);
static inline void ptr_ringbuffer_push(ptr_ringbuffer_t* buffer, const void* data, uint16_t size)
{
	if (size <= 0) {
		return;
	}

	ptr_item_t* n = buffer->in;
	if (size > buffer->dataSize) size = buffer->dataSize;
	n->length = size;
	memcpy(n->data, data, size);

	__disable_irq();

	if (++buffer->in == buffer->end)
		buffer->in = buffer->start;

	buffer->count++;
	__enable_irq();
}

static inline uint16_t ptr_ringbuffer_pop(ptr_ringbuffer_t* buffer, void* dest);
static inline uint16_t ptr_ringbuffer_pop(ptr_ringbuffer_t* buffer, void* dest)
{
	__disable_irq();

	if (buffer->count == 0) {
		__enable_irq();
		return 0;
	}

	ptr_item_t* n = buffer->out;
	uint16_t length = n->length;
	if (dest != 0) {
		memcpy(dest, n->data, length);
	}
	n->length = 0;

	if (++buffer->out == buffer->end)
		buffer->out = buffer->start;

	buffer->count--;

	__enable_irq();

	return length;
}

static inline ptr_item_t* ptr_ringbuffer_peek(ptr_ringbuffer_t* const buffer);
static inline ptr_item_t* ptr_ringbuffer_peek(ptr_ringbuffer_t* const buffer)
{
	return buffer->out;
}

static inline uint16_t ptr_ringbuffer_getcount(ptr_ringbuffer_t* const buffer);
static inline uint16_t ptr_ringbuffer_getcount(ptr_ringbuffer_t* const buffer)
{
	uint8_t count;
	__disable_irq();
	count = buffer->count;
	__enable_irq();
	return count;
}

static inline uint16_t ptr_ringbuffer_getfreecount(ptr_ringbuffer_t* const buffer);
static inline uint16_t ptr_ringbuffer_getfreecount(ptr_ringbuffer_t* const buffer)
{
	uint8_t tmp;
	__disable_irq();
	tmp = buffer->queueSize - buffer->count;
	__enable_irq();
	return tmp;
}

static inline bool ptr_ringbuffer_isempty(ptr_ringbuffer_t* const buffer);
static inline bool ptr_ringbuffer_isempty(ptr_ringbuffer_t* const buffer)
{
	return (ptr_ringbuffer_getcount(buffer) == 0);
}

static inline uint16_t ptr_ringbuffer_isfull(ptr_ringbuffer_t* const buffer);
static inline uint16_t ptr_ringbuffer_isfull(ptr_ringbuffer_t* const buffer)
{
	bool tmp;
	__disable_irq();
	tmp = buffer->queueSize <= buffer->count;
	__enable_irq();
	return tmp;
}

static inline void ptr_ringbuffer_flush(ptr_ringbuffer_t* const buffer);
static inline void ptr_ringbuffer_flush(ptr_ringbuffer_t* const buffer)
{
	while (ptr_ringbuffer_isempty(buffer) == 0) {
		ptr_ringbuffer_pop(buffer, 0);
	}
}

#endif
