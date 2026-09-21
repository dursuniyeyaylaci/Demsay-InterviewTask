#include "uart_transport.h"

#define UART_TX_QUEUE_CAPACITY 256U

_Static_assert((UART_TX_QUEUE_CAPACITY & (UART_TX_QUEUE_CAPACITY - 1U)) == 0U,
               "UART TX queue capacity must be a power of two");

static UART_HandleTypeDef *transport_uart;
static LedApp *transport_app;
static uint8_t rx_byte;
static uint8_t tx_active_byte;
static uint8_t tx_queue[UART_TX_QUEUE_CAPACITY];
static volatile uint16_t tx_write_index;
static volatile uint16_t tx_read_index;
static volatile bool tx_busy;
static volatile uint32_t uart_error_count;

static void UartTransport_StartNext(void)
{
  if (tx_busy || (tx_read_index == tx_write_index) ||
      (transport_uart == NULL))
  {
    return;
  }

  tx_active_byte = tx_queue[tx_read_index & (UART_TX_QUEUE_CAPACITY - 1U)];
  if (HAL_UART_Transmit_IT(transport_uart, &tx_active_byte, 1U) == HAL_OK)
  {
    tx_busy = true;
  }
}

HAL_StatusTypeDef UartTransport_Init(UART_HandleTypeDef *uart, LedApp *app)
{
  transport_uart = uart;
  transport_app = app;
  tx_write_index = 0U;
  tx_read_index = 0U;
  tx_busy = false;
  uart_error_count = 0U;

  return HAL_UART_Receive_IT(transport_uart, &rx_byte, 1U);
}

bool UartTransport_Queue(void *context, const uint8_t *data, size_t length)
{
  uint32_t interrupt_state;
  uint16_t used;
  uint16_t write_index;
  size_t index;

  (void)context;

  if ((data == NULL) || (length > UART_TX_QUEUE_CAPACITY))
  {
    return false;
  }

  interrupt_state = __get_PRIMASK();
  __disable_irq();

  used = (uint16_t)(tx_write_index - tx_read_index);
  if (length > (size_t)(UART_TX_QUEUE_CAPACITY - used))
  {
    if (interrupt_state == 0U)
    {
      __enable_irq();
    }
    return false;
  }

  write_index = tx_write_index;
  for (index = 0U; index < length; index++)
  {
    tx_queue[write_index & (UART_TX_QUEUE_CAPACITY - 1U)] = data[index];
    write_index++;
  }
  tx_write_index = write_index;
  UartTransport_StartNext();

  if (interrupt_state == 0U)
  {
    __enable_irq();
  }

  return true;
}

void UartTransport_Poll(void)
{
  uint32_t interrupt_state = __get_PRIMASK();

  __disable_irq();
  UartTransport_StartNext();
  if (interrupt_state == 0U)
  {
    __enable_irq();
  }
}

uint32_t UartTransport_GetErrorCount(void)
{
  return uart_error_count;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if ((huart == transport_uart) && (transport_app != NULL))
  {
    (void)LedApp_RxIsrByte(transport_app, rx_byte);
    if (HAL_UART_Receive_IT(transport_uart, &rx_byte, 1U) != HAL_OK)
    {
      uart_error_count++;
    }
  }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if ((huart == transport_uart) && tx_busy)
  {
    tx_read_index++;
    tx_busy = false;
    UartTransport_StartNext();
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart == transport_uart)
  {
    uart_error_count++;
    (void)HAL_UART_Receive_IT(transport_uart, &rx_byte, 1U);
  }
}
