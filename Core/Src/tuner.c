#include "tuner.h"

#include <stdbool.h>
#include <uart_log.h>
#include <stdio.h>
#include "arm_math.h"
#include "adc_data.h"
#include "string_tuning.h"
#include "ssd1306.h"

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

void fft(const arm_rfft_fast_instance_f32* pFftInstance, const uint16_t* pAudioData, float32_t* pFftOutputMag);
void showInfo();
void normalize(const uint16_t* src, float32_t* dst, size_t len);

#ifdef UART_DEBUG_ARRAYS
static void logAudioData(const uint16_t* pAudioData, const uint16_t size)
{
    uartPrintf("pAudioData[idx]:\n\r");
    const uint16_t blockSize = 32;
    for (uint16_t i = 0; i < size; i += blockSize)
    {
        const uint16_t blockEnd = i + blockSize - 1 < size ? i + blockSize - 1 : size - 1;
        uartPrintf("[%4u..%4u]: ", i, blockEnd);

        for (uint16_t j = i; j <= blockEnd; j++)
        {
            uartPrintf("%5u ", pAudioData[j]);
        }

        uartPrintf("\n\r");
    }
}

static void logNormalizedAudioData(const float32_t* pAudioDataNormalized, const uint16_t size)
{
    uartPrintf("pAudioDataNormalized[idx]:\n\r");
    const uint16_t blockSize = 32;
    for (uint16_t i = 0; i < size; i += blockSize)
    {
        const uint16_t blockEnd = i + blockSize - 1 < size ? i + blockSize - 1 : size - 1;
        uartPrintf("[%4u..%4u]: ", i, blockEnd);

        for (uint16_t j = i; j <= blockEnd; j++)
        {
            uartPrintf(" %3.1f\t", pAudioDataNormalized[j]);
        }

        uartPrintf("\n\r");
    }
}

static void logFftOutput(const float32_t* pFftOutput, const uint16_t size)
{
    uartPrintf("pFftOutput[idx]:\n\r");
    const uint16_t blockSize_ = 16;
    for (uint16_t i = 0; i < size; i += blockSize_)
    {
        const uint16_t blockEnd = i + blockSize_ - 1 < size ? i + blockSize_ - 1 : size - 1;
        uartPrintf("[%4u..%4u]: ", i, blockEnd);

        for (uint16_t j = i; j + 1 <= blockEnd; j += 2)
        {
            const float32_t real = pFftOutput[j];
            const float32_t imag = pFftOutput[j + 1];
            uartPrintf("%7.1f, %7.1f | ", real, imag);
        }

        uartPrintf("\n\r");
    }
    uartPrintf("\n\r");
}

static void logFftOutputMag(const float32_t* pFftOutputMag, const uint16_t size)
{
    uartPrintf("pFftOutputMag[idx]:\n\r");
    const uint16_t blockSize = 8;
    for (uint16_t i = 0; i < size; i += blockSize)
    {
        const uint16_t blockEnd = i + blockSize - 1 < size ? i + blockSize - 1 : size - 1;
        uartPrintf("[%4u..%4u]: ", i, blockEnd);

        for (uint16_t j = i; j <= blockEnd; j++)
        {
            const float32_t freq = calculateFreqFromFftIndex(size, ADC_SAMPLING_FREQ, j);
            uartPrintf("%6.1fHz: %6.2f | ", freq, pFftOutputMag[j]);
        }

        uartPrintf("\n\r");
    }
    uartPrintf("\n\r");
}
#endif // UART_DEBUG_ARRAYS

