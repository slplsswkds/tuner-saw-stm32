#include "tuner.h"

#include <stdio.h>
#include <string.h>
#include "arm_math.h"

void blinkTimesWithDelay(const int times, const int delay)
{
    for (int i = 0; i < times * 2; i++)
    {
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        HAL_Delay(delay);
    }
}

bool isWakedUpFromStandby()
{
    return __HAL_PWR_GET_FLAG(PWR_FLAG_WU);
}

extern ADC_HandleTypeDef hadc1;
extern UART_HandleTypeDef huart1;

volatile bool UART_TX_BUSY = false;

void sendUartStr(const uint8_t* str);
void startAdcDataRecording(uint32_t* pData);
void waitForAdcData();
void fft();
void calculateStringTuningInfo();
void showInfo();

const uint32_t AUDIO_DATA_LEN = 1024;
bool AUDIO_DATA_IS_ACTUAL = false;

uint8_t UART_TX_DATA[128];

int main(void)
{
    HAL_Init();
    SystemClockConfig();
    MxGpioInit();
    MxAdcInit();
    MxDmaInit();
    MxUartInit();

    HAL_Delay(100);

    if (isWakedUpFromStandby())
    {
        __HAL_PWR_CLEAR_FLAG(PWR_FLAG_SB);
        blinkTimesWithDelay(2, 500);
    }
    else
    {
        blinkTimesWithDelay(5, 100);
    }

    uint16_t ADC_VAL[AUDIO_DATA_LEN];

    while (1)
    {
        startAdcDataRecording((uint32_t*)ADC_VAL);
        waitForAdcData();
        fft();
        calculateStringTuningInfo();
        showInfo();
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart)
{
    if (huart->Instance == USART1)
    {
        UART_TX_BUSY = false;
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1)
    {
        AUDIO_DATA_IS_ACTUAL = true;
    }
}

void startAdcDataRecording(uint32_t* pData)
{
    HAL_ADC_Start_DMA(&hadc1, pData, 1024);
}

void waitForAdcData()
{
    HAL_SuspendTick();
    HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
    HAL_ResumeTick();

    snprintf((char*)UART_TX_DATA, sizeof(UART_TX_DATA), "Waked Up\n\r");
    sendUartStr(UART_TX_DATA);
}

void fft()
{
}

void calculateStringTuningInfo()
{
}

void showInfo()
{
    snprintf((char*)UART_TX_DATA, sizeof(UART_TX_DATA), "QWERTY\n\r");
    sendUartStr(UART_TX_DATA);
    HAL_Delay(500);
}

void sendUartStr(const uint8_t* str)
{
    uint8_t internalUartTxData[sizeof(UART_TX_DATA)];
    while (UART_TX_BUSY) { __NOP(); }
    memccpy(internalUartTxData, str, '\0', sizeof(internalUartTxData));
    UART_TX_BUSY = true;
    HAL_UART_Transmit_DMA(&huart1, internalUartTxData, strlen((char*)str));
}
