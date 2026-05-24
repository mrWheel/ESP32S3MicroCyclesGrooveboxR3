# 6. Runtime Behavior and Performance

## 6.1 Task Model

Core 0:

- `AudioTask`

Core 1:

- `UiTask`
- `InputTask`
- `SystemTask`

## 6.2 Sequencer Engine Characteristics

- 6 tracks × 16 steps
- 4 pattern memory slots
- Deterministic step scheduling using `esp_timer_get_time()`
- Swing timing applied per step interval

## 6.3 Audio Engine Characteristics

- Fixed voice pool (16 voices)
- No dynamic voice allocation per trigger
- Clip-safe 16-bit mix output
- Mono source duplicated to stereo out

## 6.4 UI Refresh Strategy

- Full screen draw when dirty state changes
- Footer-only partial updates while running
- Refresh interval target: 50 ms cadence in UI manager

## 6.5 Safe Fallback Behavior

If task creation for UI/input fails:

- `loop()` runs fallback cycle for input + UI updates

If system task fails:

- `loop()` still services `systemManagerUpdate()`

This keeps the device operational under partial startup failures.
