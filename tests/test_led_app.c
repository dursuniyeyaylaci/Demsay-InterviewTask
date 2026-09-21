#include "led_app.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
  LedApp app;
  bool led_on;
  char output[1024];
  size_t output_length;
  bool accept_output;
} Fixture;

static unsigned int failure_count;

#define CHECK(condition)                                                        \
  do                                                                            \
  {                                                                             \
    if (!(condition))                                                           \
    {                                                                           \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);              \
      failure_count++;                                                          \
    }                                                                           \
  } while (0)

static void FakeWriteLed(void *context, bool led_on)
{
  Fixture *fixture = (Fixture *)context;
  fixture->led_on = led_on;
}

static bool FakeWriteUart(void *context, const uint8_t *data, size_t length)
{
  Fixture *fixture = (Fixture *)context;

  if (!fixture->accept_output ||
      (length >= (sizeof(fixture->output) - fixture->output_length)))
  {
    return false;
  }

  memcpy(&fixture->output[fixture->output_length], data, length);
  fixture->output_length += length;
  fixture->output[fixture->output_length] = '\0';
  return true;
}

static void FixtureInit(Fixture *fixture, uint32_t now_ms)
{
  LedAppIo io;

  memset(fixture, 0, sizeof(*fixture));
  fixture->accept_output = true;
  io.write_led = FakeWriteLed;
  io.write_uart = FakeWriteUart;
  io.context = fixture;
  LedApp_Init(&fixture->app, &io, now_ms);
}

static void Feed(Fixture *fixture, const char *text)
{
  while (*text != '\0')
  {
    (void)LedApp_RxIsrByte(&fixture->app, (uint8_t)*text++);
  }
}

static void ExpectOutput(Fixture *fixture, const char *expected)
{
  CHECK(strcmp(fixture->output, expected) == 0);
  fixture->output_length = 0U;
  fixture->output[0] = '\0';
}

static void TestStartupAndStatus(void)
{
  Fixture fixture;
  LedAppStatus status;

  FixtureInit(&fixture, 0U);
  status = LedApp_GetStatus(&fixture.app);
  CHECK(status.mode == LED_MODE_OFF);
  CHECK(!status.led_on);
  CHECK(status.period_ms == 250U);
  CHECK(!fixture.led_on);

  Feed(&fixture, "STATUS\n");
  LedApp_Process(&fixture.app, 0U);
  ExpectOutput(&fixture, "MODE=OFF LED=OFF PERIOD=250\n");
}

static void TestOnOffAndStrictParsing(void)
{
  Fixture fixture;
  LedAppStatus status;

  FixtureInit(&fixture, 10U);

  Feed(&fixture, "LED ON\n");
  LedApp_Process(&fixture.app, 10U);
  ExpectOutput(&fixture, "OK\n");
  status = LedApp_GetStatus(&fixture.app);
  CHECK(status.mode == LED_MODE_ON);
  CHECK(status.led_on && fixture.led_on);

  Feed(&fixture, "STATUS\r\n");
  LedApp_Process(&fixture.app, 11U);
  ExpectOutput(&fixture, "MODE=ON LED=ON PERIOD=250\n");

  Feed(&fixture, "LED OFF\r\n");
  LedApp_Process(&fixture.app, 12U);
  ExpectOutput(&fixture, "OK\n");
  CHECK(!fixture.led_on);

  Feed(&fixture, "led on\nLED  ON\n\n");
  LedApp_Process(&fixture.app, 13U);
  ExpectOutput(&fixture, "ERR\nERR\nERR\n");
}

static void TestBlinkTiming(void)
{
  Fixture fixture;
  LedAppStatus status;

  FixtureInit(&fixture, 1000U);
  Feed(&fixture, "BLINK 250\n");
  LedApp_Process(&fixture.app, 1000U);
  ExpectOutput(&fixture, "OK\n");
  CHECK(!fixture.led_on);

  LedApp_Process(&fixture.app, 1249U);
  CHECK(!fixture.led_on);
  LedApp_Process(&fixture.app, 1250U);
  CHECK(fixture.led_on);
  LedApp_Process(&fixture.app, 1500U);
  CHECK(!fixture.led_on);

  LedApp_Process(&fixture.app, 2000U);
  CHECK(!fixture.led_on);

  Feed(&fixture, "STATUS\n");
  LedApp_Process(&fixture.app, 2000U);
  ExpectOutput(&fixture, "MODE=BLINK LED=OFF PERIOD=250\n");
  status = LedApp_GetStatus(&fixture.app);
  CHECK(status.mode == LED_MODE_BLINK);
  CHECK(status.period_ms == 250U);
}

