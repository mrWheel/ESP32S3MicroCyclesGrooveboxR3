# ESP32 MicroCycles Groovebox
## ESP32 Groovebox / Drum Machine for TFT_LCD_DISPLAY_EC11_BOARD

This document is intended as a **Developer Guideline** for building the
ESP32 MicroCycles Groovebox project.

## Documentation

| Document | Description |
|---|---|
| [codingRules.md](codingRules.md) | Coding conventions and rules for this project |
| [colorSettings.md](colorSettings.md) | Display color theme and UI color rules |
| [DisplayDriverClass.md](DisplayDriverClass.md) | Technical and API documentation for the reusable display driver class |
| [InputClass.md](InputClass.md) | Technical and API documentation for the reusable encoder + auxiliary button input class |
| [WifiSettings.md](WifiSettings.md) | Technical and API documentation for the reusable WiFi settings struct |
| [WiFiManagerExt.md](WiFiManagerExt.md) | Technical and API documentation for the reusable WiFi manager class |

## Disclaimer
This software and/or  hardware is developed incrementally. That means I have 
no clear idea how it works (though it mostly does).

If you have questions about this software, it will probably take you just 
as long to figure things out as it would take me. So I’d prefer that you 
investigate it yourself.

Having said that ***Don’t Even Think About Using It***.

Seriously. Don’t.

Building this design may injure or kill you during construction, burn your 
house down while in use, and then—*just to be thorough*—explode afterward.

This is not a joke. This project involves lethal voltages and temperatures. 
If you are not a qualified electronics engineer, close this repository, 
step away from the soldering iron, and make yourself a cup of tea.

If you decide to ignore all of the above and build it anyway, you do so 
**entirely** at your own risk. You are fully responsible for taking proper 
safety precautions. I take zero responsibility for anything that 
happens—electrically, mechanically, chemically, spiritually, or otherwise.

Also, full disclosure: *I am not a qualified electrical engineer*. I provide 
no guarantees, no warranties, and absolutely no assurance that this design 
is correct, safe, or suitable for any purpose whatsoever.

---

# Purpose

The software target is:

- ESP32-WROVER
- TFT_LCD_DISPLAY_EC11_BOARD
- Adafruit ST7789 (Adafruit_GFX + Adafruit_ST77xx)
- PlatformIO
- Arduino framework
- I2S audio output
- MAX98357A or PCM5102A
- Dual-core realtime architecture

The project uses the following existing classes from the
`ultimateTimer` repository:

- `DisplayDriver`
- `InputClass`
- `WiFiManagerExt`

The software architecture and coding style must remain compatible with
the patterns used in that repository.

This document defines the software architecture and implementation goals
for:

- Phase 1
- Phase 2

It also includes design thoughts for:

- Phase 3
- Phase 4

Phase 3 and 4 features are NOT to be implemented yet.

---

# Hardware

## Board

TFT_LCD_DISPLAY_EC11_BOARD (see <a href="https://willem.aandewiel.nl/index.php/2026/05/19/tft-lcd-display-ec11-piggy-back-board/">this post</a>)

## MCU

ESP32-WROVER with PSRAM

## Fixed Board Pin Mapping

```ini
-D PIN_TFT_BL=13
-D PIN_TFT_RST=14
-D PIN_TFT_CS=18
-D PIN_TFT_DC=23

-D PIN_ENC_BTN=25
-D PIN_KEY0=26
-D PIN_ENC_A=32
-D PIN_ENC_B=33

-D PIN_INPUT1=34
-D PIN_INPUT2=35
```

---

# I2S Audio Mapping

Use:

```ini
-D PIN_I2S_BCLK=27
-D PIN_I2S_WS=4
-D PIN_I2S_DOUT=5
```

Supported DACs:

- MAX98357A
- PCM5102A

Preferred for development:

- MAX98357A

If this build_flag is enabled do not try to initialize (or use) the hardware
```ini
  -D NO_DAC_HARDWARE
```
---

# Software Goals

The groovebox must be:

- deterministic
- realtime-safe
- low-latency
- non-blocking
- stable during playback
- modular
- easy to extend

