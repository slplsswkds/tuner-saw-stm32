#pragma once
#include <arm_math.h>

typedef enum
{
    LOW,
    HIGH,
    OK,
    UNKNOWN,
} StringTension;

void detectNote(float32_t frequency);
float32_t calculateNoteNumber(float32_t frequency);
uint8_t calculateRoundedNoteNumber(float32_t noteNumber);
uint8_t calculateNoteIndex(uint8_t roundedNoteNumber);
uint8_t calculateNoteOctave(uint8_t roundedNoteNumber);
float32_t calculateFreqFromFftIndex(uint16_t size, float32_t sampling_freq, uint16_t idx);
float32_t findDominantFrequency(const float32_t* pFftMag, uint16_t size);
void calculateStringTuningInfo(const float32_t* pFftMag, uint16_t size);
