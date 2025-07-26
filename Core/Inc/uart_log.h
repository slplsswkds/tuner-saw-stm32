#pragma once

#include "main.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define UART_TX_BUFFER_SIZE 128

extern volatile bool UART_TX_BUSY;

void uartPrintf(const char* fmt, ...);
void sendUartStr(const uint8_t* str);
void uartClearTerminal();