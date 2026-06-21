# `src/sequencer.cpp` — Real-Time Pattern Engine and Step Sequencer

**Purpose:** Real-time step sequencing with sample-accurate timing, pattern slot management, BPM/swing control, chain playback, and thread-safe state access for both audio and UI tasks.

---

## Responsibilities

```
1. Store multiple Pattern slots in RAM (typically 64 slots max)
2. Advance 16-step playback timing at sample-accurate resolution
3. Apply BPM and swing timing to step triggers
4. Generate track trigger masks for AudioTask on each due step
5. Handle probability, velocity, pitch-lock, and decay-lock per-step
6. Track activePatternIndex (visible in UI) vs playingPatternIndex (audio playback)
7. Manage chain targets by slot index
8. Support chain playback (pattern A → pattern B → ...)
9. Export/import PatternData structs for JSON persistence
10. Provide read-only SequencerView snapshot for UI rendering
11. Protect shared state with FreeRTOS portMUX spinlock
```

---

## Core Data Structures

**Pattern struct:**

```cpp
struct Pattern {
  String name;
  uint16_t bpm;
  uint8_t swing;             // 0–100 percent
  bool chainEnabled;
  uint8_t chainLength;       // 1–16 patterns per chain
  uint8_t chainTarget;       // next pattern index (1-based, UI visible)
  uint8_t masterLevel;       // 0–100 percent
  Track tracks[6];           // one per sample (kick, snare, ch, oh, tone, metal)
};

struct Track {
  uint8_t  triggers[16];     // 0 or 1, one per step
  uint8_t  velocity[16];     // 0–100 percent per step
  uint8_t  probability[16];  // 0–100 percent per step
  bool     pitchLocked;      // fixed pitch for all steps
  int8_t   pitchValue;       // −12 to +12 semitones
  bool     decayLocked;      // fixed decay for all steps
  uint8_t  decayValue;       // 0–100 percent
  bool     mute;             // track mute flag
  bool     stepMute[16];      // per-step mute, stored on each Step in firmware
};
```

---

## Timing Model

**BPM to frames conversion:**

```
BPM 120 = 2 beats per second
16-step pattern = 8 beats per loop
1 beat = 44100 samples / 2 = 22050 samples
1 step = 22050 / 2 = 11025 samples
```

Step timing is recalculated whenever BPM changes.

**Swing application:**

```
even steps: normal timing (0 offset)
odd steps: delayed by (swing % × step_duration / 200)

Example at 120 BPM, 50% swing:
step 1: t=0
step 2: t=11025 + (11025 × 50 / 200) = t=13537.5
step 3: t=22050
step 4: t=33075 + (swing offset)
```

---

## Thread-Safe Access

**Critical sections:**

AudioTask (core 0) and UiTask (core 1) both access sequencer state. Mutations protected by `portMUX` spinlock:

```cpp
portMUX_TYPE sequencerMutex = portMUX_INITIALIZER_UNLOCKED;

void sequencerConsumeDueStep(...) {
  portENTER_CRITICAL(&sequencerMutex);
  // read/write shared state
  portEXIT_CRITICAL(&sequencerMutex);
}
```

**Thread roles:**

- **AudioTask:** Read-only access to current pattern, playback timing, triggers
- **UiTask:** Read-update access to editing state, cursor position, active pattern index
- Both: Non-blocking (spinlock hold time < 1 ms)

---

## Active vs. Playing Pattern Index

**activePatternIndex:**

The pattern visible on the UI screen. User edits this pattern. During playback, may differ from playing pattern if chain is active.

**playingPatternIndex:**

The pattern currently being played by the audio engine. During chain playback, advances to `chainTarget` after the pattern finishes.

**Usage:**

```
If chain disabled:
  playingPatternIndex = activePatternIndex  (both same)

If chain enabled and playing:
  playingPatternIndex = current chain slot
  activePatternIndex = user's selected edit slot (independent)
```

---

## Key Internal Functions

**sequencerConsumeDueStep():**

Called from AudioTask at ~5.8 ms intervals (44.1 kHz / 256 samples per block):

```
1. Advance playback frame counter by 256
2. Check if frame counter has crossed a step boundary
3. If step boundary crossed:
   a. Determine which step(s) are due
   b. For each due step in [0..15]
      - Check probability (random pass/fail)
      - Build trigger mask for each track
      - Accumulate velocity, pitch-lock, decay-lock per track
   c. Return trigger mask to audio engine
4. Handle chain transitions (if final step of pattern)
5. Respect track mute and per-step mute flags
```

**loadDefaultPattern():**

Creates an initial pattern seed (all steps empty, mid-tempo, no chains) to avoid undefined state on boot.

**chain transition logic:**

When playback reaches the final step of a pattern and chain is enabled:

```
1. Check chainTarget index
2. Validate chainTarget is a valid loaded slot
3. Schedule pattern switch for AFTER final step plays
4. Update playingPatternIndex to chainTarget
5. If chainTarget is itself a chain endpoint, continue chain
6. If stop-after-final-pattern is set, stop playback instead
```

---

## Public Functions

### `sequencerInit()`

**Purpose:** Initialize sequencer state and default pattern.

**Actions:**

1. Allocate Pattern slot array
2. Load or create default pattern from NVS
3. Register active pattern index = 0
4. Reset playback timing to step 0
5. Stop playback (not playing)
6. Clear chain targets

