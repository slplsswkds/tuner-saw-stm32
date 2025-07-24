#pragma once

#include <arm_math.h>
#include <stdbool.h>
#include <stdint.h>

extern const uint32_t AUDIO_DATA_LEN;
extern const float32_t ADC_SAMPLING_FREQ;
extern const float32_t ADC_SAMPLING_RATE;
extern volatile bool AUDIO_DATA_IS_ACTUAL;

void startAdcDataRecording(uint16_t* pData, uint32_t length);
void waitForAdcData();
