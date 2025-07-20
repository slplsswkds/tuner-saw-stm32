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
void fft(const arm_rfft_fast_instance_f32* pFftInstance, const uint16_t* pAudioData, float32_t* pFftOutput);
void calculateStringTuningInfo();
void showInfo();
void convert_uint16_to_float32(const uint16_t* src, float* dst, size_t len);

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

    uint16_t pAudioData[AUDIO_DATA_LEN];
    float32_t pFftOutput[AUDIO_DATA_LEN];

    arm_rfft_fast_instance_f32 fftInstance;
    arm_rfft_fast_init_f32(&fftInstance, AUDIO_DATA_LEN);

    while (1)
    {
        startAdcDataRecording((uint32_t*)pAudioData);
        waitForAdcData();
        fft(&fftInstance, pAudioData, pFftOutput);
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
    AUDIO_DATA_IS_ACTUAL = false;
    HAL_ADC_Start_DMA(&hadc1, pData, 1024);
}

void waitForAdcData()
{
    HAL_SuspendTick();
    while (!AUDIO_DATA_IS_ACTUAL)
    {
        HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
    }
    HAL_ResumeTick();

    snprintf((char*)UART_TX_DATA, sizeof(UART_TX_DATA), "Audio data is actual %d\n\r", AUDIO_DATA_IS_ACTUAL);
    sendUartStr(UART_TX_DATA);
}

void fft(const arm_rfft_fast_instance_f32* pFftInstance, const uint16_t* pAudioData, float32_t* pFftOutput)
{
    if (AUDIO_DATA_IS_ACTUAL == true)
    {
        snprintf((char*)UART_TX_DATA, sizeof(UART_TX_DATA), "INFO: Audio data is actual. Processing fft...\n\r");
        sendUartStr(UART_TX_DATA);
    }
    else
    {
        snprintf((char*)UART_TX_DATA, sizeof(UART_TX_DATA), "ERROR: Audio data is not actual! Returning...\n\r");
        sendUartStr(UART_TX_DATA);
        return;
    }

    float32_t pFftInput[AUDIO_DATA_LEN];
    convert_uint16_to_float32(pAudioData, pFftInput, AUDIO_DATA_LEN);
    arm_rfft_fast_f32(pFftInstance, pFftInput, pFftOutput, 0);
    // arm_cmplx_mag_f32(fftOutput, fftOutputMag, AUDIO_DATA_LEN / 2);
}

void calculateStringTuningInfo()
{
}

void showInfo()
{
    snprintf((char*)UART_TX_DATA, sizeof(UART_TX_DATA), "Tuning info: empty\n\r");
    sendUartStr(UART_TX_DATA);
}

void sendUartStr(const uint8_t* str)
{
    static uint8_t internalUartTxData[sizeof(UART_TX_DATA)];
    while (UART_TX_BUSY) { __NOP(); }
    memccpy(internalUartTxData, str, '\0', sizeof(internalUartTxData));
    UART_TX_BUSY = true;
    HAL_UART_Transmit_DMA(&huart1, internalUartTxData, strlen((char*)str));
}

void convert_uint16_to_float32(const uint16_t* src, float* dst, const size_t len)
{
    const float scale = 1.0f / 2048.0f; // normalization [-1.0..+1.0]
    for (size_t i = 0; i < len; i++)
    {
        dst[i] = ((float)src[i] - 2048.0f) * scale;
    }
}