int main(void)
{
    HAL_Init();
    SystemClockConfig();
    MxDmaInit();
    MxGpioInit();
    MxAdcInit();
    MxI2cInit();

    #ifdef UART
    MxUartInit();
    #endif // UART

    if (isWakedUpFromStandby())
    {
        __HAL_PWR_CLEAR_FLAG(PWR_FLAG_SB);
        // blinkTimesWithDelay(2, 500);
        #ifdef UART_LOG
        uartPrintf("Waked up from standby\n\r");
        #endif // UART_LOG
    }
    else
    {
        // blinkTimesWithDelay(5, 100);
        #ifdef UART_LOG
        uartPrintf("First boot\n\r");
        #endif // UART_LOG
    }

    HAL_GPIO_WritePin(MOSFET_GATE_GPIO_Port, MOSFET_GATE_Pin, GPIO_PIN_SET);

    HAL_StatusTypeDef oledInitStatus;
    do
    {
        oledInitStatus = ssd1306_Init();
        blinkTimesWithDelay(1, 100);
        #ifdef UART_LOG
        const char* statusStr = NULL;

        switch (oledInitStatus)
        {
        case HAL_OK:
            statusStr = "OK";
            break;
        case HAL_ERROR:
            statusStr = "ERROR";
            break;
        case HAL_BUSY:
            statusStr = "BUSY";
            break;
        case HAL_TIMEOUT:
            statusStr = "TIMEOUT";
            break;
        default:
            statusStr = "UNKNOWN";
            break;
        }
        uartPrintf("OLED init status: %s\n\r", statusStr);
        #endif // UART_LOG
    }
    while (oledInitStatus != HAL_OK);

    ssd1306_FlipScreenVertically();
    ssd1306_SetColor(White);
    ssd1306_UpdateScreen();

    uint16_t pAudioData[AUDIO_DATA_LEN];
    float32_t pFftOutputMag[AUDIO_DATA_LEN];

    arm_rfft_fast_instance_f32 fftInstance;
    arm_rfft_fast_init_f32(&fftInstance, AUDIO_DATA_LEN);

    #ifdef UART
    uartClearTerminal();
    #endif // UART

    while (1)
    {
        #ifdef UART_DEBUG
        uartClearTerminal();
        #endif // UART_DEBUG
        startAdcDataRecording(pAudioData, AUDIO_DATA_LEN);
        ssd1306_UpdateScreen();
        waitForOledReadiness();
        ssd1306_Clear();
        waitForAdcData();
        #ifdef UART_DEBUG_ARRAYS
        logAudioData(pAudioData, AUDIO_DATA_LEN);
        #endif // UART_DEBUG_ARRAYS
        fft(&fftInstance, pAudioData, pFftOutputMag);
        calculateStringTuningInfo(pFftOutputMag, AUDIO_DATA_LEN);
        // showInfo();
        #ifdef UART_DEBUG
        HAL_Delay(5000);
        #endif // UART_DEBUG
    }
}

void fft(const arm_rfft_fast_instance_f32* pFftInstance, const uint16_t* pAudioData, float32_t* pFftOutputMag)
{
    // HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, !HAL_GPIO_ReadPin(LED_GPIO_Port, LED_Pin));
    AUDIO_DATA_IS_ACTUAL = false;
    float32_t pAudioDataNormalized[AUDIO_DATA_LEN];
    float32_t pFftOutput[AUDIO_DATA_LEN];
    normalize(pAudioData, pAudioDataNormalized, AUDIO_DATA_LEN);
    #ifdef UART_DEBUG_ARRAYS
    logNormalizedAudioData(pAudioDataNormalized, AUDIO_DATA_LEN);
    #endif // UART_DEBUG_ARRAYS
    arm_rfft_fast_f32(pFftInstance, pAudioDataNormalized, pFftOutput, 0);
    #ifdef UART_DEBUG_ARRAYS
    logFftOutput(pFftOutput, AUDIO_DATA_LEN);
    #endif // UART_DEBUG_ARRAYS
    arm_cmplx_mag_squared_f32(pFftOutput, pFftOutputMag, AUDIO_DATA_LEN);
    #ifdef UART_DEBUG_ARRAYS
    logFftOutputMag(pFftOutputMag, AUDIO_DATA_LEN);
    #endif // UART_DEBUG_ARRAYS
}

void showInfo()
{
    #ifdef UART_LOG
    uartPrintf("Tuning info: empty\n\n\r");
    #endif // UART_LOG
}

void normalize(const uint16_t* src, float32_t* dst, const size_t len)
{
    const uint16_t ADC_BITS = 12;
    const uint16_t ADC_MAX = (1 << ADC_BITS) - 1; // 4095
    const float32_t ADC_CENTER = (float32_t)ADC_MAX / 2.0f; // 2047.5
    const float32_t ADC_SCALE = ADC_CENTER;

    int32_t mean = 0;
    for (size_t i = 0; i < len; i++)
    {
        mean += src[i];
    }
    mean /= (int32_t)len;

    #ifdef UART_LOG
    uartPrintf("Mean: %ld\n\r", mean);
    #endif // UART_LOG

    for (size_t i = 0; i < len; i++)
    {
        const int32_t centered = (int32_t)src[i] - mean;
        dst[i] = (float32_t)centered / ADC_SCALE;
    }
}
