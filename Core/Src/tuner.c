#include "tuner.h"

#include <stdio.h>
#include <string.h>
#include "arm_math.h"
#include "uart_log.h"

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

#ifdef UART_LOG
void sendUartStr(const uint8_t* str);
#endif

void startAdcDataRecording(uint16_t* pData, uint32_t length);
void waitForAdcData();
void fft(const arm_rfft_fast_instance_f32* pFftInstance, const uint16_t* pAudioData, float32_t* pFftOutputMag);
void calculateStringTuningInfo(const float32_t* pFftMag, uint16_t size);
void showInfo();
void convert_uint16_to_float32(const uint16_t* src, float* dst, size_t len);

const uint32_t AUDIO_DATA_LEN = 1024;
volatile bool AUDIO_DATA_IS_ACTUAL = false;

int main(void)
{
    HAL_Init();
    SystemClockConfig();
    MxDmaInit();
    MxGpioInit();
    MxAdcInit();

#ifdef UART_LOG
    MxUartInit();
    uartLogInit(&huart1);
    #endif

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

    uint16_t pAudioData[AUDIO_DATA_LEN];
    float32_t pFftOutputMag[AUDIO_DATA_LEN];

    arm_rfft_fast_instance_f32 fftInstance;
    arm_rfft_fast_init_f32(&fftInstance, AUDIO_DATA_LEN);

    while (1)
    {
        memset(pAudioData, 0, sizeof(pAudioData));

        startAdcDataRecording(pAudioData, AUDIO_DATA_LEN);
        waitForAdcData();

        #ifdef UART_LOG
        for (uint16_t i = 0; i < AUDIO_DATA_LEN; i++)
        {
            uartPrintf("%u ", pAudioData[i]);
        }
        uartPrintf("\n\r");
        #endif

        // fft(&fftInstance, pAudioData, pFftOutputMag);
        // calculateStringTuningInfo(pFftOutputMag, AUDIO_DATA_LEN);
        // showInfo();
        HAL_Delay(500);
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1)
    {
        AUDIO_DATA_IS_ACTUAL = true;
    }
}

void startAdcDataRecording(uint16_t* pData, const uint32_t length)
{
    AUDIO_DATA_IS_ACTUAL = false;
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)pData, length);
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef* hadc)
{
    #ifdef UART_LOG
    uartPrintf("ADC Error, code: 0x%X\r\n", hadc->ErrorCode);
    #endif
}

void waitForAdcData()
{
    HAL_SuspendTick();
    while (!AUDIO_DATA_IS_ACTUAL)
    {
        HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
    }
    HAL_ResumeTick();

    #ifdef UART_LOG
    uartPrintf("Audio data is%s actual\n\r", AUDIO_DATA_IS_ACTUAL ? "" : " not");
    #endif
}

void fft(const arm_rfft_fast_instance_f32* pFftInstance, const uint16_t* pAudioData, float32_t* pFftOutputMag)
{
    AUDIO_DATA_IS_ACTUAL = false;
    float32_t pFftInput[AUDIO_DATA_LEN];
    float32_t pFftOutput[AUDIO_DATA_LEN];

    convert_uint16_to_float32(pAudioData, pFftInput, AUDIO_DATA_LEN);
    arm_rfft_fast_f32(pFftInstance, pFftInput, pFftOutput, 0);
    arm_cmplx_mag_f32(pFftOutput, pFftOutputMag, AUDIO_DATA_LEN / 2);
}

void calculateStringTuningInfo(const float32_t* pFftMag, const uint16_t size)
{
    float32_t maxMag = 0.0f;
    uint16_t maxMagIdx = 0;
    for (uint16_t i = 0; i < size; i++)
    {
        if (pFftMag[i] > maxMag)
        {
            maxMag = pFftMag[i];
            maxMagIdx = i;
        }
    }
    const float sampingFreq = 8130; // Hz
    const float32_t maxMagFreq = (float32_t)maxMagIdx * sampingFreq / (float32_t)size;

    #ifdef UART_LOG
    uartPrintf("Idx: %u \t\tMax Frequency: %f\n\r", maxMagIdx, maxMagFreq);
    #endif
}

void showInfo()
{
    #ifdef UART_LOG
    uartPrintf("Tuning info: empty\n\r");
    #endif
}

void convert_uint16_to_float32(const uint16_t* src, float* dst, const size_t len)
{
    const uint16_t ADC_BITS = 12;
    const uint16_t ADC_MAX = (1 << ADC_BITS) - 1; // 4095
    const float ADC_CENTER = ADC_MAX / 2.0f; // 2047.5
    const float ADC_SCALE = ADC_CENTER; // 2047.5

    for (size_t i = 0; i < len; i++)
    {
        const uint16_t adc_value = src[i] & ADC_MAX;
        dst[i] = ((float)adc_value - ADC_CENTER) / ADC_SCALE;

        #ifdef UART_LOG
        if (i % 64 == 0)
        {
            uartPrintf("src[%u] = %u;\tdst = %.4f\r\n", i, adc_value, dst[i]);
        }
        #endif
    }
}
