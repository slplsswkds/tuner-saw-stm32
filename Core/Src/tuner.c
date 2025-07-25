#include "tuner.h"

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
void calculateStringTuningInfo(const float32_t* pFftMag, uint16_t size);
void showInfo();
void normalize(const uint16_t* src, float32_t* dst, size_t len);
float32_t calculateFreqFromFftIndex(uint16_t size, float32_t sampling_freq, uint16_t idx);
float32_t findDominantFrequency(const float32_t* pFftMag, uint16_t size);

int main(void)
{
    HAL_Init();
    SystemClockConfig();
    MxDmaInit();
    MxGpioInit();
    MxAdcInit();
    MxI2cInit();

    #ifdef UART_LOG
    MxUartInit();
    #endif

    HAL_Delay(100);

    if (isWakedUpFromStandby())
    {
        __HAL_PWR_CLEAR_FLAG(PWR_FLAG_SB);
        blinkTimesWithDelay(2, 500);
        #ifdef UART_LOG
        uartPrintf("Waked up from standby\n\r");
        #endif
    }
    else
    {
        blinkTimesWithDelay(5, 100);
        #ifdef UART_LOG
        uartPrintf("First boot\n\r");
        #endif
    }


    ssd1306_Init();

    ssd1306_FlipScreenVertically();
    ssd1306_Clear();
    ssd1306_SetColor(White);

    ssd1306_DrawRect(0, 0, ssd1306_GetWidth(), ssd1306_GetHeight());
    ssd1306_SetCursor(0, 0);
    ssd1306_WriteString("ssd1306_text", Font_7x10);
    ssd1306_UpdateScreen();

    uint16_t pAudioData[AUDIO_DATA_LEN];
    float32_t pFftOutputMag[AUDIO_DATA_LEN];

    arm_rfft_fast_instance_f32 fftInstance;
    arm_rfft_fast_init_f32(&fftInstance, AUDIO_DATA_LEN);

    while (1)
    {
        startAdcDataRecording(pAudioData, AUDIO_DATA_LEN);
        waitForAdcData();
        fft(&fftInstance, pAudioData, pFftOutputMag);
        calculateStringTuningInfo(pFftOutputMag, AUDIO_DATA_LEN);
        // showInfo();
        // HAL_Delay(500);
        // break;
    }
}

void fft(const arm_rfft_fast_instance_f32* pFftInstance, const uint16_t* pAudioData, float32_t* pFftOutputMag)
{
    AUDIO_DATA_IS_ACTUAL = false;
    float32_t pAudioDataNormalized[AUDIO_DATA_LEN];
    float32_t pFftOutput[AUDIO_DATA_LEN];

    normalize(pAudioData, pAudioDataNormalized, AUDIO_DATA_LEN);
    arm_rfft_fast_f32(pFftInstance, pAudioDataNormalized, pFftOutput, 0);

    #ifdef UART_LOG
    for (uint16_t i = 0; i < AUDIO_DATA_LEN; i++)
    {
        if (i % (AUDIO_DATA_LEN / 8) == 0)
        {
            uartPrintf("pFftOutput[%*u] = % .4f\r\n", 4, i, pFftOutput[i]);
        }
    }
    uartPrintf("\r\n");
    #endif

    arm_cmplx_mag_f32(pFftOutput, pFftOutputMag, AUDIO_DATA_LEN / 2);

    #ifdef UART_LOG
    for (uint16_t i = 0; i < AUDIO_DATA_LEN; i++)
    {
        if (i % (AUDIO_DATA_LEN / 8) == 0)
        {
            const float32_t frequency = calculateFreqFromFftIndex(AUDIO_DATA_LEN, ADC_SAMPLING_FREQ, i);
            uartPrintf("pFftOutputMag[%*u; %*.*f Hz] = % .4f\r\n",
                       4, i,
                       8 - 2, 1, frequency,
                       pFftOutputMag[i]);
        }
    }
    uartPrintf("\r\n");
    #endif
}

float32_t calculateFreqFromFftIndex(const uint16_t size, const float32_t sampling_freq, const uint16_t idx)
{
    float32_t frequncy = 0.0f;
    if (idx < size)
    {
        frequncy = (float32_t)idx * sampling_freq / (float32_t)size;
    }
    else
    {
        #ifdef UART_LOG
        uartPrintf("Error: index is out of range\n\n\r");
        #endif
    }
    return frequncy;
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
    const float32_t maxMagFreq = (float32_t)maxMagIdx * ADC_SAMPLING_FREQ / (float32_t)size;

    #ifdef UART_LOG
    uartPrintf("Idx: %u \t\tMax Frequency: %f\n\n\r", maxMagIdx, maxMagFreq);
    #endif
}

void showInfo()
{
    #ifdef UART_LOG
    uartPrintf("Tuning info: empty\n\n\r");
    #endif
}

void normalize(const uint16_t* src, float32_t* dst, const size_t len)
{
    const uint16_t ADC_BITS = 12;
    const uint16_t ADC_MAX = (1 << ADC_BITS) - 1; // 4095
    const float32_t ADC_CENTER = ADC_MAX / 2.0f; // 2047.5
    const float32_t ADC_SCALE = ADC_CENTER; // 2047.5

    for (size_t i = 0; i < len; i++)
    {
        const uint16_t adc_value = src[i] & ADC_MAX;
        dst[i] = ((float)adc_value - ADC_CENTER) / ADC_SCALE;

        #ifdef UART_LOG
        if (i % 256 == 0)
        {
            uartPrintf("src[%*u] = %u;\tdst = %.4f\r\n", 4, i, adc_value, dst[i]);
        }
        #endif
    }
    #ifdef UART_LOG
    uartPrintf("\r\n");
    #endif
}
