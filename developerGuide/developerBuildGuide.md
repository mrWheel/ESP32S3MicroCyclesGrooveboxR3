# ESP32S3 MicroCycles Groovebox R3 — Developer Build Guide

**Current firmware version:** `v1.3.9`  
**Hardware platform:** `TFT_LCD_Display_EC11` with ESP32-S3 piggy-back board  
**Target board class:** ESP32-S3 N8R8, native USB, external I2S DAC, ST7789 TFT, SD card over dedicated SPI

This guide is the main developer document for rebuilding a compatible Groovebox from zero. It describes the hardware, firmware architecture, runtime model, storage model, build environment, and the role of every `.cpp` source file.
---

## Source File Reference

| File | Purpose |
|------|---------|
| [main.md](main.md#src-main-cpp) | Firmware entry point, task orchestration, boot order |
| [DisplayDriverClass.md](DisplayDriverClass.md) | TFT driver, text rendering, UI layout engine |
| [InputClass.md](InputClass.md) | Encoder and button input handling |
| [WiFiManagerExtClass.md](WiFiManagerExtClass.md) | WiFiManager wrapper and captive portal |
| [webServerManager.md](webServerManager.md) | HTTP server foundation for future SPA/API access |
| [audioEngine.md](audioEngine.md) | I2S output, voice pool, synthesis, mixing |
| [sampleManager.md](sampleManager.md) | SD card, WAV loading, memory allocation |
| [sequencer.md](sequencer.md) | Pattern sequencing, step timing, BPM control |
| [settingsStore.md](settingsStore.md) | NVS/LittleFS persistence, JSON pattern I/O |
| [systemManager.md](systemManager.md) | WiFi lifecycle, NVS reconnect, system commands |
| [uiManager.md](uiManager.md) | UI state machine, input routing, screen dispatch |
| [uiGrooveboxScreen.md](uiGrooveboxScreen.md) | Sequencer screen layout and rendering |
| [uiSystemSettingsMenu.md](uiSystemSettingsMenu.md) | System settings menu (theme, rotation, etc.) |
| [uiCardStorageMenu.md](uiCardStorageMenu.md) | Card storage menu (save/load/copy/etc.) |
| [uiCardStorageActions.md](uiCardStorageActions.md) | Pattern group I/O delegation |
| [uiPatternGroupInput.md](uiPatternGroupInput.md) | Text input for pattern group names |
| [uiSequencerInput.md](uiSequencerInput.md) | Common UI list navigation utilities |

---

## Table of Contents

- [1. Project Goal](#1-project-goal)
- [2. Hardware Platform](#2-hardware-platform)
- [3. Pin Mapping](#3-pin-mapping)
- [4. SPI Architecture](#4-spi-architecture)
- [5. PlatformIO Environment](#5-platformio-environment)
- [6. Required Libraries](#6-required-libraries)
- [7. SD Card Layout](#7-sd-card-layout)
- [8. WAV Sample Requirements](#8-wav-sample-requirements)
- [9. setGain.json](#9-setgainjson)
- [10. Pattern Storage Model](#10-pattern-storage-model)
- [11. Pattern JSON Model](#11-pattern-json-model)
- [12. Boot Sequence](#12-boot-sequence)
- [13. Tasks and Timing](#13-tasks-and-timing)
- [14. UI Philosophy](#14-ui-philosophy)
- [15. Main Screen Layout](#15-main-screen-layout)
- [16. Controls and Input Routing](#16-controls-and-input-routing)
- [17. Audio Architecture](#17-audio-architecture)
- [18. Development Rules](#18-development-rules)
- [19. Building From Zero](#19-building-from-zero)
- [20. Source File Reference](#20-source-file-reference)

---

## 1. Project Goal

The project implements a compact six-track sample groovebox for the ESP32-S3 platform, designed as a realtime musical instrument rather than a general-purpose menu application.

**Capabilities:**

- Six independent sample tracks with per-track controls
- 16-step pattern editing with variable chain length
- Multiple pattern slots in RAM (up to 64 patterns)
- SD-card pattern groups for persistent storage
- SD-card sample sets (S1–S9) for swapping sample palettes
- Per-sample gain profiles via `setGain.json`
- Chain playback for pattern sequencing
- Runtime sample-set switching without reboot
- TFT boot diagnostics with real-time status logging
- I2S stereo audio output with sample-accurate pitch and decay control
- WiFi manager portal for credential entry; ESP32 WiFi NVS stores credentials
- NVS/LittleFS runtime settings persistence

**Preferred user workflow:**

```
Boot
  → initialize display with boot log
  → initialize SD card
  → load active sample set
  → initialize input, sequencer, audio and WiFi system
  → load active pattern group into RAM
  → edit/play in RAM
  → Save Group writes active RAM patterns back to SD card
```

**[⬆ UP](#table-of-contents) | [📖 README](../README.md#)**

---

## 2. Hardware Platform

**Required hardware:**

- ESP32-S3 N8R8 module (8 MB flash, 8 MB PSRAM, native USB)
- TFT_LCD_Display_EC11 carrier/front-panel PCB
- ESP32-S3 piggy-back adapter board
- ST7789 320×240 TFT LCD display
- EC11 rotary encoder with integrated push button
- KEY0 auxiliary momentary button
- microSD card socket with card-detect pin
- I2S DAC module (e.g., PCM5102-compatible)
- USB-C for programming and serial debugging

**Important ESP32-S3 GPIO notes:**

- GPIO 22–25: Do not exist on ESP32-S3
- GPIO 26–37: Generally reserved for flash/PSRAM; avoid for normal I/O
- GPIO 45 & 46: Strapping pins; avoid for normal output
- GPIO 19 & 20: Used by native USB; avoid unless repurposing USB

**[⬆ UP](#table-of-contents) | [📖 README](../README.md#)**

---

## 3. Pin Mapping

The active pin mapping is defined in `platformio.ini` via `-DPIN_*` build flags.

### TFT LCD (SPI1)

```
PIN_TFT_BLK  = GPIO2   (backlight enable, PWM capable)
PIN_TFT_RST  = GPIO4   (reset)
PIN_TFT_CS   = GPIO5   (chip select)
PIN_TFT_SCLK = GPIO12  (clock)
PIN_TFT_MOSI = GPIO11  (MOSI)
PIN_TFT_DC   = GPIO15  (data/command)
```

TFT uses a dedicated `SPIClass(TFT_SPI_HOST)`, isolated from SD card.

### Encoder and Button

```
PIN_ENC_BTN = GPIO6   (encoder push button)
PIN_KEY0    = GPIO1   (auxiliary button)
PIN_ENC_A   = GPIO16  (quadrature A)
PIN_ENC_B   = GPIO17  (quadrature B)
```

### I2S Audio (Stereo)

```
PIN_I2S_BCLK = GPIO38  (bit clock)
PIN_I2S_WS   = GPIO40  (word select / LRCK)
PIN_I2S_DOUT = GPIO42  (data out)
PIN_I2S_SD   = GPIO41  (serial data, usually tied to DOUT or GND)
```

The DAC must be configured for **standard I2S**, not Left Justified or other variants.

### SD Card (SPI2)

```
PIN_SD_DTCT              = GPIO7   (card detect line)
PIN_SD_DTCT_ENABLED      = 1       (detection enabled)
PIN_SD_DTCT_NO_CARD_LEVEL = HIGH   (high = no card)
PIN_SD_CS                = GPIO13  (chip select)
PIN_SD_SCK               = GPIO14  (clock)
PIN_SD_MISO              = GPIO18  (MISO)
PIN_SD_MOSI              = GPIO21  (MOSI)
```

SD card uses a dedicated `SPIClass(SD_SPI_HOST)`, isolated from TFT. Card presence is verified via `sampleManagerIsSdCardReady()` before SD-dependent operations.

**[⬆ UP](#table-of-contents) | [📖 README](../README.md#)**

---

## 4. SPI Architecture

Older firmware versions shared one global SPI object between TFT and SD, causing contention issues. **Current firmware does not.**

**Current architecture:**

```
TFT → dedicated SPIClass(TFT_SPI_HOST)
SD  → dedicated SPIClass(SD_SPI_HOST)
```

This separation is essential for the ESP32-S3 piggy-back board, where TFT and SD are wired to physically distinct SPI buses.

**Do not reintroduce global `SPI.begin(...)` for either display or SD card.** Each module initializes its own SPIClass independently during setup.

**[⬆ UP](#table-of-contents) | [📖 README](../README.md#)**

---

## 5. PlatformIO Environment

The main build environment is `ESP32S3GrooveboxR3`.

**Build:**

```bash
pio run -e ESP32S3GrooveboxR3
```

**Upload:**

```bash
pio run -e ESP32S3GrooveboxR3 -t upload
```

**Serial monitor:**

```bash
pio device monitor -e ESP32S3GrooveboxR3 -b 115200
```

**Key esp32-s3-devkitc-1 settings in platformio.ini:**

```ini
board = esp32-s3-devkitc-1
board_build.arduino.memory_type = qio_opi
board_build.flash_mode = qio
board_build.psram_type = opi
board_upload.flash_size = 8MB
board_build.psram = enabled
-D BOARD_HAS_PSRAM
```

The partition file reserves most of the 8 MB flash for the application image while keeping LittleFS small.

**[⬆ UP](#table-of-contents) | [📖 README](../README.md#)**

---

## 6. Required Libraries

**PlatformIO dependencies:**

- `Adafruit GFX Library` — bitmap graphics primitives
- `Adafruit ST7735 and ST7789 Library` — TFT driver
- `ArduinoJson` — JSON parsing and serialization
- `WiFiManager` — WiFi credential portal

**ESP-IDF/Arduino core services:**

- FreeRTOS (multi-core tasks, mutexes, queues)
- esp_timer (microsecond timing for audio)
- Preferences / NVS (non-volatile storage)
- LittleFS (flash filesystem for runtime settings)
- SD / SPI (SD card via dedicated SPI bus)
- WiFi (native ESP32 WiFi stack)
- I2S (legacy I2S TX driver for audio output)
- heap_caps (PSRAM-aware memory allocation)
- Console / logging (ESP_LOGx macros)

**[⬆ UP](#table-of-contents) | [📖 README](../README.md#)**

---

## 7. SD Card Layout

The SD card must be FAT-formatted with the following structure:

```
/
  /patterns
    /DEMO
      p01.json
      p02.json
      p03.json
      ...
    /MyGroup
      p01.json
      ...

  /samples
    /S1
      kick.wav
      snare.wav
      ch.wav
      oh.wav
      tone.wav
      metal.wav
      setGain.json    (optional)
    
    /S2
      kick.wav
      snare.wav
      ...
```

**Pattern groups** are subdirectories under `/patterns`. Each group contains `p01.json` through `p64.json` (or fewer).

**Sample sets** are subdirectories named `S1` through `S9` under `/samples`. The active sample set is persisted in NVS and restored on boot.

**[⬆ UP](#table-of-contents) | [📖 README](../README.md#)**

---

## 8. WAV Sample Requirements

Each sample set should contain six tracks (kick, snare, ch, oh, tone, metal). Supported properties:

```
Container:    WAV
Codec:        PCM (no compression)
Sample rate:  44.1 kHz
Bit depth:    16-bit or 24-bit
Channels:     mono or stereo
Max duration: typically 1–4 seconds
```

**Processing:**

- The loader converts all input into mono `int16_t` interleaved samples
- PSRAM is preferred for large samples; RAM is used as fallback
- Missing, invalid, or oversized samples generate procedural fallback waveforms (sine/sawtooth)
- Gain is applied before storage via `setGain.json` if present

**[⬆ UP](#table-of-contents) | [📖 README](../README.md#)**

---

## 9. setGain.json

A sample set may include `setGain.json` to define per-sample loudness offset:

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

**Values:** 0–150, where 100 = unity gain (no change). Missing entries default to 100.

**[⬆ UP](#table-of-contents) | [📖 README](../README.md#)**

---

## 10. Pattern Storage Model

The Groovebox edits patterns **in RAM only**. SD card storage is explicit via UI menu:

| Operation   | Description                                                          |
|-------------|----------------------------------------------------------------------|
| Load Group  | Read all `pNN.json` files from `/patterns/<GROUP>` into RAM slots     |
| Save Group  | Write RAM slots back to `/patterns/<GROUP>` (overwrites)             |
| Copy Group  | Duplicate one SD group directory on card                             |
| Rename Group| Rename one SD group directory                                        |
| Delete Group| Delete one SD group directory (active group is protected)            |

The **active group name** and **active sample set** are persisted in NVS and restored on boot.

**[⬆ UP](#table-of-contents) | [📖 README](../README.md#)**

---

## 11. Pattern JSON Model

A pattern JSON file (`pNN.json`) stores:

```json
{
  "version": 1,
  "name": "Groove 1",
  "bpm": 120,
  "swing": 0,
  "chainEnabled": false,
  "chainLength": 1,
  "chainTarget": 1,
  "masterLevel": 90,
  "tracks": [
    {
      "triggers": [1,0,0,0,1,0,0,0,0,1,0,0,1,0,0,0],
      "velocity": [100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100],
      "probability": [100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100],
      "pitchLocked": false,
      "pitchValue": 0,
      "decayLocked": false,
      "decayValue": 100,
      "mute": false
    },
    ...
  ]
}
```

**Fields:**

- `version` — JSON format version (currently 1)
- `name` — User-readable pattern name
- `bpm` — Tempo (typically 60–180 BPM)
- `swing` — Swing percentage (0–100)
- `chainEnabled` — Whether chaining is active
- `chainLength` — Number of patterns in chain (1–16)
- `chainTarget` — Next pattern index (1-based, wraps)
- `masterLevel` — Master output gain (0–100 %)
- `tracks` — Array of six track objects

**Per-track fields:**

- `triggers` — Array of 16 step booleans (0 or 1)
- `velocity` — Array of 16 velocities per step (0–100)
- `probability` — Array of 16 probability percentages (0–100)
- `pitchLocked` — Whether pitch is fixed for this track
- `pitchValue` — Pitch offset in semitones (−12 to +12)
- `decayLocked` — Whether decay is fixed for this track
- `decayValue` — Decay time in percent (0–100)
- `mute` — Track mute flag (boolean)
- Per-step `mute` — Step mute flag (boolean); saved per step, independent of track mute

**Pattern filenames** must be strict `pNN.json` format where `NN` is a two-digit number (01–64). Do not use legacy pattern filenames or extensionless files.

**[⬆ UP](#table-of-contents) | [📖 README](../README.md#)**

---

## 12. Boot Sequence

**High-level boot order:**

1. Serial begin + firmware version log
2. Pin mapping diagnostics
3. Load runtime settings from NVS/LittleFS
4. Initialize TFT display immediately
5. Show boot log on TFT (via `displayInit()`)
6. Initialize SD card and load sample set
7. Initialize EC11 encoder and KEY0 button
8. Initialize sequencer state and RAM patterns
9. Initialize audio engine (I2S, voice pool)
10. Initialize WiFi/system manager
11. Initialize UI manager
12. Load active pattern group from SD into RAM
13. **Start AudioTask** (core 0, realtime)
14. **Start InputTask** (core 1, input polling)
15. **Start UiTask** (core 1, UI rendering)
16. **Start SystemTask** (core 1, WiFi, web server and commands)
17. `loop()` remains fallback-only (minimal)

The boot log is rendered directly on the TFT using partial row updates. Warnings and errors use color-coded rows and automatic delays.

**[⬆ UP](#table-of-contents) | [📖 README](../README.md#)**

---

## 13. Tasks and Timing

The firmware uses four main FreeRTOS tasks, each with specific timing and safety constraints.

### AudioTask (Core 0, Realtime)

**Frequency:** Fixed 44.1 kHz block rate (~5.8 ms per block)

**Loop body:**

```
sequencerConsumeDueStep()       // check if step needs triggering
audioEngineTriggerSample(...)   // start samples per sequencer step
audioEngineRenderBlock()        // render stereo output to I2S DMA
```

**Constraints:**

- ⛔ No memory allocation
- ⛔ No SD card, LittleFS, or WiFi access
- ⛔ No display drawing
- ⛔ No blocking operations (except fixed tick delay)

Violations cause audio glitches or dropouts.

### InputTask (Core 1)

**Frequency:** ~10 ms

**Loop body:**

```
encoder.update()                // poll quadrature encoder state
auxButton.update()              // poll KEY0 button state
send InputEventMessage to queue // if state changed
```

Debounces and detects multi-press (short/medium/long) patterns.

### UiTask (Core 1)

**Frequency:** Event-driven, redraw on input or sequencer update

**Loop body:**

```
consume InputEventMessage       // input from InputTask
route to uiManager              // finite-state dispatcher
redraw affected UI regions      // partial or full screen
```

### SystemTask (Core 1)

**Frequency:** ~100 ms

**Loop body:**

```
WiFiManager update (if portal open)
system command queue dispatch
WiFi status polling
```

### loop() (Core 1, Fallback-only)

Runs only if task creation failed. Provides minimal input/UI/system fallback to keep device usable.

**[⬆ UP](#table-of-contents) | [📖 README](../README.md#)**

---

## 14. UI Philosophy

The UI should behave like a **dedicated musical instrument**, not a desktop application:

- **Fast:** Immediate response to encoder/button input
- **Predictable:** Consistent navigation and state transitions
- **Muscle-memory friendly:** No surprise mode changes during playback
- **Minimum modal confusion:** Few nested menus; clear escape paths
- **Safe while playing:** No accidental pattern deletion; no SD I/O interruptions during audio

**Avoid:**

- Deep menu hierarchies (max 2–3 levels)
- Full-screen redraws during playback
- Slow animations or transitions
- Confirmation dialogs that trap the user

**[⬆ UP](#table-of-contents) | [📖 README](../README.md#)**

---

## 15. Main Screen Layout

The main **[Groovebox]** sequencer screen shows:

**Header** (top row):
```
Version | Status | Group Name | BPM | Swing
```

**Track rows** (6 rows, one per sample):
```
[KICK  ] [●●●●●●●●●●●●●●●●] [VEL 100] [P 100]
[SNARE ] [●●●●●●●●●●●●●●●●] [VEL 100] [P 100]
[CH    ] [●●●●●●●●●●●●●●●●] [VEL 100] [P 100]
[OH    ] [●●●●●●●●●●●●●●●●] [VEL 100] [P 100]
[TONE  ] [●●●●●●●●●●●●●●●●] [VEL 100] [P 100]
[METAL ] [●●●●●●●●●●●●●●●●] [VEL 100] [P 100]
```

**Parameter page** (rotate via encoder):
- `TRIG` — trigger/step editing
- `VEL` — velocity per step
- `PITCH` — pitch transposition
- `DECAY` — decay/release time
- `PROB` — probability per step
- `STEP` — per-step on/off state; STEP OFF mutes only the selected step and displays it as `m`
- `CHAIN` — chain settings
- `MASTER` — master output level

**Footer** (bottom row):
```
Pattern Name | Chain Status | Mode Indicator
```

**[⬆ UP](#table-of-contents) | [📖 README](../README.md#)**

---

## 16. Controls and Input Routing

### Encoder (EC11)

| Action                  | Behavior                                          |
|-------------------------|---------------------------------------------------|
| Rotate CW/CCW           | Move track cursor up/down; wrap at edges          |
| Short press (<300 ms)   | Enter/exit edit mode for current parameter page   |
| Medium press (300–1s)   | Open the Step popup from Groovebox PLAY/STOP, same as encoder medium press |
| Long press (>1s)        | Open System Settings menu                         |

### KEY0 Button

- Medium press in Groovebox PLAY/STOP opens the same Step popup as encoder medium press.
- Long press in Groovebox PLAY/STOP toggles Track Mute for the selected track. Track Mute affects the complete voice and is shown with `*` after the track name.
- Step mute is separate: STEP OFF affects only the current step and is shown as `m`.

| Action                  | Behavior                                          |
|-------------------------|---------------------------------------------------|
| Short press             | Play/Stop (sequencer transport)                   |
| Medium press            | Alternate tempo/parameter edit (context-dependent)|
| Long press              | Enter Pattern Group menu (save/load/etc.)         |

Input routing is implemented as a finite-state dispatcher in `uiManager.cpp`. Different UI states route the same button code to different handlers.

**[⬆ UP](#table-of-contents) | [📖 README](../README.md#)**

---

## 17. Audio Architecture

**Audio signal path:**

```
User trigger event
  ↓
sequencer outputs track mask per step
  ↓
audioEngineTriggerSample(track, sample, pitch, decay, velocity, pan, ...)
  ↓
allocate/steal voice from fixed pool
  ↓
load sample base gain + setGain.json multiplier
  ↓
apply velocity curve (non-linear response)
  ↓
calculate pitch phase increment (44.1 kHz → tuned frequency)
  ↓
limit playback frames by decay time
  ↓
apply attack fade (0 → max over ~5 ms)
  ↓
apply release fade (max → 0 over decay time)
  ↓
apply stereo pan (L/R balance, optional)
  ↓
sum into stereo interleaved buffer
  ↓
apply master output gain
  ↓
limiter/headroom clamp (prevent digital overdrive)
  ↓
write stereo pairs to I2S DMA buffer
  ↓
I2S DAC (external PCM5102 or similar)
```

**Key points:**

- Voice pool size is fixed (typically 8 voices)
- Voice stealing uses estimated RMS level to prioritize new sounds
- Choke groups allow synchronized release of related samples (e.g., open/closed hats)
- Output is stereo interleaved (L/R/L/R/...) for I2S DMA
- Master gain is applied after mixing but before limiter
- No memory allocation occurs during render

**[⬆ UP](#table-of-contents) | [📖 README](../README.md#)**

---

## 18. Development Rules

**Code style:**

```
Allman braces (opening brace on same line, closing on new line)
2-space indentation
lowerCamelCase for functions and variables
UpperCamelCase for classes and types
English comments and log messages
Comments above the relevant code (not inline)
setup() and loop() at the bottom of main.cpp
loop() delegates only; no blocking logic
```

**Real-time safety:**

```
No filesystem/display/WiFi calls from audioTask()
No memory allocation in render-path functions
No mutex locks in audioEngineRenderBlock()
No network I/O from sequencer or audio code
```

**Logging prefixes:**

```
Error:    — critical failure (device should halt or fallback)
Warning:  — suspicious condition (may indicate misconfiguration)
Info:     — normal operational messages
Debug:    — verbose internal state (compile-time gated)
```

Use `ESP_LOGx()` macros for ESP32 diagnostics:

```cpp
ESP_LOGE("TAG", "Error message: %d", value);    // Error
ESP_LOGW("TAG", "Warning: %s", str);             // Warning
ESP_LOGI("TAG", "Info: %s", str);                // Info
ESP_LOGD("TAG", "Debug: %d", value);             // Debug
```

**[⬆ UP](#table-of-contents) | [📖 README](../README.md#)**

---

## 19. Building From Zero

A developer implementing a compatible Groovebox from zero should follow this order:

1. **appConfig** — Define pin mappings and partition table
2. **Display driver** — ST7789 + boot log rendering
3. **Input scanner** — EC11 quadrature + KEY0 button debounce
4. **SD card + sample manager** — FAT access, WAV parsing, memory allocation
5. **Audio engine** — I2S output, voice pool, pitch/decay/gain processing
6. **Sequencer core** — Step sequencing, BPM, pattern state in RAM
7. **Settings store** — NVS, LittleFS, SD pattern JSON serialization
8. **Groovebox UI screen** — Layout rendering, track display, footer
9. **UI manager** — Finite-state dispatcher, encoder/button routing
10. **Card storage workflows** — Group save/load/copy/rename/delete
11. **WiFi/system manager** — Credential portal, ESP32 NVS reconnect, web server support
12. **Diagnostics** — Boot messages, boot log, safety guards, test modes

Use the individual source-file documents below as the implementation reference.

---

[⬆ UP](#table-of-contents) | [📖 README](../README.md#)