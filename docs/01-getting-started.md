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
2. Loads saved settings
3. Initializes audio output
4. Loads the built-in drum/sample set
5. Shows the main sequencer screen

## Good To Know

- Sample content is embedded in firmware.
- You do not need to upload `/data/samples` for normal use.
- If `TEST_TONE` is enabled, you will hear a test sine tone instead of drums.
