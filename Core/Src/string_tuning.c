#include "string_tuning.h"
#include "ssd1306.h"

const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

const float32_t REFERENCE_FREQUENCY = 440.0f; // Частота A4
const uint8_t REFERENCE_MIDI_NUMBER = 69; // MIDI номер A4
const uint8_t SEMITONES_PER_OCTAVE = 12; // Півтонів в октаві
const float32_t ROUNDING_OFFSET = 0.5f; // Для округлення
const uint8_t MIDI_OCTAVE_OFFSET = 1; // Для обчислення правильної октави
const float32_t CENTS_TOLERANCE = 5.0f; // Допустиме відхилення в центах

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
        oledPrintf(0, 0, Font_7x10, "Invalid");
        return;
    }

    const float32_t noteNumber = calculateNoteNumber(frequency); // Calculating the MIDI number of the nearest note
    const uint8_t roundedNoteNumber = calculateRoundedNoteNumber(noteNumber);
    const uint8_t noteIndex = calculateNoteIndex(roundedNoteNumber);
    const uint8_t octave = calculateNoteOctave(roundedNoteNumber);
    const float32_t idealFrequency = calculateIdealFrequency(roundedNoteNumber); // Frequency for reference note
    const float32_t centsDiff = calculateCentsDiff(frequency, idealFrequency); // Calculating the difference in cents

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
