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
