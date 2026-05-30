# ESP32 MicroCycles Groovebox R3
# Developer Build Guide

## 1. Purpose

This document describes the current architecture and intended behavior of the ESP32 MicroCycles Groovebox R3.

This guide is the authoritative reference for developers working on the firmware.

Historical implementation details and superseded storage models are intentionally omitted.

---

# 2. Hardware Platform

## MCU

- ESP32-WROVER
- PSRAM enabled

## Display

- ST7789 320x240 TFT
- TFT_LCD_DISPLAY_EC11_BOARD

## Input

- Rotary encoder
- Encoder push button (EN_BTN)
- Auxiliary button (KEY0)

## Audio

- I2S output
- PCM5102A preferred
- MAX98357A supported

## Storage

- NVS
- LittleFS
- SD Card

---

# 3. Pin Mapping

## Display

- PIN_TFT_BL=2
- PIN_TFT_RST=4
- PIN_TFT_CS=5
- PIN_TFT_SCL=18
- PIN_TFT_SDA=23
- PIN_TFT_DC=15

## Input

- PIN_ENC_BTN=35
- PIN_KEY0=0
- PIN_ENC_A=36
- PIN_ENC_B=39

## Audio

- PIN_I2S_BCLK=26
- PIN_I2S_WS=25
- PIN_I2S_DOUT=27
- PIN_I2S_SD=14

## SD Card

- PIN_SD_CS=13
- PIN_SD_SCK=18
- PIN_SD_MISO=19
- PIN_SD_MOSI=23

---

# 4. Storage Architecture

## NVS

Stores:

- WiFi credentials
- Active pattern group name
- Active sample set
- Runtime settings

## LittleFS

Stores:

- Settings files
- Runtime cache
- Recovery data

LittleFS is NOT used as a mirror of pattern groups.

## SD Card

Permanent storage.

Structure:

```text
/patterns
    /JAZZ01
        p01.json
        p02.json
        ...
    /TECHNO01
        p01.json
        p02.json

/samples
    /S1
        setGain.json
        kick.wav
        snare.wav
        ch.wav
        oh.wav
        tone.wav
        metal.wav

    /S2
        setGain.json
        kick.wav
        snare.wav
        ch.wav
        oh.wav
        tone.wav
        metal.wav
    /S3
    ...
    /S9
```

---

# 5. Runtime Pattern Model

The groovebox works directly from RAM.

Load workflow:

```text
SD Card
 -> JSON
 -> PatternData
 -> Sequencer RAM
```

Save workflow:

```text
Sequencer RAM
 -> JSON
 -> SD Card
```

Pattern groups are NOT copied to LittleFS.

Reason:

- avoids filesystem exhaustion
- avoids duplicate storage
- simplifies implementation
- scales better

---

# 6. Sample Sets

Supported sets:

```text
S1
S2
...
S9
```

Required files:

```text
setGain.json
kick.wav
snare.wav
ch.wav
oh.wav
tone.wav
metal.wav
```

Active sample set is stored in NVS.

Changing sample set may stop playback.

A reboot after selection is acceptable.

---

# 7. Audio Samples

Supported WAV:

- PCM
- 44.1 kHz
- 16-bit
- 24-bit
- mono
- stereo

Samples are loaded into PSRAM.

Missing files may fall back to internally generated content.

---

# 8. Runtime Modules

- main.cpp
- uiManager.cpp
- audioEngine.cpp
- sampleManager.cpp
- sequencer.cpp
- settingsStore.cpp
- systemManager.cpp

---

# 9. Groovebox UI Philosophy

The groovebox must behave as a musical instrument.

Priorities:

- speed
- rhythm
- muscle memory
- live usability

Avoid:

- deep menus
- complex dialog trees
- desktop-like workflows

---

# 10. Main Groovebox Screen

Header:

- BPM
- Swing
- RUN/STOP
- Version

Tracks:

- KICK
- SNARE
- CH
- OH
- TONE
- METAL

Footer:

```text
STEP nn
CUR nn
P:<pattern>
V:<voices>
```

Recommended future display:

```text
P:JAZZ03 p02 -> p04
```

while chaining.

---

# 11. Encoder Model

Normal mode:

- Rotate = select track
- Short EN_BTN = enter track edit
- Medium EN_BTN = tempo popup
- Long EN_BTN = system menu

Track edit mode:

