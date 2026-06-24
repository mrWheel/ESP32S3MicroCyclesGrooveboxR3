# `src/InputClass.cpp` — EC11 Encoder and KEY0 Button Input Handler

**Purpose:** Convert raw EC11 quadrature encoder and KEY0 GPIO activity into debounced logical UI events.

---

## Responsibilities

```
1. Configure input GPIO pins from PlatformIO build flags
2. Poll quadrature encoder (pins A and B)
3. Detect and debounce encoder rotation (CW/CCW)
4. Detect and debounce encoder push button (3-level press: short/med/long)
5. Detect and debounce KEY0 auxiliary button (3-level press: short/med/long)
6. Store pending events as InputEventMessage structs
7. Support reversed encoder direction at runtime
8. Timing thresholds for press detection (all configurable)
```

---

## Core Data Structures

**InputConfig struct:**

```cpp
struct InputConfig {
  uint8_t pinEncoderA;
  uint8_t pinEncoderB;
  uint8_t pinEncoderButton;
  uint8_t pinAuxButton;
  uint16_t debounceMs;        // typically 20 ms
  uint16_t shortPressDurationMs;   // < 300 ms
  uint16_t mediumPressDurationMs;  // 300–1000 ms
  uint16_t longPressDurationMs;    // > 1000 ms
  bool encoderDirectionReversed;
};
```

Loaded from PlatformIO build flags via `-DPIN_*` and `-D*_DEBOUNCE_MS`.

**InputEventMessage struct:**

```cpp
struct InputEventMessage {
  uint8_t source;    // SOURCE_ENCODER or SOURCE_AUXBUTTON
  uint8_t type;      // SHORT_PRESS, MEDIUM_PRESS, LONG_PRESS, ROTATE_CW, ROTATE_CCW
  uint32_t timestamp;
};
```

Pushed to input queue when an event occurs. Consumed by UiTask.

**Button timing state:**

```cpp
struct ButtonState {
  uint32_t pressStartTime;
  bool isPressed;
  uint8_t lastReportedType;   // SHORT_PRESS, etc.
  bool reportedMultiPress;    // true if already sent event
};
```

Tracks each button independently to detect press duration and send correct event type.

---

## Quadrature Encoder Decoding

**Gray code state machine:**

The encoder generates two-bit Gray code outputs as the shaft rotates:

```
00 → 10 → 11 → 01 → 00  (clockwise)
00 → 01 → 11 → 10 → 00  (counter-clockwise)
```

The input handler debounces both pins, then decodes the state transition to determine rotation direction.

**Debouncing:**

- Read GPIO state
- Sample again after `debounceMs` (typically 20 ms)
- If stable, register as state change
- Ignores glitches shorter than debounce window

---

## Press Detection Timing

**Multi-level press detection:**

```
t=0:     User presses button
t=150:   State stable (debounce pass)
t=300:   Threshold reached → SHORT_PRESS event sent
t=400:   User still holding
t=1000:  Threshold reached → MEDIUM_PRESS event sent (overrides short)
t=2000:  Threshold reached → LONG_PRESS event sent (final)
t=2100:  User releases
```

Once a press type is sent, holding longer doesn't generate new events (no repeat). Release is tracked but not reported.

---

## Key Internal Functions

**inputCreateConfigFromBuildFlags():**

Reads PlatformIO `-D` defines and constructs InputConfig:

```cpp
config.pinEncoderA = PIN_ENC_A;
config.pinEncoderB = PIN_ENC_B;
config.pinEncoderButton = PIN_ENC_BTN;
config.pinAuxButton = PIN_KEY0;
config.debounceMs = INPUT_DEBOUNCE_MS;
config.shortPressDurationMs = INPUT_SHORT_PRESS_MS;
// ... etc.
```

**readEncoderState() → uint8_t:**

Returns current two-bit quadrature state (00, 01, 10, or 11).

**updateEncoder():**

```
1. Read quadrature state
2. Compare to last stable state
3. If stable after debounce, check for rotation
4. Detect CW or CCW transition
5. Generate ROTATE_CW or ROTATE_CCW event
6. Push to event queue
```

**updateAuxButton():**

```
1. Read KEY0 GPIO
2. Track press start time
3. When debounce stable, check press duration
4. Generate SHORT_PRESS, MEDIUM_PRESS, or LONG_PRESS event
5. Push to event queue
```

