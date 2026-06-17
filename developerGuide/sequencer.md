# `src/sequencer.cpp`

[Back](developerBuildGuide.md)

---

## Purpose

This file owns the realtime pattern engine, RAM pattern slots, step playback timing, editing operations, transport state and pattern chain behavior.

---

## Responsibilities

```text
store multiple Pattern slots in RAM
advance 16-step playback timing
apply BPM and swing timing
produce due-step trigger masks for AudioTask
handle probability, velocity and lock values
track active visible pattern and playing pattern
manage chain targets by slot index
start playback from active pattern when chain is off
export/import PatternData for storage
provide a read-only SequencerView snapshot for UI
```

---

## Important Implementation Notes

- AudioTask and UI both use sequencer state; protect shared mutations.
- Do not add display or filesystem operations here.
- Any new persistent field must be added to `PatternData` and settingsStore JSON.
- Chain targets in UI are names; sequencer uses slot indexes.

---

## Important Internal Areas

```text
critical sections around shared sequencer state
sequencerConsumeDueStep() used by AudioTask
activePatternIndex vs playingPatternIndex
chainTargetIndex[] and chainTargetValid[]
pattern slot create/delete/store/load helpers
default pattern seed logic
stop-after-final-pattern behavior
```

---

## Public Functions

### `sequencerInit()`

Initializes deterministic pattern, timing and selection state.

### `sequencerConsumeDueStep(...)`

Called from AudioTask to advance timing and return track triggers for due steps.

### `sequencerTogglePlay()`

Toggles transport playback.

### `sequencerStopImmediately()`

Stops playback and resets relevant state immediately.

### `sequencerRequestStopAfterFinalPattern(uint8_t)`

Schedules playback stop behavior after a final pattern.

### `sequencerSetLoadedPatternCount(uint8_t)`

Sets how many pattern slots are active.

### `sequencerClearPatternChainTargets()`

Clears all chain target mappings.

### `sequencerSetPatternChainTarget(...)`

Sets one pattern slot chain target.

### `sequencerToggleEditMode()`

Toggles step edit mode.

### `sequencerMoveCursor(int)`

Moves selected step cursor.

### `sequencerMoveTrack(int)`

Moves selected track.

### `sequencerMoveTrackAndPattern(int,uint8_t)`

Moves across tracks and pattern edges.

### `sequencerAdjustActivePatternIndex(int)`

Changes active visible pattern.

### `sequencerSetActivePatternIndex(uint8_t)`

Sets active visible pattern directly.

### `sequencerToggleCurrentStep()`

Toggles trigger on the selected step.

### `sequencerAdjustCurrentStepVelocity(int)`

Adjusts selected step velocity.

### `sequencerAdjustCurrentStepProbability(int)`

Adjusts selected step probability.

### `sequencerAdjustCurrentStepLockPitch(int)`

Adjusts selected pitch lock.

### `sequencerAdjustCurrentStepLockDecay(int)`

Adjusts selected decay lock.

### `sequencerToggleCurrentStepLock()`

Toggles selected lock state.

### `sequencerAdjustBpm(int)`

Adjusts BPM.

### `sequencerAdjustSwing(int)`

Adjusts swing percentage.

### `sequencerToggleMuteForSelectedTrack()`

Toggles mute on selected track.

### `sequencerAdjustChainLength(int)`

Adjusts chain length.

### `sequencerToggleChainEnabled()`

Toggles chain for active pattern.

### `sequencerStartFromActivePattern()`

Starts playback from the visible pattern.

### `sequencerExportPattern(...)`

Exports active pattern to PatternData.

### `sequencerExportPatternFromSlot(...)`

Exports a specific slot to PatternData.

### `sequencerImportPattern(...)`

Imports PatternData into active pattern.

### `sequencerImportPatternToSlot(...)`

Imports PatternData into a specific slot.

### `sequencerGetView(...)`

Copies current state into a UI-safe snapshot.


---

[UP](developerBuildGuide.md) | [README](../README.md)
