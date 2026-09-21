#include "led_app.h"

#include <string.h>

#define LED_APP_DEFAULT_PERIOD_MS 250U
#define LED_APP_MIN_PERIOD_MS 10U
#define LED_APP_MAX_PERIOD_MS 5000U

static void LedApp_SetLed(LedApp *app, bool led_on)
{
  app->status.led_on = led_on;
  if (app->io.write_led != NULL)
  {
    app->io.write_led(app->io.context, led_on);
  }
}

static void LedApp_WriteResponse(LedApp *app, const uint8_t *data,
                                 size_t length)
{
  if ((app->io.write_uart == NULL) ||
      !app->io.write_uart(app->io.context, data, length))
  {
    app->dropped_responses++;
  }
}

static void LedApp_WriteText(LedApp *app, const char *text)
{
  LedApp_WriteResponse(app, (const uint8_t *)text, strlen(text));
}

static size_t LedApp_AppendText(char *buffer, size_t offset,
                                const char *text)
{
  while (*text != '\0')
  {
    buffer[offset++] = *text++;
  }
  return offset;
}

static size_t LedApp_AppendNumber(char *buffer, size_t offset, uint16_t value)
{
  char digits[5];
  size_t digit_count = 0U;

  do
  {
    digits[digit_count++] = (char)('0' + (value % 10U));
    value = (uint16_t)(value / 10U);
  } while (value != 0U);

  while (digit_count > 0U)
  {
    buffer[offset++] = digits[--digit_count];
  }

  return offset;
}

static void LedApp_WriteStatus(LedApp *app)
{
  char response[40];
  size_t length = 0U;

  length = LedApp_AppendText(response, length, "MODE=");
  if (app->status.mode == LED_MODE_OFF)
  {
    length = LedApp_AppendText(response, length, "OFF");
  }
  else if (app->status.mode == LED_MODE_ON)
  {
    length = LedApp_AppendText(response, length, "ON");
  }
  else
  {
    length = LedApp_AppendText(response, length, "BLINK");
  }

  length = LedApp_AppendText(response, length, " LED=");
  length = LedApp_AppendText(response, length,
                             app->status.led_on ? "ON" : "OFF");
  length = LedApp_AppendText(response, length, " PERIOD=");
  length = LedApp_AppendNumber(response, length, app->status.period_ms);
  response[length++] = '\n';

  LedApp_WriteResponse(app, (const uint8_t *)response, length);
}

static bool LedApp_ParsePeriod(const char *text, size_t length,
                               uint16_t *period_ms)
{
  uint32_t value = 0U;
  size_t index;

  if (length == 0U)
  {
    return false;
  }

  for (index = 0U; index < length; index++)
  {
    char digit = text[index];
    if ((digit < '0') || (digit > '9'))
    {
      return false;
    }

    value = (value * 10U) + (uint32_t)(digit - '0');
    if (value > LED_APP_MAX_PERIOD_MS)
    {
      return false;
    }
  }

  if (value < LED_APP_MIN_PERIOD_MS)
  {
    return false;
  }

  *period_ms = (uint16_t)value;
  return true;
}

static void LedApp_HandleCommand(LedApp *app, uint32_t now_ms)
{
  uint16_t period_ms;

  app->command[app->command_length] = '\0';

  if ((app->command_length == 6U) &&
      (memcmp(app->command, "LED ON", 6U) == 0))
  {
    app->status.mode = LED_MODE_ON;
    LedApp_SetLed(app, true);
    LedApp_WriteText(app, "OK\n");
  }
  else if ((app->command_length == 7U) &&
           (memcmp(app->command, "LED OFF", 7U) == 0))
  {
    app->status.mode = LED_MODE_OFF;
    LedApp_SetLed(app, false);
    LedApp_WriteText(app, "OK\n");
  }
  else if ((app->command_length > 6U) &&
           (memcmp(app->command, "BLINK ", 6U) == 0) &&
           LedApp_ParsePeriod(&app->command[6],
                              (size_t)app->command_length - 6U,
                              &period_ms))
  {
    app->status.mode = LED_MODE_BLINK;
    app->status.period_ms = period_ms;
    app->blink_anchor_ms = now_ms;
    LedApp_SetLed(app, false);
    LedApp_WriteText(app, "OK\n");
  }
  else if ((app->command_length == 6U) &&
           (memcmp(app->command, "STATUS", 6U) == 0))
  {
    LedApp_WriteStatus(app);
  }
  else
  {
    LedApp_WriteText(app, "ERR\n");
  }
}