The groovebox must NOT:

- use LVGL
- use dynamic memory allocation during playback
- use blocking filesystem operations during playback
- redraw the full display continuously
- use heavy UI frameworks

---

# Coding Style Requirements

Use the coding style from the existing repository.

Additional mandatory rules:

- Allman style
- lowerCamelCase
- 2-space indent
- comments above code
- comments in English only
- no emojis in code
- setup() and loop() at end of file
- loop() only calls functions
- use printf()/snprintf() where possible
- use ESP-IDF logging
- audio thread must never block
- All documantation in English only

---

# Libraries

Required:

- Adafruit GFX Library
- Adafruit ST7735 and ST7789 Library
- WiFiManager
- ArduinoJson
- LittleFS
- driver/i2s.h
- ESP-IDF FreeRTOS

---

# Audio Format

## WAV Requirements

Supported format:

- PCM WAV
- mono
- 16-bit
- 22050 Hz

Reason:

- low CPU load
- low memory use
- predictable playback
- easy mixing
- low latency

---

# Project Phases

# Phase 1

## Goal

Create a stable realtime audio foundation.

## Features

### Audio

- initialize I2S
- initialize DMA
- generate test tone
- playback single WAV sample
- playback multiple simultaneous samples
- stereo output with duplicated mono signal

### Filesystem

- initialize LittleFS
- preload samples into RAM/PSRAM
- validate WAV headers
- fixed sample pool

### Input

Use InputClass for:

- encoder
- encoder button
- KEY0 button

### Display

Use DisplayDriver for:

- splash screen
- status screen
- diagnostics
- audio information
- system settings menu

### System Settings Menu

A dedicated settings menu MUST exist in Phase 1.

Menu title:

```text
System Settings
```

Menu entries:

```text
SSID
IP
MAC

 Erase WiFi Credentials
 Start WiFi Manager
 Set theme (Red, Blue, Green enz.)
 Rotate Display (toggle between 1 and 3)
 Encoder Order (A-B / B-A)
 Exit 
```

Requirements:

- read-only fields must not be editable
- WiFiManagerExt must be used
- WiFi Manager portal must be launchable from menu
- credentials erase must reboot after completion
- menu navigation via encoder
- menu rendering via DisplayDriver
- no blocking redraw loops

### Multitasking

Core allocation:

Core 0:

- AudioTask

Core 1:

- UiTask
- InputTask
- SystemTask

### Performance Requirements

Must:

- avoid audio underruns
- avoid heap fragmentation
- use fixed buffers
- use DMA
- avoid blocking calls in AudioTask

---

# Phase 2

## Goal

Implement a playable groovebox sequencer.

## Features

### Sequencer

- 6 tracks
- 16 steps
- BPM control
- play/stop
- pattern memory
- mute
- swing
- track selection

### Machines

#### Sample Machines

Tracks:

- kick
- snare
- closed hat
- open hat

#### Synth Placeholder Machines

Tracks:

- tone
- metal

Only placeholders in Phase 2.

No full synth engine yet.

### UI

Screen layout:

```text
BPM 120   PLAY

KICK   x---x---x---
SNARE  ----x-------
CH     x-x-x-x-x-x-
OH     --------x---

STEP 05
```

### Controls

Encoder:

- rotate = move selected instrument (track)
- rotate in Shift mode = move step cursor
- click = toggle step
- long click = menu

KEY0:

- play/stop
- shift modifier

---

# Documentation Sync Notes (2026-05-23)

The code has evolved from the original phase planning above. Current behavior now includes:

- Groovebox run mode uses partial redraw updates for the `STEP ..` footer line.
- `System Settings` includes confirmation submenus for:
  - `Erase WiFi Credentials`
  - `Start WiFi Manager`
- `Start WiFi Manager` shows a waiting screen until credentials are entered.
- Portal AP identity format is now `<name>-xxyyzz` (last 3 MAC bytes).
- WiFi credential persistence stores:
  - STA credentials
  - last connected SSID and IP

Class documentation was updated accordingly in:

