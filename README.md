# ESP32 MicroCycles Groovebox (R3)

This repository contains firmware for the ESP32 MicroCycles Groovebox on the R3 hardware revision.

## User Manual

The end-user manual is in the docs folder.

- Start page: [docs/index.html](docs/index.html)
- Manual home: [docs/README.md](docs/README.md)

## Current Scope

- Hardware focus: R3 only
- 6 drum/sample tracks
- 16-step sequencer
- Sequence load/save/new/delete from System Settings
- Optional Wi-Fi setup from System Settings
- Embedded sample set in firmware (no sample file upload required)

## Build Environment

Use PlatformIO environment:

- `ESP32GrooveboxR3`

## Notes

- `TEST_TONE` is for audio-path testing and disables normal sample playback while active.
- `AUDIO_MASTER_GAIN_PERCENT` controls final software output volume.
