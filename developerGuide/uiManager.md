# `src/uiManager.cpp` — Central UI State Machine and Input Router

**Purpose:** Main UI finite-state machine, encoder/button event dispatch, pattern group workflows, storage coordination, and screen state management.

---

## Responsibilities

```
1. Initialize UI mode, pattern lists, chain book-keeping
2. Load active pattern group from SD card at boot
3. Handle Groovebox sequencer screen input and navigation
4. Handle System Settings menu (theme, rotation, encoder reverse)
5. Handle Card Storage menu (save/load/copy/rename/delete)
6. Handle sample-set selection menu
7. Handle pattern load/delete actions
8. Handle pattern group name input/editing
9. Handle tempo/master-level edit popups with encoder input
10. Route encoder events through UiEncoderState finite-state machine
11. Synchronize pattern names in UI with sequencer slot indexes
12. Show busyStatus/confirmation popups
13. Guard all SD operations with card-present checks
14. Prevent SD I/O during audio playback (if needed)
```

---

## Core Data Structures

**UiState struct:**

```cpp
struct UiState {
  UiEncoderState currentState;
  PatternListEntry patternList[64];
  uint8_t patternListCount;
  uint8_t patternListSelectedIndex;
  uint8_t patternListFirstVisibleIndex;
  
  // Chain book-keeping
  String chainSlotNames[16];
  
  // Popup state
  bool editPopupOpen;
  String editPopupTitle;
  uint16_t editPopupSelectedIndex;
  
  // Dirty flags
  bool patternGroupDirty;
  bool sampleSetChanged;
};
```

Persists all UI navigation and edit state between frames.

**UiEncoderState enum:**

```cpp
enum class UiEncoderState {
  IDLE,
  GROOVEBOX_BROWSE,      // main screen, navigate tracks
  GROOVEBOX_EDIT,        // main screen, edit parameter
  GROOVEBOX_TEMPO_EDIT,  // tempo popup (encoder adjusts BPM)
  SETTINGS_MENU,         // system settings list
  CARD_STORAGE_MENU,     // save/load/copy/rename/delete
  PATTERN_LIST,          // select pattern to load/delete
  SAMPLE_SET_LIST,       // select sample set to load
  // ... additional states
};
```

Finite-state machine determining which encoder handler executes.

**PatternListEntry:**

```cpp
struct PatternListEntry {
  String name;
  bool isLocal;           // true if in RAM, false if on SD
  uint8_t patternIndex;
  String groupName;       // which group on SD
};
```

---

## Key Internal Functions

**handleGrooveboxEncoderEvent(EncoderEvent event):**

```
IF rotate CW:
  currentTrackIndex++ (wrap)
  redraw track highlight

ELSE IF rotate CCW:
  currentTrackIndex-- (wrap)
  redraw track highlight

ELSE IF short press:
  if (in browse mode)
    enter edit mode for current parameter page
  else
    exit edit mode, return to browse

ELSE IF medium press:
  open tempo edit popup

ELSE IF long press:
  open system settings menu
```

**executMenuAction(uint8_t actionId):**

Dispatcher table for menu actions:
- Save Group
- Load Group
- Copy Group
- Rename Group
- Delete Group (with confirmation)
- Load Pattern
- Switch Sample Set
- etc.

**loadCardPatternGroupIntoMemory(const char* groupName):**

```
1. Check SD card ready
2. For each p01.json–p64.json in /patterns/<groupName>/
   - Parse JSON
   - Load into RAM sequencer slots
3. Log success or errors
4. Clear patternGroupDirty flag
5. Redraw screen
```

**saveLoadedPatternGroupToCard(const char* groupName):**

```
1. Check SD card ready
2. For each sequencer RAM slot
   - Convert to JSON
   - Write to /patterns/<groupName>/p<NN>.json
3. Log success or errors
4. Clear patternGroupDirty flag
```

**refreshPatternList():**

Scan all locally loaded patterns (RAM) and optionally list SD group contents. Update `patternList[]` and count for menu rendering.

**ensureSdCardPresentForUiAction():**

Check `sampleManagerIsSdCardReady()`. If false, show "SD Card Not Found" popup and return false. Prevents confusing SD errors during card removal.

**handleGlobalLongPressEncoderEvent():**

Global handler for encoder long press from any UI state. Typically enters System Settings. Can be overridden by active input states (e.g., text input).

---

## Public Functions

### `uiManagerInit()`

**Purpose:** Initialize UI state and load active pattern group.

**Actions:**

1. Load runtime UI settings from NVS (last active group, mode, etc.)
2. Initialize pattern-group input state
3. Initialize chain slot name arrays
4. Load active pattern group from SD into RAM (via `loadCardPatternGroupIntoMemory()`)
5. Set initial screen mode (Groovebox screen)
6. Call first `uiManagerUpdate()` to draw boot screen