**Call during:** `setup()`

### `sequencerConsumeDueStep(uint8_t& triggerMask[], uint8_t& trackVelocity[], ...) → bool`

**Purpose:** Called from AudioTask to advance timing and fetch due triggers.

**Parameters:**

- `triggerMask` — output array [6], one bit per track indicating trigger
- `trackVelocity` — output array [6], velocity 0–100 per track
- `trackPitchOffset` — output array [6], pitch −12 to +12 semitones
- `trackDecay` — output array [6], decay 0–100 percent

**Returns:** true if any step is due (triggers generated)

**Constraints:** No memory allocation. Read-only access to sequencer state (except for playback frame counter).

### `sequencerTogglePlay()`

**Purpose:** Start/stop playback.

- If stopped → start from currentPatternIndex, step 0
- If playing → stop immediately, retain pattern position

### `sequencerStopImmediately()`

**Purpose:** Stop playback and reset to step 0 of active pattern.

### `sequencerRequestStopAfterFinalPattern(uint8_t patternIndex)`

**Purpose:** Schedule playback to stop after pattern finishes.

Used by UI "Stop After Chain" feature.

### `sequencerSetLoadedPatternCount(uint8_t count)`

**Purpose:** Specify how many pattern slots are valid and loaded.

Used after loading a pattern group from SD; marks slots 1..count as valid.

### `sequencerClearPatternChainTargets()`

**Purpose:** Clear all chain target mappings.

### `sequencerSetPatternChainTarget(uint8_t fromSlot, uint8_t toSlot)`

**Purpose:** Set which pattern follows `fromSlot` in the chain.

### `sequencerToggleEditMode()`

**Purpose:** Toggle between browse and edit modes.

- Browse: Encoder rotates track cursor
- Edit: Encoder adjusts step triggers/velocity/etc.

### `sequencerMoveCursor(int offset)`

**Purpose:** Move selected step cursor ±1 (wrap at 16).

### `sequencerMoveTrack(int offset)`

**Purpose:** Move selected track cursor ±1 (wrap at 6).

### `sequencerAdjustActivePatternIndex(int offset)`

**Purpose:** Change visible pattern ±1 (wrap at pattern count).

### `sequencerToggleCurrentStepMute()`

**Purpose:** Toggle mute for the selected step only. This preserves the trigger and step parameters, skips the step during playback, and displays the step as lowercase `m`.

This is separate from Track Mute, which mutes the whole voice and is displayed with `*` after the track name.

### `sequencerToggleCurrentStep()`

**Purpose:** Toggle trigger on selected step (0 → 1 or 1 → 0). A newly created trigger should initialize its step mute flag to false.

### `sequencerAdjustCurrentStepVelocity(int delta)`

**Purpose:** Adjust velocity of selected step by ±delta (clamp 0–100).

### `sequencerAdjustCurrentStepProbability(int delta)`

**Purpose:** Adjust probability of selected step by ±delta (clamp 0–100).

### `sequencerAdjustBpm(int delta)`

**Purpose:** Adjust BPM by ±delta (clamp typical 40–200 range).

Recalculates all step timing. Affects playback immediately.

### `sequencerAdjustSwing(int delta)`

**Purpose:** Adjust swing percentage by ±delta (clamp 0–100).

### `sequencerToggleChainEnabled()`

**Purpose:** Enable/disable chain playback for active pattern.

### `sequencerExportPattern(PatternData& out)`

**Purpose:** Convert active pattern to PatternData struct for JSON storage.

### `sequencerImportPattern(const PatternData& in)`

**Purpose:** Import PatternData into active pattern (e.g., after loading from SD).

### `sequencerGetView(SequencerView& out) → bool`

**Purpose:** Copy current state into UI-safe snapshot (read-only copy).

Used by UI to render screen without holding sequencer mutex. Snapshot is consistent point-in-time view.

---

## PatternData Struct (for JSON storage)

```cpp
struct PatternData {
  uint8_t version;
  String name;
  uint16_t bpm;
  uint8_t swing;
  bool chainEnabled;
  uint8_t chainLength;
  uint8_t chainTarget;
  uint8_t masterLevel;
  // ... track array serialized to JSON
};
```

Matches the JSON format in `/patterns/<GROUP>/pNN.json`.

---

## Dependencies

- FreeRTOS (portMUX spinlock)
- `esp_system` — esp_random for probability
- `audioEngine.h` — voice/trigger interface
- `settingsStore.h` — JSON import/export

---

## Important Implementation Notes

1. **Thread-safe mutations only.** All writes to shared state must hold portMUX spinlock. Lock hold time must be < 1 ms (no malloc, I/O, etc. inside critical section).

2. **No display or filesystem calls.** Sequencer generates data only; UI and storage handle rendering/persistence.

3. **Audio-safe playback advance.** `sequencerConsumeDueStep()` must remain deterministic and fast. No nested function calls to blocking code.

4. **Probability is pseudo-random.** Uses `esp_random()` called once per due step. Same seed per boot (not cryptographically random, acceptable for musical playback).

5. **Chain transitions sample-accurate.** Pattern switch occurs exactly at step boundary, not mid-step.

6. **Persistent fields sync.** Any new field to be saved must be added to PatternData struct AND to JSON  import/export in settingsStore.

---

[⬆ UP](developerBuildGuide.md#20-source-file-reference) | [📖 README](../README.md#)
