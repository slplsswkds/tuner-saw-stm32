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