**Button event timing:**

- Event is generated once per press at the earliest qualifying time
- Multiple press durations can occur during one press (short → medium → long)
- Each threshold triggers an event immediately upon discovery
- Release doesn't generate an event

---

## Public Functions

### `InputClass::begin()`

**Purpose:** Configure GPIO pins and initialize input state.

**Actions:**

1. Set `pinEncoderA` and `pinEncoderB` to INPUT_PULLUP
2. Set `pinEncoderButton` and `pinAuxButton` to INPUT_PULLUP
3. Read initial GPIO state
4. Initialize button timing states to released
5. Clear pending events

**Call during:** `setup()`

### `InputClass::update()`

**Purpose:** Poll encoder and button hardware.

**Actions:**

1. Call `updateEncoder()`
2. Call `updateAuxButton()`
3. Push any generated events to queue

**Call frequency:** ~10 ms (from InputTask)

**Returns:** true if any events were generated

### `InputClass::updateEncoder()`

**Purpose:** Update quadrature decoder and encoder button state.

Low-level function; usually called from `update()`.

### `InputClass::updateAuxButton()`

**Purpose:** Update KEY0 button state and timing.

Low-level function; usually called from `update()`.

### `InputClass::setConfig(const InputConfig& newConfig)`

**Purpose:** Replace input configuration at runtime.

Useful for testing different debounce or timing values without rebuild.

### `InputClass::getConfig() → const InputConfig&`

**Purpose:** Return currently active input configuration.

### `InputClass::getEncoderEvent() → InputEventMessage`

**Purpose:** Retrieve and consume pending encoder event.

**Returns:** Event struct, or { source: 0, type: 0 } if no event pending

Calling this function clears the pending event.

### `InputClass::clearEncoderEvent()`

**Purpose:** Discard pending encoder event without consuming it.

Useful for flushing stale events.

### `InputClass::getAuxButtonEvent() → InputEventMessage`

**Purpose:** Retrieve and consume pending KEY0 event.

**Returns:** Event struct, or { source: 0, type: 0 } if no event pending

### `InputClass::setEncoderDirectionReversed(bool reversed)`

**Purpose:** Reverse encoder direction at runtime.

- `true` → CW input generates ROTATE_CCW, vice versa
- `false` → normal CW generates ROTATE_CW

Used by System Settings menu to let user reverse encoder if installed backwards.

### `InputClass::getEncoderDirectionReversed() → bool`

**Purpose:** Return current encoder direction reversal state.

---

## GPIO Pin Assumptions

All input pins use `INPUT_PULLUP` configuration, assuming:

- High (~3.3V) = released/idle
- Low (GND) = pressed/active via external pushbutton

Compatible with typical EC11 encoder and momentary button circuits.

---

## Event Queue

Events are stored in a FreeRTOS queue created in `main.cpp`:

```cpp
StaticQueue_t inputQueueStruct;
uint8_t inputQueueStorageArea[INPUT_QUEUE_SIZE];
QueueHandle_t inputEventQueue = xQueueCreateStatic(
  INPUT_QUEUE_COUNT, sizeof(InputEventMessage), ...);
```

InputTask pushes events; UiTask consumes them.

---

## Dependencies

- `appConfig.h` — GPIO pin definitions, timing constants
- FreeRTOS queue (created by main.cpp)
- ESP32 GPIO/timer HAL

---

## Important Implementation Notes

1. **Keep polling cheap.** InputTask runs frequently (~10 ms). No expensive operations here.

2. **No UI drawing.** InputClass generates only events. All screen updates happen in UiTask.

3. **Debounce timing critical.** Set `debounceMs` high enough to filter electrical noise but low enough not to miss fast user inputs (typically 20–30 ms).

4. **Multi-press detection robust.** Each press level (short/medium/long) must be possible during realistic user interaction. Avoid extreme timing thresholds.

5. **Encoder reversal persists.** Save to NVS in systemManager so it survives power-cycle and user doesn't have to re-reverse after reboot.

6. **EVENT queue overflow.** If UI doesn't consume events fast enough, queue may overflow. Older events are dropped. Log this as a warning if it occurs.

---

[⬆ UP](developerBuildGuide.md#20-source-file-reference) | [📖 README](../README.md#)