static void TestBlinkRanges(void)
{
  Fixture fixture;

  FixtureInit(&fixture, 0U);
  Feed(&fixture, "BLINK 10\nBLINK 5000\n");
  LedApp_Process(&fixture.app, 0U);
  ExpectOutput(&fixture, "OK\nOK\n");

  Feed(&fixture,
       "BLINK 9\nBLINK 5001\nBLINK +10\nBLINK 10x\nBLINK  10\n");
  LedApp_Process(&fixture.app, 1U);
  ExpectOutput(&fixture, "ERR\nERR\nERR\nERR\nERR\n");
}

static void TestFragmentedAndMultipleCommands(void)
{
  Fixture fixture;

  FixtureInit(&fixture, 0U);
  Feed(&fixture, "LED ");
  LedApp_Process(&fixture.app, 0U);
  ExpectOutput(&fixture, "");

  Feed(&fixture, "ON\nSTATUS\r\n");
  LedApp_Process(&fixture.app, 1U);
  ExpectOutput(&fixture, "OK\nMODE=ON LED=ON PERIOD=250\n");
}

static void TestLineLengthAndRecovery(void)
{
  Fixture fixture;
  char line[40];

  FixtureInit(&fixture, 0U);

  memset(line, 'A', 32U);
  line[32] = '\n';
  line[33] = '\0';
  Feed(&fixture, line);
  LedApp_Process(&fixture.app, 0U);
  ExpectOutput(&fixture, "ERR\n");

  memset(line, 'B', 33U);
  line[33] = '\n';
  line[34] = '\0';
  Feed(&fixture, line);
  LedApp_Process(&fixture.app, 1U);
  ExpectOutput(&fixture, "ERR\n");

  Feed(&fixture, "LED ON\rX\nSTATUS\n");
  LedApp_Process(&fixture.app, 2U);
  ExpectOutput(&fixture, "ERR\nMODE=OFF LED=OFF PERIOD=250\n");
}

static void TestRingOverflowRecovery(void)
{
  Fixture fixture;
  unsigned int index;

  FixtureInit(&fixture, 0U);
  for (index = 0U; index < 70U; index++)
  {
    (void)LedApp_RxIsrByte(&fixture.app, (uint8_t)'A');
  }
  LedApp_Process(&fixture.app, 0U);
  ExpectOutput(&fixture, "");

  Feed(&fixture, "\n");
  LedApp_Process(&fixture.app, 1U);
  ExpectOutput(&fixture, "ERR\n");

  Feed(&fixture, "STATUS\n");
  LedApp_Process(&fixture.app, 2U);
  ExpectOutput(&fixture, "MODE=OFF LED=OFF PERIOD=250\n");
}

static void TestTickWraparound(void)
{
  Fixture fixture;
  uint32_t start = UINT32_MAX - 100U;

  FixtureInit(&fixture, start);
  Feed(&fixture, "BLINK 250\n");
  LedApp_Process(&fixture.app, start);
  ExpectOutput(&fixture, "OK\n");

  LedApp_Process(&fixture.app, 148U);
  CHECK(!fixture.led_on);
  LedApp_Process(&fixture.app, 149U);
  CHECK(fixture.led_on);
}

static void TestOutputFailureIsCounted(void)
{
  Fixture fixture;

  FixtureInit(&fixture, 0U);
  fixture.accept_output = false;
  Feed(&fixture, "STATUS\n");
  LedApp_Process(&fixture.app, 0U);
  CHECK(LedApp_GetDroppedResponseCount(&fixture.app) == 1U);
}

int main(void)
{
  TestStartupAndStatus();
  TestOnOffAndStrictParsing();
  TestBlinkTiming();
  TestBlinkRanges();
  TestFragmentedAndMultipleCommands();
  TestLineLengthAndRecovery();
  TestRingOverflowRecovery();
  TestTickWraparound();
  TestOutputFailureIsCounted();

  if (failure_count != 0U)
  {
    printf("%u test assertion(s) failed.\n", failure_count);
    return 1;
  }

  printf("All LED application tests passed.\n");
  return 0;
}
