#include "adc_data.h"
#include <main.h>
#include <stdbool.h>
#include "uart_log.h"

const uint16_t AUDIO_DATA_LEN = 2048;
const float32_t ADC_SAMPLING_FREQ = 8130.0f;
const float32_t ADC_SAMPLING_RATE = 1.0f / 8130.0f;
volatile bool AUDIO_DATA_IS_ACTUAL = false;

extern ADC_HandleTypeDef hadc1;

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1)
    {
        AUDIO_DATA_IS_ACTUAL = true;
    }
}

void startAdcDataRecording(uint16_t* pData, const uint16_t length)
{
    AUDIO_DATA_IS_ACTUAL = false;
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)pData, length);
}


#ifdef UART_DEBUG
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef* hadc)
{
    uartPrintf("ADC Error, code: 0x%X\r\n", hadc->ErrorCode);

}
#endif // UART_DEBUG

void waitForAdcData()
{
    HAL_SuspendTick();
    while (!AUDIO_DATA_IS_ACTUAL)
    {
        HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
    }
    HAL_ResumeTick();

    #ifdef UART_DEBUG
    uartPrintf("Audio data is%s actual\n\n\r", AUDIO_DATA_IS_ACTUAL ? "" : " not");
    #endif // UART_DEBUG
}
