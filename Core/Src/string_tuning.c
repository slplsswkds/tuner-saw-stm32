#include "string_tuning.h"
#include "adc_data.h"
#include "ssd1306.h"
#include "uart_log.h"

/*
 * Standard guitar tuning (EADGBE):
 *
 * E: E2 = 82.41 Hz.
 * A: A2 = 110 Hz.
 * D: D3 = 146.83 Hz.
 * G: G3 = 196 Hz.
 * B: B3 = 246.94 Hz.
 * E: E4 = 329.63 Hz.
 */

const char* semitoneNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

const float32_t REFERENCE_FREQUENCY = 440.0f; // Frequency of the A4 note
const uint8_t REFERENCE_MIDI_NUMBER = 69; // MIDI number corresponding to A4
const uint8_t SEMITONES_PER_OCTAVE = 12; // Number of semitones in one octave
const float32_t ROUNDING_OFFSET = 0.5f; // Offset used for rounding to nearest integer
const uint8_t MIDI_OCTAVE_OFFSET = 1; // Offset to compute the correct octave number
const float32_t CENTS_TOLERANCE = 5.0f; // Acceptable deviation in cents for tuning precision

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

inline float32_t calculateIdealFrequency(const uint8_t roundedNoteNumber)
{
    return REFERENCE_FREQUENCY * powf(
        2.0f, ((float32_t)roundedNoteNumber - (float32_t)REFERENCE_MIDI_NUMBER) / (float32_t)SEMITONES_PER_OCTAVE);
}

inline float32_t calculateCentsDiff(const float32_t frequency, const float32_t idealFrequency)
{
    return 1200.0f * log2f(frequency / idealFrequency);
}

void detectNote(const float32_t frequency)
{
    if (frequency <= 0.0f)
    {
        #ifdef UART_LOG
        uartPrintf("Invalid frequency\n\r");
        #endif // UART_LOG
        oledPrintf(0, 0, Font_11x18, "NoData");
        ssd1306_DrawHorizontalLine(0, 20, (int16_t)ssd1306_GetWidth());
        oledPrintf(0, 22, Font_11x18, "<= 0Hz");
        return;
    }

    const float32_t noteNumber = calculateNoteNumber(frequency); // Calculating the MIDI number of the nearest note

    if (noteNumber < 21) // A0
    {
        oledPrintf(0, 0, Font_11x18, "f < A0");
        oledPrintf(0, 30, Font_7x10, "%7.3f Hz", frequency);
        return;
    }
    if (noteNumber > 108) // C8
    {
        oledPrintf(0, 0, Font_11x18, "f > C8");
        oledPrintf(0, 30, Font_7x10, "%7.3f Hz", frequency);
        return;
    }

    const uint8_t roundedSemitoneNumber = calculateRoundedNoteNumber(noteNumber);
    const uint8_t nearestSemitoneIndex = calculateNoteIndex(roundedSemitoneNumber);
    const uint8_t octave = calculateNoteOctave(roundedSemitoneNumber);
    const float32_t idealFrequency = calculateIdealFrequency(roundedSemitoneNumber); // Frequency for reference note
    const float32_t centsDiff = calculateCentsDiff(frequency, idealFrequency); // Calculating the difference in cents

    #ifdef UART_LOG
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
        uartPrintf("✔ In tune\n\r");
        break;
    case LOW:
        uartPrintf("↓ Too low — tighten the string\n\r");
        break;
    case HIGH:
        uartPrintf("↑ Too high — loosen the string\n\r");
        break;
    default: ;
    }

    uartPrintf("Note: %s%d (MIDI %d)\n\r", noteNames[noteIndex], octave, roundedNoteNumber);
    uartPrintf("Detected freq: %.2f Hz\tIdeal freq: %.2f Hz\n\r", frequency, idealFrequency);
    uartPrintf("Diff: %.2f cents\n\r", centsDiff);
    uartPrintf("\n\r");
    #endif // UART_LOG

    const uint8_t nextSemitoneIndex = (nearestSemitoneIndex + 1) % SEMITONES_PER_OCTAVE;
    const uint8_t prevSemitoneIndex = (nearestSemitoneIndex + SEMITONES_PER_OCTAVE - 1) % SEMITONES_PER_OCTAVE;

    const uint8_t strLenPrevSemitone = strlen(semitoneNames[prevSemitoneIndex]) + 1;
    const uint8_t currentSemitoneCoordinateX = strLenPrevSemitone * Font_7x10.FontWidth + 3;

    const uint8_t strLenNextSemitone = strlen(semitoneNames[nextSemitoneIndex]) + 1;
    const uint8_t nextSemitoneCoordinateX = SSD1306_WIDTH - strLenNextSemitone * Font_7x10.FontWidth - Font_7x10.FontWidth;

    oledPrintf(currentSemitoneCoordinateX, 0, Font_11x18, "%s%d", semitoneNames[nearestSemitoneIndex], octave);
    oledPrintf(0, 0, Font_7x10, "%s%d", semitoneNames[prevSemitoneIndex], octave);
    oledPrintf(nextSemitoneCoordinateX, 0, Font_7x10, "%s%d", semitoneNames[nextSemitoneIndex], octave);
    oledPrintf(0, 22, Font_11x18, "%.2f", centsDiff);
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
    arm_max_f32(pFftMag, size, &maxMag, (uint32_t*)&maxMagIdx);
    const float32_t maxMagFreq = (float32_t)maxMagIdx * ADC_SAMPLING_FREQ / (float32_t)size;

    #ifdef UART_LOG
    uartPrintf("Idx: %u \t\tMax Frequency: %f\n\r", maxMagIdx, maxMagFreq);
    #endif // UART_LOG
    detectNote(maxMagFreq);
}