- Rotate = move step cursor
- Short EN_BTN = toggle step
- Medium EN_BTN = edit popup
- KEY0 medium/long = exit track edit

---

# 12. Transport Behavior

KEY0 short in STOP:

```text
START
```

Always starts from:

```text
p01
```

KEY0 short during RUN:

The groovebox must:

1. Finish the currently playing pattern.
2. Play the highest existing pNN pattern.
3. Stop.

Example:

```text
Chain:
p01 -> p02 -> p03 -> p04

STOP during p02

Result:
finish p02
play p04
STOP
```

---

# 13. System Settings

Menu:

```text
SSID: MyWifi            (Read Only)
IP: 192.168.1.44        (Read Only)
MAC: aa:bb:cc:dd:ee:ff  (Read Only)
Local Storage
Card Storage
Set Theme (color)       
Rotate Display (1)      
Encoder Order (x-z)     
Erase WiFi Credentials  
Start WiFiManager
Restart Groovebox       
Exit          
```

Actions may stop playback.

---

# 14. Local Storage Menu

```text
New Pattern
Delete Pattern
```

Delete operation:

- select pattern
- confirm
- delete
- renumber remaining patterns

---

# 15. Card Storage Menu

```text
Load Pattern
Save Pattern
Rename Pattern
Copy Pattern
```

## Load Pattern

1. Optionally save current work.
2. Select pattern group.
3. Load p01..pNN from SD.
4. Import into RAM.
5. Store group name in NVS.

## Save Pattern

Exports RAM to SD.

## Rename Pattern

Renames:

```text
/ patterns / <group>
```

Only.

Never touches sample sets.

## Copy Pattern

Creates new group.

Copies:

```text
/patterns/<group>
```

Only.

Never copies samples.

---

# 16. WiFi Manager

Workflow:

```text
Connect to <hostname>
Browse to 192.168.4.1
Waiting...
```

After portal closes:

```text
Restarting...
```

then:

```cpp
esp_restart();
```

---

# 17. Pattern JSON

Pattern files:

```text
p01.json
p02.json
...
```

Important:

Loader must support both:

```json
"trig": true
```

and

```json
"trigger": true
```

Also:

```json
"locks": {}
```

and

```json
"lockEnabled": true
```

formats.

Backward compatibility is required.

---

# 18. Sequencer Features

Implemented:

- 16 steps
- 6 tracks
- velocity
- probability
- mute
- swing
- chaining
- lock pitch
- lock decay

---

# 19. Audio Engine

Implemented:

- voice pool
- overlapping playback
- voice allocation
- release handling
- choke groups
- limiter
- int32 mixer

Audio must remain realtime safe.

---

# 20. Voice Pool

Fixed-size pool.

No dynamic allocation.

Voice stealing order:

1. inactive
2. released
3. quietest
4. oldest

---

# 21. Choke Groups

Purpose:

- CH cuts OH
- percussion muting

Fade release:

5-15 ms

Never hard-stop audio.

---

# 22. Mixer

Processing order:

```text
sample gain
track gain
velocity
master gain
limiter
output
```

Use int32 accumulation.

---

# 23. Memory Strategy

Internal RAM:

- DMA
- voices
- mixer

PSRAM:

- samples
- large buffers

---

# 24. Audio Task Rules

Audio task must NEVER:

- allocate memory
- access filesystem
- use WiFi
- redraw display
- block on mutexes

---

# 25. Display Rules

Use:

- partial redraw
- dirty regions
- popup updates

Avoid:

- fullscreen redraw during playback

Target:

- 20 FPS maximum

---

# 26. Coding Rules

- Allman style
- lowerCamelCase
- 2-space indent
- comments above code
- English comments only
- ESP logging
- setup() at end
- loop() at end
- loop() only dispatches functions

---

# 27. Not Yet Implemented

The following features are planned but not yet fully implemented:

- true stereo panning
- filter DSP
- overdrive
- delay
- reverb
- compressor
- EQ
- sidechain
- scene morphing
- live recording
- FM percussion engine
- Tone synth engine
- Metal synth engine
- advanced parameter locks
- microtiming playback
- retrigger engine
- conditional trigs

---

# 28. Long-Term Goal

The groovebox should behave as:

```text
Elektron-inspired embedded groovebox
```

Priorities:

1. audio stability
2. deterministic timing
3. musical feel
4. low latency
5. live usability
6. maintainability
