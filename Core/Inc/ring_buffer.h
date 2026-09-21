#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdbool.h>
#include <stdint.h>

#define RING_BUFFER_CAPACITY 64U

typedef struct
{
  volatile uint8_t data[RING_BUFFER_CAPACITY];
  volatile uint8_t write_index;
  volatile uint8_t read_index;
  volatile uint32_t overflow_count;
} RingBuffer;

void RingBuffer_Init(RingBuffer *buffer);
bool RingBuffer_PushFromIsr(RingBuffer *buffer, uint8_t value);
bool RingBuffer_Pop(RingBuffer *buffer, uint8_t *value);
uint32_t RingBuffer_GetOverflowCount(const RingBuffer *buffer);

#endif /* RING_BUFFER_H */