static void LedApp_UpdateBlink(LedApp *app, uint32_t now_ms)
{
  uint32_t elapsed_ms;
  uint32_t elapsed_periods;

  if (app->status.mode != LED_MODE_BLINK)
  {
    return;
  }

  elapsed_ms = now_ms - app->blink_anchor_ms;
  elapsed_periods = elapsed_ms / app->status.period_ms;
  if (elapsed_periods == 0U)
  {
    return;
  }

  app->blink_anchor_ms += elapsed_periods * app->status.period_ms;
  if ((elapsed_periods & 1U) != 0U)
  {
    LedApp_SetLed(app, !app->status.led_on);
  }
}

static void LedApp_ObserveRxOverflow(LedApp *app)
{
  uint32_t overflow_count = RingBuffer_GetOverflowCount(&app->rx_buffer);

  if (overflow_count != app->observed_rx_overflows)
  {
    app->observed_rx_overflows = overflow_count;
    app->discard_line = true;
    app->saw_carriage_return = false;
    app->command_length = 0U;
  }
}

static void LedApp_ProcessByte(LedApp *app, uint8_t byte, uint32_t now_ms)
{
  if (app->discard_line)
  {
    if (byte == (uint8_t)'\n')
    {
      app->discard_line = false;
      app->saw_carriage_return = false;
      app->command_length = 0U;
      LedApp_WriteText(app, "ERR\n");
    }
    return;
  }

  if (app->saw_carriage_return)
  {
    app->saw_carriage_return = false;
    if (byte == (uint8_t)'\n')
    {
      LedApp_HandleCommand(app, now_ms);
      app->command_length = 0U;
    }
    else
    {
      app->discard_line = true;
      app->command_length = 0U;
    }
    return;
  }

  if (byte == (uint8_t)'\r')
  {
    app->saw_carriage_return = true;
  }
  else if (byte == (uint8_t)'\n')
  {
    LedApp_HandleCommand(app, now_ms);
    app->command_length = 0U;
  }
  else if (app->command_length < LED_APP_MAX_COMMAND_LENGTH)
  {
    app->command[app->command_length++] = (char)byte;
  }
  else
  {
    app->discard_line = true;
    app->command_length = 0U;
  }
}

void LedApp_Init(LedApp *app, const LedAppIo *io, uint32_t now_ms)
{
  RingBuffer_Init(&app->rx_buffer);
  app->io = *io;
  app->status.mode = LED_MODE_OFF;
  app->status.led_on = false;
  app->status.period_ms = LED_APP_DEFAULT_PERIOD_MS;
  app->blink_anchor_ms = now_ms;
  app->observed_rx_overflows = 0U;
  app->dropped_responses = 0U;
  app->command_length = 0U;
  app->saw_carriage_return = false;
  app->discard_line = false;
  LedApp_SetLed(app, false);
}

bool LedApp_RxIsrByte(LedApp *app, uint8_t byte)
{
  return RingBuffer_PushFromIsr(&app->rx_buffer, byte);
}

void LedApp_Process(LedApp *app, uint32_t now_ms)
{
  uint8_t byte;

  LedApp_UpdateBlink(app, now_ms);
  LedApp_ObserveRxOverflow(app);

  while (RingBuffer_Pop(&app->rx_buffer, &byte))
  {
    LedApp_ObserveRxOverflow(app);
    LedApp_ProcessByte(app, byte, now_ms);
  }
}

LedAppStatus LedApp_GetStatus(const LedApp *app)
{
  return app->status;
}

uint32_t LedApp_GetDroppedResponseCount(const LedApp *app)
{
  return app->dropped_responses;
}
