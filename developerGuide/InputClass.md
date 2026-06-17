# `src/InputClass.cpp`

[Back](developerBuildGuide.md)

---

## Purpose

This file converts raw EC11 encoder and KEY0 GPIO activity into debounced logical UI events.

---

## Responsibilities

```text
configure input pins from build flags
read quadrature encoder state
detect left/right rotation
detect short/medium/long encoder-button press
detect short/medium/long KEY0 press
store pending events until UiTask consumes them
support reversed encoder direction
```

---

## Important Implementation Notes

- Keep input polling cheap; it runs frequently.
- Do not call UI drawing functions from this class.
- Debounced logical events are consumed by `uiManager`.
- Timing thresholds are build flags.

---

## Important Internal Areas

```text
inputCreateConfigFromBuildFlags()
readEncoderState()
button press timing state
pendingEncoderEvent and pendingAuxButtonEvent
INPUT_PULLUP assumptions for front-panel controls
```

---

## Public Functions

### `inputCreateConfigFromBuildFlags()`

Builds an `InputConfig` from PlatformIO build flags.

### `InputClass::begin()`

Configures GPIO pins and initializes input state.

### `InputClass::update()`

Polls encoder and auxiliary button.

### `InputClass::updateEncoder()`

Updates quadrature and encoder button state.

### `InputClass::updateAuxButton()`

Updates KEY0 state.

### `InputClass::setConfig(...)`

Replaces input configuration.

### `InputClass::getConfig()`

Returns the active input configuration.

### `InputClass::getEncoderEvent()`

Returns and consumes the pending encoder event.

### `InputClass::clearEncoderEvent()`

Clears the pending encoder event.

### `InputClass::getAuxButtonEvent()`

Returns and consumes the pending KEY0 event.

### `InputClass::clearAuxButtonEvent()`

Clears the pending KEY0 event.

### `InputClass::setEncoderDirectionReversed(bool)`

Changes encoder direction at runtime.

### `InputClass::getEncoderDirectionReversed()`

Returns current encoder direction setting.


---

[UP](developerBuildGuide.md) | [README](../README.md)
