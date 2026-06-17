# ESP32S3 MicroCycles Groovebox R3
# Developer Build Guide

**Current firmware version:** `v1.3.5`  
**Hardware platform:** `TFT_LCD_Display_EC11` with ESP32-S3 piggy-back board  
**Target board class:** ESP32-S3 N8R8, native USB, external I2S DAC, ST7789 TFT, SD card over dedicated SPI

This guide is the main developer document for rebuilding a compatible Groovebox from zero. It describes the hardware, firmware structure, runtime model, storage model, build environment, and the role of every `.cpp` source file.

---

## Table of contents

- [`src/main.cpp`](main.md)
- [`src/DisplayDriverClass.cpp`](DisplayDriverClass.md)
- [`src/InputClass.cpp`](InputClass.md)
- [`src/WiFiManagerExtClass.cpp`](WiFiManagerExtClass.md)
- [`src/audioEngine.cpp`](audioEngine.md)
- [`src/sampleManager.cpp`](sampleManager.md)
- [`src/sequencer.cpp`](sequencer.md)
- [`src/settingsStore.cpp`](settingsStore.md)
- [`src/systemManager.cpp`](systemManager.md)
- [`src/uiManager.cpp`](uiManager.md)
- [`src/uiGrooveboxScreen.cpp`](uiGrooveboxScreen.md)
- [`src/uiSystemSettingsMenu.cpp`](uiSystemSettingsMenu.md)
- [`src/uiCardStorageMenu.cpp`](uiCardStorageMenu.md)
- [`src/uiCardStorageActions.cpp`](uiCardStorageActions.md)
- [`src/uiPatternGroupInput.cpp`](uiPatternGroupInput.md)
- [`src/uiSequencerInput.cpp`](uiSequencerInput.md)

---

## 1. Project Goal

The project implements a compact six-track sample groovebox for the ESP32-S3 platform. It is designed as a realtime musical instrument, not as a general-purpose menu application.

The system provides:

```text
six sample tracks
16-step pattern editing
multiple pattern slots in RAM
SD-card pattern groups
SD-card sample sets
per-sample gain files
chain playback
runtime sample-set switching
TFT boot diagnostics
I2S audio output
WiFi manager portal support
NVS/LittleFS runtime settings
```

The preferred user workflow is:

```text
Boot
  -> initialize display
  -> initialize SD card
  -> load active sample set
  -> initialize input, sequencer, audio and WiFi system manager
  -> load active pattern group into RAM
  -> edit/play in RAM
  -> Save Group writes active RAM patterns back to SD card
```

---

## 2. Hardware Platform

Required hardware:

```text
ESP32-S3 N8R8 module or board
TFT_LCD_Display_EC11 carrier/front-panel board
ESP32-S3 piggy-back board
ST7789 320x240 TFT LCD
EC11 rotary encoder with push button
KEY0 auxiliary button
microSD card socket with card-detect pin
I2S DAC module, such as PCM5102-compatible module
USB-C native programming/serial connection
```

Important ESP32-S3 GPIO notes:

```text
GPIO22-GPIO25 do not exist on ESP32-S3.
GPIO26-GPIO37 are generally flash/PSRAM related and must be avoided.
GPIO45 and GPIO46 are strapping/input-special pins and must be avoided for normal output.
Native USB uses GPIO19/GPIO20.
```

---

## 3. Pin Mapping

The active pin mapping is defined in `platformio.ini`.

### TFT LCD

```text
PIN_TFT_BLK  = GPIO2
PIN_TFT_RST  = GPIO4
PIN_TFT_CS   = GPIO5
PIN_TFT_SCLK = GPIO12
PIN_TFT_MOSI = GPIO11
PIN_TFT_DC   = GPIO15
```

The TFT uses a dedicated SPI object, independent of the SD card bus.

### Encoder and Button

```text
PIN_ENC_BTN = GPIO6
PIN_KEY0    = GPIO1
PIN_ENC_A   = GPIO16
PIN_ENC_B   = GPIO17
```

### I2S Audio

```text
PIN_I2S_BCLK = GPIO38
PIN_I2S_WS   = GPIO40
PIN_I2S_DOUT = GPIO42
PIN_I2S_SD   = GPIO41
```

The current audio path outputs standard I2S. A PCM5102-style DAC must be configured for I2S format, not Left Justified.

### SD Card

