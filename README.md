# ESP32 MicroCycles Groovebox (R3)

This repository contains firmware for the ESP32 MicroCycles Groovebox on the R3 hardware revision.

## Developer Guide

For a full from-scratch developer setup and rebuild workflow, read:

- [developerBuildGuide.md](developerBuildGuide.md)

## User Manual

The end-user manual is in the docs folder.

- Start page: [docs/index.html](docs/index.html)
- Manual home: [docs/README.md](docs/README.md)

## Current Scope

- Hardware focus: R3 only
- 6 drum/sample tracks
- 16-step sequencer
- Per-step velocity and probability active in playback
- Track mute and swing playback controls
- Pattern chaining and contextual parameter pages (TRIG/VEL/PITCH/DECAY/PROB/MUTE/CHAIN)
- Step parameter locks (pitch/decay)
- Pattern load/save/new/delete from System Settings
- Optional Wi-Fi setup from System Settings
- SD card sample set from `/samples/*.wav`

## Build Environment

Use PlatformIO environment:

- `ESP32GrooveboxR3`

## Notes

- `TEST_TONE` is for audio-path testing and disables normal sample playback while active.
- `AUDIO_MASTER_GAIN_PERCENT` controls final software output volume.
- `SD_SMOKE_TEST` is diagnostics-only and halts firmware after SD checks.
- R3 build enables PSRAM (`board_build.psram = enabled`, `BOARD_HAS_PSRAM`) and logs PSRAM availability at boot.
