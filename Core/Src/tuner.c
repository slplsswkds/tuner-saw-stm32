#include "tuner.h"

#include "ssd1306.h"
#include <stdarg.h>

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

void oledPrintNoUpdate(char* str, const uint8_t x, const uint8_t y, const FontDef font)
{
    ssd1306_SetCursor(x, y);
    ssd1306_WriteString(str, font);
}

void oledPrintf(const uint8_t x, const uint8_t y, const FontDef font, const char* fmt, ...)
{
    static char oledBuf[16];

    va_list args;
    va_start(args, fmt);
    vsnprintf(oledBuf, sizeof(oledBuf), fmt, args);
    va_end(args);

    oledPrintNoUpdate(oledBuf, x, y, font);
}

const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

const float32_t REFERENCE_FREQUENCY = 440.0f; // Частота A4
const uint8_t REFERENCE_MIDI_NUMBER = 69; // MIDI номер A4
const uint8_t SEMITONES_PER_OCTAVE = 12; // Півтонів в октаві
const float32_t ROUNDING_OFFSET = 0.5f; // Для округлення
const uint8_t MIDI_OCTAVE_OFFSET = 1; // Для обчислення правильної октави
const float32_t CENTS_TOLERANCE = 5.0f; // Допустиме відхилення в центах

typedef enum
{
    LOW,
    HIGH,
    OK,
    UNKNOWN,
} StringTension;

inline float32_t calculateNoteNumber(const float32_t frequency)
{
    return (float32_t)REFERENCE_MIDI_NUMBER + (float32_t)SEMITONES_PER_OCTAVE * log2f(frequency / REFERENCE_FREQUENCY);
}

inline uint8_t calculateRoundedNoteNumber(const float32_t noteNumber)
{
    return (uint8_t)(noteNumber + ROUNDING_OFFSET);
}

inline uint8_t calculateNoteIndex(const uint8_t roundedNoteNumber)
{
    return roundedNoteNumber % SEMITONES_PER_OCTAVE;
}

inline uint8_t calculateNoteOctave(const uint8_t roundedNoteNumber)
{
    return roundedNoteNumber / SEMITONES_PER_OCTAVE - MIDI_OCTAVE_OFFSET;
}

void detectNote(const float32_t frequency)
{
    if (frequency <= 0.0f)
    {
        #ifdef UART_LOG
        uartPrintf("Invalid frequency\n\r");
        #endif // UART_LOG
        oledPrintf(0, 0, Font_7x10, "Invalid");
        return;
    }

    // Обчислення MIDI-номера найближчої ноти
    const float32_t noteNumber = calculateNoteNumber(frequency);
    const uint8_t roundedNoteNumber = calculateRoundedNoteNumber(noteNumber);
    const uint8_t noteIndex = calculateNoteIndex(roundedNoteNumber);
    const uint8_t octave = calculateNoteOctave(roundedNoteNumber);

    // Частота для еталонної ноти
    const float32_t idealFrequency = REFERENCE_FREQUENCY * powf(
        2.0f, ((float32_t)roundedNoteNumber - (float32_t)REFERENCE_MIDI_NUMBER) / (float32_t)SEMITONES_PER_OCTAVE);

    // Різниця в центах
    const float32_t centsDiff = 1200.0f * log2f(frequency / idealFrequency);

    StringTension stringTension = UNKNOWN;

    if (fabsf(centsDiff) < CENTS_TOLERANCE)
    {
        stringTension = OK;
    }
    else if (centsDiff < 0.0f)
    {
        stringTension = LOW;
    }
    else
    {
        stringTension = HIGH;
    }

    switch (stringTension)
    {
    case OK:
        #ifdef UART_LOG
        uartPrintf("✔ In tune\n\r");
        #endif // UART_LOG
        break;
    case LOW:
        #ifdef UART_LOG
        uartPrintf("↓ Too low — tighten the string\n\r");
        #endif // UART_LOG
        break;
    case HIGH:
        #ifdef UART_LOG
        uartPrintf("↑ Too high — loosen the string\n\r");
        #endif // UART_LOG
        break;
    default: ;
    }

    #ifdef UART_LOG
    uartPrintf("Note: %s%d (MIDI %d)\n\r", noteNames[noteIndex], octave, roundedNoteNumber);
    uartPrintf("Detected freq: %.2f Hz\tIdeal freq: %.2f Hz\n\r", frequency, idealFrequency);
    uartPrintf("Diff: %.2f cents\n\r", centsDiff);
    uartPrintf("\n\r");
    #endif // UART_LOG

    oledPrintf(19, 0, Font_11x18, "%s%d", noteNames[noteIndex], octave);
    oledPrintf(0, 0, Font_7x10, "%s%d", noteNames[noteIndex - 1], octave);
    oledPrintf(58, 0, Font_7x10, "%s%d", noteNames[noteIndex + 1], octave);
    oledPrintf(0, 22, Font_11x18, "%.2f", centsDiff);
}

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
    float32_t pFftOutputMag[AUDIO_DATA_LEN / 2];

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
    AUDIO_DATA_IS_ACTUAL = false;
    float32_t pAudioDataNormalized[AUDIO_DATA_LEN];
    float32_t pFftOutput[AUDIO_DATA_LEN];

    normalize(pAudioData, pAudioDataNormalized, AUDIO_DATA_LEN);
    arm_rfft_fast_f32(pFftInstance, pAudioDataNormalized, pFftOutput, 0);

    #ifdef UART_DEBUG_ARRAYS
    logFftOutput(pFftOutput, AUDIO_DATA_LEN);
    #endif // UART_DEBUG_ARRAYS

    arm_cmplx_mag_squared_f32(pFftOutput, pFftOutputMag, AUDIO_DATA_LEN / 2);

    #ifdef UART_DEBUG_ARRAYS
    logFftOutputMag(pFftOutputMag, AUDIO_DATA_LEN);
    #endif // UART_DEBUG_ARRAYS
}

float32_t calculateFreqFromFftIndex(const uint16_t size, const float32_t sampling_freq, const uint16_t idx)
{
    float32_t frequency = 0.0f;
    if (idx < size)
    {
        frequency = (float32_t)idx * sampling_freq / (float32_t)size;
    }
    #ifdef UART_DEBUG
    else
    {
        uartPrintf("Error: index is out of range\n\n\r");
    }
    #endif // UART_DEBUG
    return frequency;
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
    uartPrintf("Idx: %u \t\tMax Frequency: %f\n\r", maxMagIdx, maxMagFreq);
    detectNote(maxMagFreq);
    #endif // UART_LOG
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
    const float32_t ADC_CENTER = ADC_MAX / 2.0f; // 2047.5
    const float32_t ADC_SCALE = ADC_CENTER; // 2047.5

    for (size_t i = 0; i < len; i++)
    {
        const uint16_t adc_value = src[i] & ADC_MAX;
        dst[i] = ((float)adc_value - ADC_CENTER) / ADC_SCALE;

        #ifdef UART_DEBUG_ARRAYS
        if (i % 256 == 0)
        {
            uartPrintf("src[%*u] = %u;\tdst = %.4f\r\n", 4, i, adc_value, dst[i]);
        }
        #endif // UART_DEBUG_ARRAYS
    }
    #ifdef UART_DEBUG_ARRAYS
    uartPrintf("\r\n");
    #endif // UART_DEBUG_ARRAYS
}
