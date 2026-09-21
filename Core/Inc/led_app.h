#ifndef LED_APP_H
#define LED_APP_H

#include "ring_buffer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LED_APP_MAX_COMMAND_LENGTH 32U

typedef enum
{
  LED_MODE_OFF = 0,
  LED_MODE_ON,
  LED_MODE_BLINK
} LedMode;

typedef struct
{
  LedMode mode;
  bool led_on;
  uint16_t period_ms;
} LedAppStatus;

typedef void (*LedAppWriteLedFn)(void *context, bool led_on);
typedef bool (*LedAppWriteUartFn)(void *context, const uint8_t *data,
                                  size_t length);

typedef struct
{
  LedAppWriteLedFn write_led;
  LedAppWriteUartFn write_uart;
  void *context;
} LedAppIo;

typedef struct
{
  RingBuffer rx_buffer;
  LedAppIo io;
  LedAppStatus status;
  uint32_t blink_anchor_ms;
  uint32_t observed_rx_overflows;
  uint32_t dropped_responses;
  char command[LED_APP_MAX_COMMAND_LENGTH + 1U];
  uint8_t command_length;
  bool saw_carriage_return;
  bool discard_line;
} LedApp;

void LedApp_Init(LedApp *app, const LedAppIo *io, uint32_t now_ms);
bool LedApp_RxIsrByte(LedApp *app, uint8_t byte);
void LedApp_Process(LedApp *app, uint32_t now_ms);
LedAppStatus LedApp_GetStatus(const LedApp *app);
uint32_t LedApp_GetDroppedResponseCount(const LedApp *app);

#endif /* LED_APP_H */
