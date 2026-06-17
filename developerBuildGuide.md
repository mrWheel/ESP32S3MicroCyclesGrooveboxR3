# ESP32 MicroCycles Groovebox R3
# Developer Build Guide

**Current firmware version:** `v1.3.n`  
**Source of version:** `src/main.cpp`

```cpp
//-- PROG_VERSION.
const char* PROG_VERSION = "v1.3.n";
```

This guide describes the current codebase and is intended to let a developer rebuild the project from zero.

---

## Detailed References

- [`developerBuildGuide`](developerGuide/developerBuildGuide.md)
- [`src/main.cpp`](developerGuide/main.md)
- [`src/DisplayDriverClass.cpp`](developerGuide/DisplayDriverClass.md)
- [`src/InputClass.cpp`](developerGuide/InputClass.md)
- [`src/WiFiManagerExtClass.cpp`](developerGuide/WiFiManagerExtClass.md)
- [`src/audioEngine.cpp`](developerGuide/audioEngine.md)
- [`src/sampleManager.cpp`](developerGuide/sampleManager.md)
- [`src/sequencer.cpp`](developerGuide/sequencer.md)
- [`src/settingsStore.cpp`](developerGuide/settingsStore.md)
- [`src/systemManager.cpp`](developerGuide/systemManager.md)
- [`src/uiManager.cpp`](developerGuide/uiManager.md)
- [`src/uiGrooveboxScreen.cpp`](developerGuide/uiGrooveboxScreen.md)
- [`src/uiSystemSettingsMenu.cpp`](developerGuide/uiSystemSettingsMenu.md)
- [`src/uiCardStorageMenu.cpp`](developerGuide/uiCardStorageMenu.md)
- [`src/uiCardStorageActions.cpp`](developerGuide/uiCardStorageActions.md)
- [`src/uiPatternGroupInput.cpp`](developerGuide/uiPatternGroupInput.md)
- [`src/uiSequencerInput.cpp`](developerGuide/uiSequencerInput.md)

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