```text
PIN_SD_DTCT              = GPIO7
PIN_SD_DTCT_ENABLED      = 1
PIN_SD_DTCT_NO_CARD_LEVEL = HIGH
PIN_SD_CS                = GPIO13
PIN_SD_SCK               = GPIO14
PIN_SD_MISO              = GPIO18
PIN_SD_MOSI              = GPIO21
```

The SD card uses a dedicated SPI object, independent of the TFT bus. SD card presence is checked before SD-dependent boot and UI operations.

---

## 4. SPI Architecture

Older versions shared one global `SPI` object between TFT and SD. Current firmware does not.

Current architecture:

```text
TFT -> dedicated SPIClass tftSpi(TFT_SPI_HOST)
SD  -> dedicated SPIClass sdSpi(SD_SPI_HOST)
```

This is essential for the ESP32-S3 piggy-back board where TFT and SD are physically wired to separate SPI buses.

Do not reintroduce global `SPI.begin(...)` for either display or SD card.

---

## 5. PlatformIO Environment

The main build environment is:

```text
ESP32S3GrooveboxR3
```

Build:

```bash
pio run -e ESP32S3GrooveboxR3
```

Upload:

```bash
pio run -e ESP32S3GrooveboxR3 -t upload
```

Serial monitor:

```bash
pio device monitor -e ESP32S3GrooveboxR3
```

Important ESP32-S3 N8R8 settings:

```ini
board = esp32-s3-devkitc-1
board_build.arduino.memory_type = qio_opi
board_build.flash_mode = qio
board_build.psram_type = opi
board_upload.flash_size = 8MB
board_build.psram = enabled
-D BOARD_HAS_PSRAM
```

The partition file keeps LittleFS small and gives most of the 8 MB flash to the application image.

---

## 6. Required Libraries

`platformio.ini` declares:

```text
Adafruit GFX Library
Adafruit ST7735 and ST7789 Library
ArduinoJson
WiFiManager
```

The firmware also uses ESP-IDF/Arduino core services:

```text
FreeRTOS tasks
esp_timer
NVS / Preferences
LittleFS
SD / SPI
WiFi
I2S legacy driver
heap_caps PSRAM allocation
```

---

## 7. SD Card Layout

The SD card must be FAT-formatted and contain:

```text
/
  /patterns
    /DEMO
      p01.json
      p02.json
      ...

  /samples
    /S1
      kick.wav
      snare.wav
      ch.wav
      oh.wav
      tone.wav
      metal.wav
      setGain.json

    /S2
      ...
```

Pattern groups are directories under `/patterns`.

Sample sets are directories named `S1` through `S9` under `/samples`.

---

## 8. WAV Sample Requirements

Each sample set should contain:

```text
kick.wav
snare.wav
ch.wav
oh.wav
tone.wav
metal.wav
```

Supported audio properties:

```text
WAV container
PCM audio format
44.1 kHz sample rate
16-bit or 24-bit input
mono or stereo input
```

The loader converts samples into internal mono `int16_t` buffers. PSRAM is preferred when available. If a sample is missing, invalid or too large for available memory, a generated fallback waveform is used.

---

## 9. setGain.json

A sample set may include `setGain.json`:

```json
{
  "setGain": {
    "kick": 100,
    "snare": 90,
    "ch": 70,
    "oh": 80,
    "tone": 100,
    "metal": 85
  }
}
```

The current loader uses the mainstream `setGain` object format. Missing values default to 100 percent.

---

## 10. Pattern Storage Model

The Groovebox edits patterns in RAM.

SD card storage is explicit:

```text
Load Group -> read all pNN.json files from /patterns/<GROUP>
Save Group -> write RAM slots back to /patterns/<GROUP>
Copy Group -> duplicate one SD group directory
Rename Group -> rename one SD group directory
Delete Group -> delete one SD group directory, except the active group is protected
```

The active group and active sample set names are persisted in NVS.

---

## 11. Pattern JSON Model

A pattern JSON file stores:

```text
version
name
bpm
swing
chainEnabled
chainLength
chainTarget
masterLevel
tracks
```

Each track stores:

```text
trigger data
velocity
probability
pitch lock
decay lock
mute state
```

Pattern filenames are strict `pNN.json` names. Do not reintroduce legacy pattern filenames or extensionless files.

---

## 12. Boot Sequence

