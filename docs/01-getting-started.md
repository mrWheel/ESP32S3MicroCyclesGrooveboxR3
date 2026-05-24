# 1. Getting Started

## 1.1 What You Need

- ESP32 board target: **ESP32-WROVER** (with PSRAM recommended)
- TFT_LCD_DISPLAY_EC11_BOARD-compatible wiring
- ST7789 display (320x240)
- Rotary encoder with pushbutton
- KEY0 button
- Optional I2S DAC (MAX98357A or PCM5102A)
- USB cable and host computer with PlatformIO

## 1.2 Build Environment

Project configuration is defined in:

- `platformio.ini`

Main environment:

- `env:ESP32Groovebox`
- Framework: Arduino
- Platform: espressif32

## 1.3 First Flash

1. Build and flash the firmware with PlatformIO.
2. Upload the `data/` folder to LittleFS (contains default sample files).
3. Open serial monitor at 115200 baud.
4. Confirm boot log reports Groovebox startup and pin mapping.

## 1.4 First Boot Behavior

On boot, the firmware:

1. Initializes input and display
2. Initializes sequencer state
3. Mounts LittleFS and loads six sample slots
4. Loads persisted runtime settings (`/settings.json`) if available
5. Initializes audio engine
6. Initializes system manager (WiFi disabled by default at startup)
7. Starts FreeRTOS tasks

## 1.5 No DAC Hardware Mode

If `NO_DAC_HARDWARE` is enabled in build flags:

- I2S/DAC init is skipped
- System still runs UI and sequencer logic
- Useful for control/UI testing without audio output hardware
