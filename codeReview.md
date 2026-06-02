# Code Review - ESP32 MicroCycles Groovebox R3

Review date: 2026-06-01  
Repository: `mrWheel/ESP32MicroCyclesGroovebox`  
Branch reviewed: `main`  
PROG_VERSION: `v0.8.5`  
Focus: UI refactor status, RAM/Card pattern workflow, LittleFS cleanup, chain/runtime responsibilities, boot order, and audio-quality architecture.

## 1. Executive Summary

This review uses the current `main` branch after the latest UI refactor commits.

The firmware is now at:

```cpp
const char* PROG_VERSION = "v0.8.5";
```

The codebase is in a significantly better state than during the earlier reviews.

Major improvements now visible in the current code:

- `uiManager.cpp` has been split into multiple focused UI modules.
- Normal startup no longer initializes LittleFS as pattern storage.
- SD/sample initialization still happens before display initialization, which is correct for R3 shared SPI behavior.
- Pattern storage architecture is now clearer:
  - SD Card = persistent pattern groups
  - RAM = active editable patterns
  - LittleFS = settings/configuration only
- Audio quality work has started and includes release fade, choke groups, improved voice stealing, and velocity curve.

Overall assessment:

The project is now more maintainable and architecturally healthier, but the next focus should be functional validation on hardware before doing another large refactor.

---

## 2. Startup / Boot Order

### Status

Good.

The current `setup()` keeps SD/sample initialization before display initialization:

```text
sampleManagerInit()
settingsStoreLoadRuntimeSettings()
input.begin()
display init / bootStatusInit()
sequencerInit()
audioEngineInit()
systemManagerInit()
uiManagerInit()
```

This is important because R3 shares SPI lines between SD and TFT.

### Positive finding

The old normal boot flow that initialized LittleFS pattern storage is no longer visible in current `main.cpp`.

The startup now uses:

```text
Pattern storage: SD/Card model
```

instead of scanning LittleFS `/patterns`.

### Recommendation

Keep this rule documented clearly:

```text
On R3, do not initialize ST7789 before SD card.
SD and TFT share SPI lines.
SD/sample init must remain the first real SPI operation during boot.
```

---

## 3. UI Architecture

### Status

Much improved.

The UI code has been split into focused modules:

```text
uiPatternGroupInput
uiCardStorageActions
uiCardStorageMenu
uiGrooveboxScreen
uiSystemSettingsMenu
uiSequencerInput
```

This is a strong improvement over the previous all-in-one `uiManager.cpp`.

### What improved

`uiManager.cpp` now acts more like a coordinator:

- owns global UI state
- routes encoder and button events
- connects UI actions to sequencer/storage/system modules
- delegates rendering and focused workflows to helper modules

### Remaining risk

`uiManager.cpp` still owns a very large `UiState` struct and still controls many unrelated state domains:

- transport UI state
- menu state
- pattern list state
- pattern group state
- chain target state
- sample set state
- WiFi confirmation state
- status popup state

This is acceptable for now, but future bugs may still happen when one UI mode accidentally affects another mode.

### Recommendation

Do not split more immediately.

First do hardware testing.

Later, consider extracting:

```text
uiRuntimeState.*
uiPatternListPopup.*
```

The goal should be to reduce direct access to the large `UiState` struct.

---

## 4. UI Module Interfaces

### Finding

Some new module interfaces are currently broad. That is normal for a first refactor pass.

The next cleanup should not move more code blindly, but should improve interface shape.

### Recommended direction

Introduce context structs, for example:

```cpp
struct UiGrooveboxRenderState
{
  uint8_t parameterPageIndex;
  bool tempoEditOpen;
  int tempoEditSelection;
  bool editPopupOpen;
  int editPopupSelection;
  bool editPopupValueEdit;
  uint8_t editPopupChainFocus;
  bool chainTargetValid;
  String chainTargetPatternName;
};
```

This would prevent future render functions from growing long argument lists.

### Priority

Medium.

Do this after hardware validation, not before.

---

## 5. Storage Architecture

### Status

Good.

The project has mostly completed the transition away from LittleFS pattern storage.

The current intended model is now:

```text
SD Card:
  /patterns/<group>/pNN.json
  /samples/S1..S9

RAM:
  active editable pattern group

LittleFS:
  settings/configuration only
```

### Positive finding

Current `main.cpp` no longer performs normal LittleFS pattern initialization during startup.

### Remaining cleanup candidate

`settingsStore.cpp` still contains some helper names that appear to belong to the older A01/Z99 pattern naming model.

Potential candidates:

```text
isPatternNameLetterNumberFormat(...)
normalizePatternLetter(...)
settingsStoreFindNextPatternNameForLetterOnCard(...)
settingsStoreCountAvailablePatternSlotsForLetterOnCard(...)
```

These may be harmless if unused, but they should be checked.

### Recommended grep