Current high-level boot sequence:

```text
Serial start
log firmware version and pins
check pin conflicts
load runtime settings
initialize TFT immediately
show boot log on TFT
initialize SD card and load samples
initialize input
initialize sequencer
initialize audio engine
initialize WiFi/system manager
initialize UI manager
load active card pattern group
start AudioTask
start UiTask
start InputTask
start SystemTask
loop() remains fallback-only
```

The boot log is rendered directly on the TFT using partial row updates. Warnings and errors use severity-colored rows and automatic delay.

---

## 13. Tasks and Timing

The firmware uses four main FreeRTOS tasks.

### AudioTask

Runs on core 0 and is timing-sensitive.

```text
sequencerConsumeDueStep()
audioEngineTriggerSample()
audioEngineRenderBlock()
```

It must not allocate memory, access SD, draw UI, or do blocking work except its fixed tick delay.

### UiTask

Consumes queued encoder/button events and redraws UI.

### InputTask

Polls EC11 encoder and KEY0, converts hardware transitions into logical events, and sends them to `UiTask`.

### SystemTask

Runs WiFi manager/system commands.

### loop()

Only fallback logic. It must remain minimal.

---

## 14. UI Philosophy

The UI should behave like a dedicated musical instrument:

```text
fast
predictable
muscle-memory friendly
minimum modal confusion
safe while playing
```

Avoid desktop-style workflows, deep menus, and unnecessary full-screen redraws during playback.

---

## 15. Main Screen

The main `[Groovebox]` screen shows:

```text
header with version/status
six track rows
current parameter/edit page
active/playing pattern information
chain status and group name
```

Tracks:

```text
KICK
SNARE
CH
OH
TONE
METAL
```

Main parameter pages include:

```text
TRIG
VEL
PITCH
DECAY
PROB
MUTE
CHAIN
MASTER
```

---

## 16. Controls

Normal mode:

```text
Encoder rotate       -> move track / move across pattern edge
Encoder short press  -> enter/edit current page
Encoder medium press -> tempo/master related edit popup
Encoder long press   -> System Settings
KEY0 short           -> play/stop
KEY0 medium/long     -> alternate tempo/edit shortcuts depending on UI state
```

Input routing is implemented as a finite-state dispatcher in `uiManager.cpp`.

---

## 17. Audio Architecture

Audio path:

```text
Sequencer due step
  -> track trigger mask
  -> audioEngineTriggerSample()
  -> fixed voice pool
  -> per-sample gain
  -> velocity curve
  -> pitch phase increment
  -> decay frame limit
  -> attack/release fade
  -> pan and mix
  -> master gain
  -> limiter/headroom clamp
  -> stereo I2S DMA buffer
  -> I2S DAC
```

Current output is stereo interleaved, with track pan support in the mixer.

---

## 18. Diagnostics

Useful build flags:

```text
TEST_TONE
TEST_TONE_FREQUENCY_HZ
SD_SMOKE_TEST
NO_DAC_HARDWARE
DISPLAY_DEBUG_INFO
SD_VERBOSE_DIRECTORY_LISTING
CORE_DEBUG_LEVEL
LOG_LOCAL_LEVEL
```

For normal firmware builds, keep diagnostic modes disabled.

---

## 19. Development Rules

Recommended code style:

```text
Allman braces
2-space indentation
lowerCamelCase for functions and variables
English comments
comments above the relevant code
setup() and loop() at the bottom of main.cpp
loop() delegates only
no filesystem/display/WiFi calls from realtime audio rendering
```

Logging prefixes:

```text
Error:
Warning:
Info:
Debug:
```

Use `ESP_LOGx()` for ESP32 diagnostics.

---

## 20. Building a Compatible Groovebox From Zero

A developer starting from zero should implement in this order:

```text
1. appConfig/platformio pin map and partition table
2. Display driver with ST7789 and boot log
3. Input scanner for EC11 and KEY0
4. SD card and sample manager
5. Audio engine and I2S DAC output
6. Sequencer core and RAM pattern model
7. Settings store and SD pattern JSON format
8. Groovebox UI rendering
9. UI manager finite-state routing
10. Card storage workflows
11. WiFi/system manager
12. Diagnostics, boot messages and safety guards
```

Use the individual source-file documents below as the implementation map.

- [`UP`](../developerBuildGuide.md) | [`README`](../README.md)
