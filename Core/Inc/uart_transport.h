#ifndef UART_TRANSPORT_H
#define UART_TRANSPORT_H

#include "led_app.h"
#include "stm32f1xx_hal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

HAL_StatusTypeDef UartTransport_Init(UART_HandleTypeDef *uart, LedApp *app);
bool UartTransport_Queue(void *context, const uint8_t *data, size_t length);
void UartTransport_Poll(void);
uint32_t UartTransport_GetErrorCount(void);

#endif /* UART_TRANSPORT_H */
