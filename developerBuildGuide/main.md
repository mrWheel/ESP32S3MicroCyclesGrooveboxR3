<a id="src-main-cpp"></a>

# `src/main.cpp` — Firmware Entry Point and Task Orchestrator

**Purpose:** Arduino framework entry point (`setup()` and `loop()`), FreeRTOS task creation, boot order coordination, version definition, and runtime fallback delegation.

---

## Responsibilities

```
1. Define PROG_VERSION (currently "v1.4.5")
2. Initialize Serial for debugging (115200 baud)
3. Log pin mapping and check for GPIO conflicts
4. Load runtime settings from NVS/LittleFS
5. Initialize display (TFT) immediately for boot logging
6. Run optional SD card smoke test (if SD_SMOKE_TEST enabled)
7. Initialize sample manager (load current sample set from SD)
8. Create input event queue (InputEventMessage)
9. Initialize input (EC11 + KEY0)
10. Initialize sequencer (load first pattern into memory)
11. Initialize audio engine (I2S, voice pool)
12. Initialize system manager (WiFi/NVS reconnect and commands)
13. Initialize web server manager
14. Initialize UI manager (screen state machine)
14. Start FreeRTOS tasks: AudioTask, InputTask, UiTask, SystemTask
15. Provide fallback loop() if task creation fails
```

---

## Key Internal Areas

**Version definition:**

```cpp
const char* PROG_VERSION = "v1.4.5";
```

Located at the top of the file. Update this before each firmware release.

**InputEventMessage queue:**

```cpp
typedef struct {
  uint8_t source;           // ENCODER or AUXBUTTON
  uint8_t type;             // SHORT_PRESS, MEDIUM_PRESS, LONG_PRESS, ROTATE_CW, ROTATE_CCW
  uint32_t timestamp;
} InputEventMessage;

StaticQueue_t inputQueueStruct;
uint8_t inputQueueStorageArea[...];
QueueHandle_t inputEventQueue;
```

Queue communication from `inputTask()` to `uiTask()`.

**logPinConflictWarnings():**

Detects hardware conflicts (GPIO assigned to multiple functions, strapping pin misuse, etc.). Logs warnings if conflicts found.

**runSdSmokeTestAndHalt():**

If `SD_SMOKE_TEST` compile flag is enabled, performs basic SD card I/O validation and halts the device when complete. Useful for hardware debugging.

**runInputUiFallbackCycle():**

Fallback loop if FreeRTOS task creation fails. Polls input, processes UI, and maintains audio playback without tasks. Should remain minimal.

**audioTask():**

High-priority loop running on core 0 (realtime):

```cpp
while (1) {
  sequencerConsumeDueStep();      // check if step is due
  audioEngineTriggerSample(...);  // start samples per step
  audioEngineRenderBlock();       // render and output stereo block
  delayMicroseconds(AUDIO_BLOCK_PERIOD);
}
```

**⛔ Constraints:**

- No memory allocation
- No SD, LittleFS, or WiFi access
- No display drawing
- No blocking operations (except fixed tick)

Audio glitches occur immediately if violated.

**inputTask():**

Polls hardware on core 1:

```cpp
while (1) {
  inputObj.update();                    // update encoder and button state
  if (state changed) {
    send InputEventMessage to queue
  }
  vTaskDelay(10 ms);
}
```

**uiTask():**

Responds to input and sequencer updates on core 1:

```cpp
while (1) {
  if (InputEventMessage available) {
    uiManagerHandleEncoderEvent(...);
    uiManagerHandleAuxButtonEvent(...);
  }
  if (sequencer state changed) {
    redraw affected screen regions
  }
}
```

**systemTask():**

WiFi and system command loop on core 1:

```cpp
while (1) {
  systemManagerUpdate();
  webServerManagerUpdate(systemManagerIsWifiPortalActive());
  vTaskDelay(100 ms);
}
```

---

## Public Functions

### `setup()`

**Purpose:** Initialize the complete firmware in a predictable, documented order.

**Order:**

1. Serial begin
2. Version and pin log
3. Pin conflict check
4. Load runtime settings
5. Initialize display (with boot log)
6. Initialize sample manager (SD card, WAV loading)
7. Create input queue
8. Initialize input driver
9. Initialize sequencer
10. Initialize audio engine
11. Initialize system manager
12. Initialize web server manager
13. Initialize UI manager
14. Create FreeRTOS tasks (AudioTask, InputTask, UiTask, SystemTask)

**Do not:**

- Add blocking loops
- Call network operations (should happen in SystemTask)
- Create additional threads/tasks here (defer to the task creation section)

### `loop()`

**Purpose:** Fallback-only loop; runs only if FreeRTOS task creation failed.

**Behavior:**

- Polls input
- Routes to UI manager
- Calls audio engine render
- Minimal, non-blocking

**Do not:**

- Add background loops here during normal operation
- Place real application logic here

When tasks are running successfully (normal case), `loop()` should rarely execute.

---

## Dependencies

- `DisplayDriverClass.h` — display initialization and boot logging
- `InputClass.h` — encoder and button input
- `audioEngine.h` — audio I2S setup, voice pool
- `sampleManager.h` — SD card sample loading
- `sequencer.h` — pattern sequencing core
- `settingsStore.h` — NVS/LittleFS settings
- `systemManager.h` — WiFi and system management
- `webServerManager.h` — HTTP server manager for future SPA/API access
- `uiManager.h` — UI state machine
- `appConfig.h` — GPIO pin definitions
- FreeRTOS (tasks, queues)
- ESP32 core libraries (Serial, preferences, LittleFS)

---

## Important Implementation Notes

1. **setup() and loop() must stay at the bottom** of this file for easy reading.
2. **audioTask() must remain realtime-safe.** If violated, audio will glitch or dropout.
3. **Do not add blocking I/O to audioTask()** — defer to SystemTask or UiTask.
4. **Keep boot messages short** — TFT boot log has limited space.
5. **PIN_SD_DTCT is checked via `sampleManagerIsSdCardReady()`**, not directly in UI code. This prevents stale card-detect reads.
6. **FreeRTOS task failures are rare** but possible if heap is exhausted. The fallback loop handles this gracefully.
7. **Tasks run on both cores** to avoid contention and maintain audio quality:
   - Core 0: AudioTask (realtime)
   - Core 1: InputTask, UiTask, SystemTask (non-realtime)

---

[⬆ UP](developerBuildGuide.md#20-source-file-reference) | [📖 README](../README.md#)
