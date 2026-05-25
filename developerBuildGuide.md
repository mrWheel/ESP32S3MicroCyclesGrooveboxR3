# ESP32 MicroCycles Groovebox R3
# Developer Build Guide 

This guide explains how to rebuild the full system from zero, including firmware, wiring assumptions, SD card layout, and validation steps.

## 1. Scope

This guide targets:

- Hardware revision: R3
- Firmware environment: ESP32GrooveboxR3
- Audio sample source: SD card in /samples

This guide does not target R2.

## 2. Prerequisites

## 2.1 Hardware

- [TFT LCD DISPLAY EC11 piggy back BOARD](https://willem.aandewiel.nl/index.php/2026/05/19/tft-lcd-display-ec11-piggy-back-board/) (By Willem Aandewiel)
- ST7789 display (320x240)
- Rotary encoder with push button
- KEY0 button
- I2S DAC or DAC/amplifier board
- microSD card module wired in SPI mode
- microSD card (FAT16 or FAT32)
- USB cable for flashing and serial monitor

## 2.2 Software

- Visual Studio Code
- PlatformIO extension
- PlatformIO Core CLI available in terminal (pio)

## 3. Project Setup

1. Clone or copy this repository.
2. Open the repository folder in VS Code.
3. Ensure PlatformIO detects the project (platformio.ini at root).
4. Use environment ESP32GrooveboxR3.

## 4. R3 Pin Mapping

Pin mapping is defined in platformio.ini under env:ESP32GrooveboxR3.

### Display

- PIN_TFT_BL=2
- PIN_TFT_RST=4
- PIN_TFT_CS=5
- PIN_TFT_SCL=18
- PIN_TFT_SDA=23
- PIN_TFT_DC=15

### Input

- PIN_ENC_BTN=35
- PIN_KEY0=0
- PIN_ENC_A=36
- PIN_ENC_B=39

### Audio (I2S)

- PIN_I2S_BCLK=26
- PIN_I2S_WS=25
- PIN_I2S_DOUT=27
- PIN_I2S_SD=14

Note: PIN_I2S_SD is used as enable/shutdown control for the audio board and is driven HIGH by firmware.

### SD Card (SPI)

- PIN_SD_CS=13
- PIN_SD_SCK=18
- PIN_SD_MISO=19
- PIN_SD_MOSI=23

Note: SD and TFT share SCK and MOSI on R3.

## 5. SD Card Content

The firmware expects these exact files:

- /samples/kick.wav
- /samples/snare.wav
- /samples/ch.wav
- /samples/oh.wav
- /samples/tone.wav
- /samples/metal.wav

If one or more files are missing, firmware falls back to internal generated waveforms for missing slots.

WAV decode is streamed from SD into the final sample buffer.
This reduces peak RAM usage compared to full-file staging in RAM during boot.

## 6. Build Flags You Should Know

In platformio.ini, env:ESP32GrooveboxR3:

- TEST_TONE: optional audio path test mode (disables normal sample playback)
- TEST_TONE_FREQUENCY_HZ: frequency used in TEST_TONE mode
- AUDIO_MASTER_GAIN_PERCENT: final software output gain
- SD_SMOKE_TEST: isolated SD diagnostics mode at boot (firmware halts after diagnostics)
- board_build.psram = enabled
- BOARD_HAS_PSRAM
- -mfix-esp32-psram-cache-issue

Keep TEST_TONE disabled for normal groovebox usage.

Keep SD_SMOKE_TEST disabled for normal groovebox usage.

## 7. Build And Flash

From terminal in repository root:

- pio run -e ESP32GrooveboxR3

Upload is done by the developer on hardware (do not automate upload unless explicitly requested).

## 8. First Boot Validation

Open serial monitor at 115200 and verify:

1. Boot log shows R3 pin mapping.
2. Sample manager reports PSRAM availability.
3. Sample manager prints a full SD root listing before any sample load attempt.
4. Sample manager reports per-sample allocation request and load result.
5. Audio engine initializes without pin conflict errors.

If a sample cannot be allocated, the log includes requested bytes and internal/PSRAM free and largest block sizes.

## 9. SD Smoke Test Workflow

Use this when SD card behavior is uncertain.

1. In platformio.ini, enable SD_SMOKE_TEST for ESP32GrooveboxR3.
2. Build and flash.
3. Open serial monitor.
4. Check output:
   - SD init attempts and frequency
   - mount status
   - root directory listing
   - presence of each /samples/*.wav file
5. Disable SD_SMOKE_TEST after diagnostics.

In SD smoke test mode, firmware intentionally halts after diagnostics.
This mode is a diagnostics path only and must not be left enabled for normal usage.

## 10. Runtime Architecture (Quick Orientation)

Main runtime modules:

- src/main.cpp: startup, tasks, orchestration
- src/uiManager.cpp: UI state machine and controls
- src/audioEngine.cpp: I2S output and mixer
- src/sampleManager.cpp: SD sample loading and WAV decoding
- src/sequencer.cpp: sequencing logic
- src/settingsStore.cpp: settings and pattern persistence
- src/systemManager.cpp: Wi-Fi/system command handling

## 10.1 Groovebox UI Philosophy

The groovebox UI MUST behave like a dedicated realtime musical instrument.

The UI is NOT a desktop application and must NOT use deep hierarchical menu trees.

The groovebox should prioritize:

- rhythm
- immediacy
- live usability
- low cognitive load
- muscle memory
- fast editing

The UI must avoid:

- nested menu systems
- configuration-heavy workflows
- modal confusion
- spreadsheet-like editing

The groovebox should always feel performance-oriented.

## 10.2 Main Groovebox Screen

The groovebox always returns to the main sequencer list view:

- Header: BPM, swing, transport state, version
- Six track rows: KICK, SNARE, CH, OH, TONE, METAL
- Parameter/info row above footer (contextual page text in edit mode)
- Footer: current step, cursor step, active voices

This screen remains the primary interaction surface during playback and editing.

## 10.3 Encoder And KEY0 Model

Controls are centered around:

- rotary encoder rotation
- encoder button (EN_BTN)
- auxiliary button (KEY0)

Normal mode behavior:

- Encoder rotate: select track
- Encoder short: enter edit mode
- Encoder medium: open Tempo Edit popup
- Encoder long: open/close System Settings
- KEY0 short: play/stop
- KEY0 medium: open Tempo Edit popup (BPM selected)
- KEY0 long: open Tempo Edit popup (SW selected)

## 10.4 Main-Screen Contextual Edit Pages

In edit mode, the main screen cycles contextual pages:

1. TRIG
2. VEL
3. PITCH
4. DECAY
5. PROB
6. MUTE
7. CHAIN

Main edit-mode behavior:

- TRIG: rotate moves cursor, short toggles step
- VEL: rotate changes velocity
- PITCH: rotate changes lock pitch, short toggles lock
- DECAY: rotate changes lock decay, short toggles lock
- PROB: rotate changes probability
- MUTE: rotate changes selected track, short toggles mute
- CHAIN: rotate changes chain length, short toggles chain on/off

## 10.5 Edit Track Popup

The Edit Track popup is opened with encoder medium press while already in edit mode.

Popup entries:

1. VELOCITY
2. PITCH
3. DECAY
4. PROBABILITY
5. MUTE
6. CHAIN

TRIG is intentionally excluded from the popup because TRIG editing is already fast on the main screen.

Popup states:

- Selection state:
  - rotate: move popup selection
  - short: enter value-edit
  - medium: leave value-edit and stay in popup
  - long: close popup
- Value-edit state:
  - VELOCITY/PITCH/DECAY/PROB: rotate edits value
  - MUTE: rotate toggles ON/OFF
  - CHAIN: short switches focus between ON/OFF and Lx, rotate edits focused field

Popup rendering behavior:

- While popup is open, redraw is partial (popup area only)
- Full-screen redraw on every encoder tick is intentionally avoided for calmer visuals

## 11. Common Failure Modes And Fast Checks

## 11.1 SD Mount Fails

- Verify CS/SCK/MISO/MOSI wiring exactly
- Verify shared SPI lines with TFT are stable
- Verify card format (FAT16 or FAT32)
- Run SD_SMOKE_TEST (diagnostics path) and inspect mount attempts plus missing file names
- Disable SD_SMOKE_TEST again after diagnostics

## 11.2 No Sound

- Verify I2S wiring (BCLK, WS, DOUT)
- Verify PIN_I2S_SD connection to DAC enable/shutdown behavior
- Confirm TEST_TONE is disabled for normal playback
- Confirm sample files are present on SD

## 11.3 Distorted Sound

- Lower AUDIO_MASTER_GAIN_PERCENT
- Verify DAC power and grounding
- Verify sample WAV files are valid PCM WAV

## 12. Rebuild Checklist For A New Developer

1. Open repository and platformio.ini
2. Confirm env:ESP32GrooveboxR3 pin map
3. Prepare SD card with required /samples/*.wav files
4. Build with pio run -e ESP32GrooveboxR3
5. If SD issues appear, enable SD_SMOKE_TEST and diagnose
6. Disable SD_SMOKE_TEST and any other diagnostic flags
7. Rebuild and run normal firmware

## 13. Pattern JSON Schema (Current)

Pattern files are stored in:

- /patterns/Pnnn.json

Top-level fields:

- version
- name
- bpm
- swing
- chainEnabled
- chainLength
- masterLevel
- tracks

Each track contains:

- name
- machine
- sample
- mute
- solo
- level
- pan
- steps

Each step contains:

- trig
- velocity
- probability
- locks (object)

Current locks object fields:

- enabled
- pitch
- decay

The loader now expects this schema directly.
Backward conversion from older pattern schemas is intentionally not used.

## 13.1 Pattern Expansion Strategy

The current JSON schema is intentionally designed for future expansion.

Future Phase 4 features must extend the existing schema instead
of replacing it.

The following extension properties remain available for future phases:

### Step-Level Future Parameters

- microTiming
- retrig
- trigCondition
- filter
- locks

Example future step:

{
  "trig": true,
  "velocity": 110,
  "probability": 75,
  "microTiming": -2,
  "pitch": 5,
  "decay": 80,
  "locks": {}
}

Backward compatibility should remain possible through:

"version": n

schema handling.

# 14. Phase 3 Design Goals

Phase 3 transforms the groovebox from a basic sequencer into a performance-oriented instrument.

Primary goals:

- per-step velocity
- swing
- probability triggers
- mute per track
- pattern chaining
- parameter locks
- contextual parameter editing
- low-latency workflow

The current firmware implements these Phase 3 items:

- per-step velocity
- swing
- mute per track
- per-step probability gating
- pattern chaining (chain length + enable)
- step parameter locks (pitch/decay lock data)
- contextual parameter editing pages

---

## 14.1 Parameter Editing Philosophy

Advanced parameter editing should NOT open deep menu systems.

Instead:

- the main sequencer should remain visible
- temporary parameter overlays should appear
- editing should remain context-driven

Example overlays:

VELOCITY

100 080 120 090

or:

PROBABILITY

100 100 075 050

or:

PITCH

+0 +0 -5 +7

The same sequencer structure remains visible while only the parameter interpretation changes.


## 14.2 Horizontal Parameter Pages

Parameter editing should behave like horizontal pages.

Example pages:

TRIG
VEL
PITCH
DECAY
PROB
MUTE
CHAIN

The UI should prioritize:

- speed
- live editing
- rhythm
- low cognitive load

NOT deep navigation trees.

---

## 14.3 Recommended Phase 3 Implementation Order

Implementation status:

1. mute per track - done
2. velocity per step - done
3. swing - done
4. probability - done
5. pattern chaining - done
6. parameter locks - done

Current lock behavior:

- lock decay scales output trigger level per step
- lock pitch is stored and editable for upcoming DSP features

---

# 15. Phase 4 Design Goals
## DO NOT IMPLEMENT YET

Phase 4 introduces sound design and DSP functionality.

Planned concepts:

- pitch shifting
- decay envelopes
- filter processing
- overdrive/distortion
- delay
- reverb
- compressor/limiter
- stereo panning
- synth voices
- FM percussion engine
- live recording
- performance macros
- scene morphing

---

## 15.1 DSP Architecture Requirements

DSP processing must remain modular.

Effects should eventually operate as isolated DSP blocks.

The mixer architecture should already anticipate:

- insert effects
- send effects
- stereo routing
- future buses

---

## 15.2 Audio Priority Rules

Realtime audio stability always has priority over UI rendering.

Audio tasks must NEVER:

- allocate memory dynamically
- wait on filesystem access
- block on WiFi operations
- stall on display updates

UI rendering must always yield to audio timing requirements.

---

## 15.3 Rendering Philosophy

The display system should use:

- partial redraws
- dirty rectangles
- minimal redraw regions
- sprite-based updates only where useful

Fullscreen redraws during playback should be avoided.

Target UI refresh rate:

20 FPS maximum

Audio stability is more important than UI smoothness.

---

## 15.4 Long-Term Instrument Direction

The final long-term direction is:

Elektron-inspired realtime groovebox
optimized for embedded hardware
and live performance workflow

The groovebox should always prioritize:

1. audio stability
2. deterministic timing
3. workflow speed
4. tactile interaction
5. low cognitive load
6. live usability

The system should favor:

instrument feel

over:

feature quantity

## 16. Related Documentation

- User manual home: docs/README.md
- Browser manual index: docs/index.html
- Coding conventions: codingRules.md
