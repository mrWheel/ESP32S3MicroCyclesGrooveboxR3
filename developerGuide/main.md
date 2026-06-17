# `src/main.cpp`

[Back](developerBuildGuide.md)

---

## Purpose

`main.cpp` is the firmware entry point and runtime orchestrator. It owns the boot order, FreeRTOS task creation, task fallback behavior, version definition and high-level hardware startup diagnostics.

---

## Responsibilities

```text
define PROG_VERSION
start Serial logging
log pin mapping
run optional SD smoke test
load runtime settings
initialize display and direct boot log
initialize sample manager
create input queue
initialize input, sequencer, audio, system manager and UI
start AudioTask, UiTask, InputTask and SystemTask
keep loop() as fallback-only delegation
```

---

## Important Implementation Notes

- `setup()` and `loop()` must stay at the bottom of this file.
- `audioTask()` must stay realtime-safe.
- Do not add blocking UI, filesystem or WiFi operations to `audioTask()`.
- Keep boot messages short enough for the TFT boot log.
- `PIN_SD_DTCT` is checked through `sampleManagerIsSdCardInserted()`, not directly in UI code.

---

## Important Internal Areas

```text
PROG_VERSION definition
InputEventMessage queue
logPinConflictWarnings()
runSdSmokeTestAndHalt() when SD_SMOKE_TEST is enabled
runInputUiFallbackCycle() for task creation failure
audioTask() realtime render loop
inputTask() hardware polling loop
uiTask() UI event and redraw loop
systemTask() WiFi/system service loop
```

---

## Public Functions

### `setup()`

Initializes the complete firmware. Keep this function ordered and predictable. Display setup happens early, SD/sample loading follows, then input, sequencer, audio, system manager, UI manager and tasks.

### `loop()`

Fallback-only loop. It runs input/UI/system fallback paths only if task creation failed. Do not place normal application logic here.


---

[UP](developerBuildGuide.md) | [README](../README.md)
