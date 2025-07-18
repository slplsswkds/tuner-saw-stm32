#include "tuner.h"

#include <stdio.h>
#include <string.h>

void blinkTimesWithDelay(const int times, const int delay)
{
    for (int i = 0; i < times * 2; i++)
    {
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        HAL_Delay(delay);
    }
}

bool isWakeupFromStandby()
{
    return __HAL_PWR_GET_FLAG(PWR_FLAG_WU);
}

extern ADC_HandleTypeDef hadc1;
extern UART_HandleTypeDef huart1;

volatile bool UART_TX_BUSY = false;

void sendUartStr(const uint8_t* str)
{
    while (UART_TX_BUSY) { __NOP(); }
    HAL_UART_Transmit_DMA(&huart1, str, strlen((char*)str));
}

int main(void)
{
    HAL_Init();
    SystemClockConfig();
    MxGpioInit();
    MxAdcInit();
    MxDmaInit();
    MxUartInit();

    HAL_Delay(100);

    if (isWakeupFromStandby())
    {
        __HAL_PWR_CLEAR_FLAG(PWR_FLAG_SB);
        blinkTimesWithDelay(2, 500);
    }
    else
    {
        blinkTimesWithDelay(5, 100);
    }

    const unsigned int DATA_LEN = 1024;
    uint16_t ADC_VAL[DATA_LEN];
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)ADC_VAL, 1280);

    uint8_t TxData[128];
    unsigned int test_counter = 0;
    while (1)
    {
        snprintf((char*)TxData, sizeof(TxData), "QWERTY %u\n\r", test_counter);
        sendUartStr(TxData);
        HAL_Delay(500);
        test_counter++;
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart)
{
    if (huart->Instance == USART1)
    {
        UART_TX_BUSY = false;
    }
}
