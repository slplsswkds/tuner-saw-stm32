#include "uart_log.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

static UART_HandleTypeDef* uartHandle = NULL;
uint8_t UART_TX_DATA[UART_TX_BUFFER_SIZE];
static uint8_t internalUartTxData[UART_TX_BUFFER_SIZE];
volatile bool UART_TX_BUSY = false;

extern UART_HandleTypeDef huart1;

void uartLogInit(UART_HandleTypeDef* huart)
{
    uartHandle = huart;
}

void uartPrintf(const char* fmt, ...)
{
    while (UART_TX_BUSY) { __NOP(); }

    va_list args;
    va_start(args, fmt);
    vsnprintf((char*)UART_TX_DATA, sizeof(UART_TX_DATA), fmt, args);
    va_end(args);

    sendUartStr(UART_TX_DATA);
}

void sendUartStr(const uint8_t* str)
{
    while (UART_TX_BUSY) { __NOP(); }

    memccpy(internalUartTxData, str, '\0', sizeof(internalUartTxData));
    UART_TX_BUSY = true;
    HAL_UART_Transmit_DMA(uartHandle, internalUartTxData, strlen((char*)str));
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart)
{
    if (uartHandle && huart->Instance == uartHandle->Instance)
    {
        UART_TX_BUSY = false;
    }
}
