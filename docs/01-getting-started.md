# 1. Getting Started (R3)

## What You Need

- ESP32 Groovebox R3 hardware
- ST7789 display (320x240)
- Rotary encoder with push button
- KEY0 button
- I2S DAC module (for example MAX98357A or PCM5102A)
- USB cable
- PlatformIO on your computer

## Build and Flash

1. Open this project in PlatformIO.
2. Select environment `ESP32GrooveboxR3`.
3. Build and upload firmware.
4. Open serial monitor at 115200 baud.

## First Boot

At startup the Groovebox:

1. Starts the screen and controls
2. Initializes SD card and prints the SD root listing
3. Loads drum/sample tracks from SD card
4. Loads saved settings and pattern storage
5. Initializes audio output and shows the main sequencer screen

## Good To Know

- Samples are loaded from SD card files in `/samples`.
- Required files are: `kick.wav`, `snare.wav`, `ch.wav`, `oh.wav`, `tone.wav`, `metal.wav`.
- If a sample is missing, invalid, or too large for available memory, that track uses a fallback waveform.
- If `TEST_TONE` is enabled, you will hear a test sine tone instead of drums.
