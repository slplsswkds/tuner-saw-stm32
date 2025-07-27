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

#ifdef UART_LOG
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
#endif // UART_LOG

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
    #endif // UART_LOG

    HAL_Delay(100);

    if (isWakedUpFromStandby())
    {
        __HAL_PWR_CLEAR_FLAG(PWR_FLAG_SB);
        blinkTimesWithDelay(2, 500);
        #ifdef UART_LOG
        uartPrintf("Waked up from standby\n\r");
        #endif // UART_LOG
    }
    else
    {
        blinkTimesWithDelay(5, 100);
        #ifdef UART_LOG
        uartPrintf("First boot\n\r");
        #endif // UART_LOG
    }

    HAL_StatusTypeDef oledInitStatus;
    do
    {
        oledInitStatus = ssd1306_Init();
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
    float32_t pFftOutputMag[AUDIO_DATA_LEN/2];

    arm_rfft_fast_instance_f32 fftInstance;
    arm_rfft_fast_init_f32(&fftInstance, AUDIO_DATA_LEN);

    while (1)
    {

        #ifdef UART_LOG
        // uartClearTerminal();
        #endif // UART_LOG
        startAdcDataRecording(pAudioData, AUDIO_DATA_LEN);
        waitForAdcData();

        #ifdef UART_LOG
        // logAudioData(pAudioData, AUDIO_DATA_LEN);
        #endif // UART_LOG

        fft(&fftInstance, pAudioData, pFftOutputMag);
        calculateStringTuningInfo(pFftOutputMag, AUDIO_DATA_LEN);
        showInfo();

        #ifdef UART_LOG
        // HAL_Delay(5000);
        #endif // UART_LOG
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
    // logFftOutput(pFftOutput, AUDIO_DATA_LEN);
    #endif // UART_LOG

    arm_cmplx_mag_squared_f32(pFftOutput, pFftOutputMag, AUDIO_DATA_LEN/2);

    #ifdef UART_LOG
    // logFftOutputMag(pFftOutputMag, AUDIO_DATA_LEN);
    #endif // UART_LOG
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
        #endif // UART_LOG
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
    #endif // UART_LOG
}

void showInfo()
{
    static uint32_t counter = 0;
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu", counter);

    ssd1306_Clear();
    ssd1306_SetCursor(0, 0);
    ssd1306_WriteString(buf, Font_16x26);
    ssd1306_UpdateScreen();
    counter++;
    #ifdef UART_LOG
    uartPrintf("Tuning info: empty\n\n\r");
    #endif // UART_LOG
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
        // if (i % 256 == 0)
        // {
        //     uartPrintf("src[%*u] = %u;\tdst = %.4f\r\n", 4, i, adc_value, dst[i]);
        // }
        #endif // UART_LOG
    }
    #ifdef UART_LOG
    uartPrintf("\r\n");
    #endif // UART_LOG
}