- [DisplayDriverClass.md](DisplayDriverClass.md)
- [WiFiManagerExt.md](WiFiManagerExt.md)

### Sequencer Timing

Requirements:

- use microsecond timing
- no delay()
- no millis()-based sequencing
- use esp_timer_get_time()

### Audio Engine

Requirements:

- sequencer timing inside AudioTask
- sample accurate trigger timing
- fixed voice pool
- fixed mixer structure

---

# Software Architecture

## Tasks

```text
Core 0
└── AudioTask

Core 1
├── UiTask
├── InputTask
└── SystemTask
```

---

# Audio Architecture

```text
LittleFS
→ preload samples
→ sample pool
→ voice mixer
→ I2S DMA
→ DAC
```

---

# Voice System

Use fixed-size voice arrays.

Do NOT allocate voices dynamically.

Example:

```cpp
struct Voice
{
  bool active;
  uint32_t position;
  uint32_t length;
  int16_t *sampleData;
  uint8_t level;
};
```

---

# Sequencer Data Structures

```cpp
struct Step
{
  bool trigger;
  uint8_t velocity;
};

struct Track
{
  Step steps[16];
};
```

---

# Display Requirements

Display updates must use:

- partial redraws
- dirty regions
- sprites only where needed

Must NOT:

- redraw full screen continuously
- block AudioTask

Maximum UI refresh target:

- 20 FPS

---

# Memory Strategy

## Internal RAM

Use for:

- mixer buffers
- DMA buffers
- active voices
- realtime structures

## PSRAM

Use for:

- sample storage
- pattern storage
- future UI buffers

---

# Recommended Directory Layout

```text
src/
├── main.cpp
├── audioEngine.cpp
├── sequencer.cpp
├── sampleManager.cpp
├── uiManager.cpp
├── DisplayDriver.cpp
├── WiFiManagerExt.cpp
├── InputClass.cpp
└── systemManager.cpp

include/
├── appConfig.h
├── audioEngine.h
├── sequencer.h
├── sampleManager.h
├── uiManager.h
├── DisplayDriver.h
├── WiFiSettings.h
├── WiFiManagerExt.h
├── InputClass.h
└── systemManager.h

data/
└── samples/
```

---

# Phase 3 Design Thoughts
## DO NOT IMPLEMENT YET

Phase 3 introduces Elektron-style workflow features.

Planned concepts:

- parameter locks
- probability triggers
- conditional triggers
- microtiming
- retrigger
- pattern chaining
- per-track length
- per-track swing
- song mode
- MIDI clock sync
- MIDI input/output
- BLE MIDI

Important:

The architecture in Phase 1 and 2 MUST already be designed to support these
features later.

Examples:

- tracks must support additional parameters
- steps must support metadata
- sequencer timing must already be sample accurate
- UI state machine must already be modular

Do NOT hardcode assumptions that prevent later expansion.

---

# Phase 4 Design Thoughts
## DO NOT IMPLEMENT YET

Phase 4 introduces synthesis and effects.

Planned concepts:

- ToneMachine synth engine
- MetalMachine FM percussion engine
- filter
- delay
- reverb
- compressor
- sidechain
- oscilloscope
- spectrum analyzer
- performance macros
- scene morphing
- live recording
- sample import tools

Possible future internet features:

- OTA update
- web configuration
- remote control
- sample download

Important:

DSP must remain modular.

Effects must eventually run in isolated DSP blocks.

The mixer architecture must already support future insert/send effects.

---

# Critical Rules

The following rules are mandatory.

## AudioTask

Must NEVER:

- allocate memory
- access filesystem
- use WiFi
- redraw display
- print logs continuously
- wait on mutexes

## UI

Must NEVER:

- redraw entire display continuously
- block input handling
- stall AudioTask

## Sequencer

Must:

- remain deterministic
- avoid timing drift
- use microsecond timing

---

# Final Goal

The final long-term project direction is:

```text
Elektron-inspired ESP32 groovebox
optimized for realtime stability
and minimal hardware
```

The implementation must prioritize:

1. audio stability
2. deterministic timing
3. modular architecture
4. maintainability
5. future expansion