**Call during:** `setup()`, after sequencer and sample manager init

### `uiManagerUpdate()`

**Purpose:** Redraw active UI screen and handle timed popups.

**Actions:**

1. Check if current UI mode requires redraw
2. Call appropriate draw function (Groovebox screen, menu, etc.)
3. Check popup expiry timers
4. Update screen title, header, footer

**Call frequency:** ~50 Hz (from UiTask)

### `uiManagerHandleEncoderEvent(const InputEventMessage& msg)`

**Purpose:** Route encoder events through the finite-state machine.

**Parameters:**

- `msg.type` — SHORT_PRESS, MEDIUM_PRESS, LONG_PRESS, ROTATE_CW, ROTATE_CCW
- `msg.timestamp` — for multi-press detection

**Dispatch:**

```cpp
switch (uiState.currentState) {
  case UiEncoderState::GROOVEBOX_BROWSE:
    handleGrooveboxEncoderEvent(msg);
    break;
  case UiEncoderState::SETTINGS_MENU:
    handleSettingsMenuEncoderEvent(msg);
    break;
  // ... other states
}
```

### `uiManagerHandleAuxButtonEvent(const InputEventMessage& msg)`

**Purpose:** Route KEY0 button events based on current UI state.

**Typical actions:**

| State | Short Press | Medium Press | Long Press |
|-------|-------------|--------------|-----------|
| GROOVEBOX_BROWSE | Play/Stop | (alternate) | Card Storage |
| SETTINGS_MENU | Select | — | Exit |
| CARD_STORAGE_MENU | Execute | — | Exit |

---

## State Transitions

**Example: Entering tempo edit from Groovebox screen**

```
GROOVEBOX_BROWSE (rotate encoder, adjust track)
  ↓ [encoder medium press]
GROOVEBOX_TEMPO_EDIT (rotate adjusts BPM)
  ↓ [encoder short press or timeout]
GROOVEBOX_BROWSE (resume playback)
```

**Example: Saving pattern group**

```
GROOVEBOX_BROWSE
  ↓ [KEY0 long press]
CARD_STORAGE_MENU (list actions)
  ↓ [select "Save Group"]
PATTERN_GROUP_INPUT (enter name)
  ↓ [finish input]
save to SD (if card ready)
  ↓
GROOVEBOX_BROWSE
```

---

## Pattern Group Dirty Flag

The `patternGroupDirty` flag indicates the RAM patterns differ from the SD card group:

- Set when user edits a pattern (add/remove step, change velocity, etc.)
- Cleared after successful `saveLoadedPatternGroupToCard()`
- Displayed in UI footer as "Modified" indicator

Prevents accidental data loss if user forgets to save.

---

## Chain Slot Names

The sequencer supports 16-pattern chains. The UI displays each slot with a name:

```cpp
String chainSlotNames[16];  // e.g., "Groove", "Variation", "Breakdown"
```

Names are stored in the current pattern JSON and synchronized on load/save.

---

## Dependencies

- `DisplayDriverClass.h` — screen rendering
- `sequencer.h` — pattern state, chain control
- `sampleManager.h` — sample set switching, SD card check
- `audioEngine.h` — master level adjustment
- `settingsStore.h` — NVS UI settings, pattern JSON I/O
- `systemManager.h` — WiFi/system commands
- `uiGrooveboxScreen.h` — Groovebox screen drawing
- `uiCardStorageMenu.h`, `uiSystemSettingsMenu.h` — menu rendering
- `uiCardStorageActions.h` — group copy/rename/delete
- `uiPatternGroupInput.h` — text input for group names
- `InputClass.h` — encoder/button events

---

## Important Implementation Notes

1. **Keep this file as dispatcher, not drawing module.** All screen rendering delegates to sub-modules (`uiGrooveboxScreen`, `uiCardStorageMenu`, etc.).

2. **State-specific handlers in dedicated functions.** Avoid giant switch statements in the main event handler. Create `handleGrooveboxEncoderEvent()`, `handleSettingsMenuEncoderEvent()`, etc.

3. **Guard SD operations.** Every SD read/write wrapped in `ensureSdCardPresentForUiAction()`. Prevents confusing errors if card is removed mid-session.

4. **No blocking I/O from UiTask.** If pattern I/O is large, consider deferring to SystemTask with progress popups.

5. **Long-press global handler.** Must not override active input states (e.g., text input in pattern group name field).

6. **Pattern group dirty flag.** Displayed in footer; saved to NVS so user can see changes after power-cycle.

7. **Chain slot names sync.** After loading a pattern group, refresh `chainSlotNames[]` from sequencer state so UI always displays correct names.

---

[⬆ UP](developerBuildGuide.md#20-source-file-reference) | [📖 README](../README.md#)
