# `src/uiManager.cpp`

[Back](developerBuildGuide.md)

---

## Purpose

This is the central UI state machine. It owns the active UI mode, handles encoder/KEY0 events, connects UI actions to sequencer/storage/audio/system modules, and coordinates pattern group workflows.

---

## Responsibilities

```text
initialize UI state
load active Card pattern group at boot
handle Groovebox screen updates
handle System Settings actions
handle Card Storage actions
handle sample-set list
handle pattern list load/delete
handle pattern group copy/rename input
handle tempo/edit popups
route input through UiEncoderState
synchronize UI chain names to sequencer slot indexes
show busy/status popups
protect SD actions when card is absent
```

---

## Important Implementation Notes

- Keep this as a dispatcher/orchestrator, not a low-level drawing module.
- Keep state-specific input handling in dedicated helper functions.
- Guard all SD-dependent actions with `ensureSdCardPresentForUiAction()`.
- Do not call storage functions directly from the realtime AudioTask.
- Long-press global handling must not override active input states such as pattern group input.

---

## Important Internal Areas

```text
UiState global state
UiEncoderState FSM
executeMenuAction()
handleGlobalLongPressEncoderEvent()
handleGrooveboxEncoderEvent()
refreshPatternList()
loadCardPatternGroupIntoMemory()
saveLoadedPatternGroupToCard()
loadSelectedSampleSetFromMenu()
ensureSdCardPresentForUiAction()
chain slot name arrays
pattern group dirty flag
```

---

## Public Functions

### `uiManagerInit()`

Initializes UI state, pattern-group input, settings, chain data and active pattern group load.

### `uiManagerUpdate()`

Draws/redraws current UI state and handles timed popup expiry.

### `uiManagerHandleEncoderEvent(EncoderEvent)`

Routes encoder events through the UI finite-state dispatcher.

### `uiManagerHandleAuxButtonEvent(ButtonEvent)`

Routes KEY0 events based on current UI state.


---

[UP](developerBuildGuide.md) | [README](../README.md)
