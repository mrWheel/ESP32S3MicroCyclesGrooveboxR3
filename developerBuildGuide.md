# ESP32 MicroCycles Groovebox R3
# Developer Build Guide

**Current firmware version:** `v1.4.n`  
**Source of version:** `src/main.cpp`

```cpp
//-- PROG_VERSION.
const char* PROG_VERSION = "v1.4.n";
```

This guide describes the current codebase and is intended to let a developer rebuild the project from zero.

---

## Detailed References

- [`developerBuildGuide`](developerBuildGuide/developerBuildGuide.md)
## Source File Reference

| File | Purpose |
|------|---------|
| [main.md](main.md#src-main-cpp) | Firmware entry point, task orchestration, boot order |
| [DisplayDriverClass.md](DisplayDriverClass.md) | TFT driver, text rendering, UI layout engine |
| [InputClass.md](InputClass.md) | Encoder and button input handling |
| [WiFiManagerExtClass.md](WiFiManagerExtClass.md) | WiFiManager wrapper and captive portal |
| [webServerManager.md](webServerManager.md) | HTTP server foundation for future SPA/API access |
| [webApi.md](webApi.md) | REST API endpoints for pattern, sequencer, and device status |
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

## Development Rules

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

- [`README`](README.md)