```bash
grep -R "FindNextPatternNameForLetterOnCard\|CountAvailablePatternSlotsForLetterOnCard\|isPatternNameLetterNumberFormat\|normalizePatternLetter" include src
```
Gives:
```
grep -R "FindNextPatternNameForLetterOnCard\|CountAvailablePatternSlotsForLetterOnCard\|isPatternNameLetterNumberFormat\|normalizePatternLetter" include src
src/settingsStore.cpp:static bool isPatternNameLetterNumberFormat(const String& patternName);
src/settingsStore.cpp:static char normalizePatternLetter(char patternLetter);
src/settingsStore.cpp:static bool isPatternNameLetterNumberFormat(const String& patternName)
src/settingsStore.cpp:  nameLetter = normalizePatternLetter(patternName[0]);
src/settingsStore.cpp:} //   isPatternNameLetterNumberFormat()
src/settingsStore.cpp:static char normalizePatternLetter(char patternLetter)
src/settingsStore.cpp:} //   normalizePatternLetter()
src/settingsStore.cpp:bool settingsStoreFindNextPatternNameForLetterOnCard(char patternLetter, String& outName)
src/settingsStore.cpp:  normalizedLetter = normalizePatternLetter(patternLetter);
src/settingsStore.cpp:        if (isPatternNameLetterNumberFormat(patternName))
src/settingsStore.cpp:          char entryLetter = normalizePatternLetter(patternName[0]);
src/settingsStore.cpp:} //   settingsStoreFindNextPatternNameForLetterOnCard()
src/settingsStore.cpp:bool settingsStoreCountAvailablePatternSlotsForLetterOnCard(char patternLetter, int& outFreeCount)
src/settingsStore.cpp:  normalizedLetter = normalizePatternLetter(patternLetter);
src/settingsStore.cpp:        if (isPatternNameLetterNumberFormat(patternName))
src/settingsStore.cpp:          char entryLetter = normalizePatternLetter(patternName[0]);
src/settingsStore.cpp:} //   settingsStoreCountAvailablePatternSlotsForLetterOnCard()
```

If only declarations/definitions remain, remove them in a small cleanup commit.

---

## 6. Card Storage

### Status

Good architecture, needs hardware workflow validation.

The split into `uiCardStorageActions` and `uiCardStorageMenu` is useful.

Card Storage should remain responsible for:

- Load Pattern Group
- Save Pattern Group
- Copy Pattern Group
- Rename Pattern Group
- Delete Pattern Group
- Busy/status feedback for SD actions

### Recommendation

Do a practical SD-card test matrix:

1. Load existing group.
2. Save group.
3. Copy group.
4. Rename copied group.
5. Reboot.
6. Confirm active group is restored from NVS.
7. Confirm no extensionless `pNN` files are created.
8. Confirm only `pNN.json` files exist.

---

## 7. Sequencer / Chain Runtime

### Status

Improved.

The sequencer now has explicit runtime concepts:

- loaded pattern count
- playing pattern index
- chain target index
- chain target validity

This is the correct direction for RAM pattern groups.

### Main risk

The UI still stores chain targets as pattern names, while the sequencer uses slot indexes.

This is okay, but synchronization must be reliable.

Sync should happen after:

- loading a pattern group
- adding a pattern
- deleting a pattern
- editing a chain target
- saving when pending chain state exists
- copy/rename workflows when active group changes

### Recommended debug helper

Add later:

```cpp
validateSequencerChainTargetsFromUi()
```

It should check:

- every chain target resolves to a loaded slot
- no chain target points beyond loaded pattern count
- sequencer loaded count matches UI loaded count

Keep it debug-only.

---

## 8. Audio Engine

### Status

Good audible improvements implemented.

The current audio engine includes:

- fixed voice pool
- release fade
- choke group release
- improved voice selection / voice stealing
- musical velocity curve
- soft limiter / headroom support

### Positive finding

The voice stealing strategy now prefers:

1. free voice
2. voice already in release
3. quietest active voice
4. oldest active voice

This is a major improvement over always overwriting voice 0.

### Remaining issue

When all voices are active and the fallback must steal a voice, the selected voice is still overwritten immediately.

This can still click in dense patterns.

### Recommended next audio work

Do these later, after storage/UI validation:

1. track gain staging
2. limiter refinement
3. optional sample-start fade-in
4. optional per-track saturation

Stereo/panning should remain low priority while hardware output is effectively mono.

---

## 9. Realtime Safety

### Status

Good.

The audio engine remains mostly isolated from UI/storage workflows.

Continue enforcing:

```text
Audio task must not:
- allocate dynamically
- access filesystem
- access WiFi
- wait on display/UI work
- log continuously
```

Storage operations may block playback when invoked from System/Card Storage menus. That is acceptable for this instrument design.

---

## 10. Code Style / Maintainability

### Positive finding

The project is now modular enough to continue safely.

### Remaining issue

Pattern naming helpers may be duplicated across modules.

A later utility module could help:

```text
patternNameUtils.h
patternNameUtils.cpp
```

Potential functions:

```cpp
String buildPatternNameForSlot(uint8_t slotIndex);
bool patternSlotIndexFromName(const String& name, uint8_t& outSlotIndex);
String normalizePatternSlotName(const String& name);
```

This should be a later cleanup, not an immediate refactor.

---

## 11. Recommended Next Steps

### Step 1: Functional hardware validation

Before more refactoring:

1. Boot with SD card inserted.
2. Confirm display shows boot screen.
3. Confirm samples load from active sample set.
4. Confirm active pattern group restores from NVS.
5. Load group from Card Storage.
6. Edit steps in multiple patterns.
7. Edit chain targets.
8. Save group.
9. Reboot.
10. Confirm steps and chain targets persist.
11. Test STOP behavior.
12. Test Copy/Rename Pattern Group.
13. Test dense audio pattern for voice stealing and choke behavior.

### Step 2: Small cleanup only

Only if grep proves unused helpers:

```text
remove remaining old A01/Z99 naming helpers
```

### Step 3: Audio work

After functional validation:

```text
track gain staging
limiter refinement
sample start fade-in
```

---

## 12. Overall Assessment

The current codebase is healthier than before.

The most important previous issues have been reduced:

- `uiManager.cpp` is no longer the only UI file.
- LittleFS pattern startup flow has been removed.
- Pattern groups are now centered around SD + RAM.
- Audio quality has improved.
- Build cleanliness has improved.

Main advice now:

```text
Stop large refactors temporarily.
Test on real hardware.
Then continue with small targeted improvements.
```

The next highest-value work is not another big split. It is verifying that the instrument behaves correctly after all architectural changes.
