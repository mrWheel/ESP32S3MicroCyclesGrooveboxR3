# ESP32 MicroCycles Groovebox User Manual

This manual explains how to install, wire, configure, and use the ESP32 MicroCycles Groovebox firmware.

## Manual Pages

1. [Getting Started](./01-getting-started.md)
2. [Hardware and Wiring](./02-hardware-and-wiring.md)
3. [Controls and UI Operation](./03-controls-and-ui.md)
4. [System Settings and WiFi](./04-system-settings-and-wifi.md)
5. [Samples and Filesystem](./05-samples-and-filesystem.md)
6. [Runtime Behavior and Performance](./06-performance-and-behavior.md)
7. [Troubleshooting](./07-troubleshooting.md)

## Device Summary

The groovebox is a 6-track, 16-step ESP32 sequencer with:

- Drum/sample playback (Kick, Snare, CH, OH, Tone, Metal)
- Real-time audio rendering on core 0
- Input, UI, and system handling on core 1
- Rotary encoder + encoder pushbutton + KEY0 control
- TFT screen with theme/rotation options
- Optional WiFi portal for credential setup

## Firmware Scope

This manual documents behavior from the current repository state (Phase 1 + Phase 2 baseline UI/sequencer behavior).
