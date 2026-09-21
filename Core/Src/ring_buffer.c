#include "ring_buffer.h"

_Static_assert((RING_BUFFER_CAPACITY & (RING_BUFFER_CAPACITY - 1U)) == 0U,
               "Ring buffer capacity must be a power of two");

void RingBuffer_Init(RingBuffer *buffer)
{
  buffer->write_index = 0U;
  buffer->read_index = 0U;
  buffer->overflow_count = 0U;
}

bool RingBuffer_PushFromIsr(RingBuffer *buffer, uint8_t value)
{
  uint8_t write_index = buffer->write_index;
  uint8_t used = (uint8_t)(write_index - buffer->read_index);

  if (used >= RING_BUFFER_CAPACITY)
  {
    buffer->overflow_count++;
    return false;
  }

  buffer->data[write_index & (RING_BUFFER_CAPACITY - 1U)] = value;
  buffer->write_index = (uint8_t)(write_index + 1U);
  return true;
}

bool RingBuffer_Pop(RingBuffer *buffer, uint8_t *value)
{
  uint8_t read_index = buffer->read_index;

  if (read_index == buffer->write_index)
  {
    return false;
  }

  *value = buffer->data[read_index & (RING_BUFFER_CAPACITY - 1U)];
  buffer->read_index = (uint8_t)(read_index + 1U);
  return true;
}

uint32_t RingBuffer_GetOverflowCount(const RingBuffer *buffer)
{
  return buffer->overflow_count;
}
