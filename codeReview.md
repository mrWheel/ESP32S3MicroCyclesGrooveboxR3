# Code Review - ESP32 MicroCycles Groovebox R3

Review date: 2026-05-31  
Repository: `mrWheel/ESP32MicroCyclesGroovebox`  
Branch reviewed: `main`  
Focus: RAM-based pattern workflow, Card Storage, chain playback, NVS/LittleFS/SD responsibilities, UI state handling, and realtime safety.

## Executive Summary

The project has made a major architectural transition from a LittleFS/local-pattern model to a RAM-based pattern workflow with SD Card pattern groups. That direction is correct for the ESP32-WROVER target because LittleFS space is too limited for full pattern group mirroring.

The current codebase is close to the desired architecture, but there are still several leftover assumptions from the older Local/LittleFS model. These leftovers are now the main source of risk.

The most important issues are:

1. Startup still contains legacy LittleFS pattern initialization/listing logic.
2. `settingsStore.cpp` still contains legacy pattern path concepts such as extensionless pattern files and older chain functions.
3. Runtime chain playback is now partly table-driven, but it needs full verification after save/load.
4. Stop behavior depends on the UI chain table being synchronized before playback and before STOP requests.
5. Card Storage Save now writes `.json`, but older extensionless files and older delete/list paths must be audited thoroughly.
6. Pattern group copy/rename now exists, but copy failure cleanup is not robust yet.
7. UI popup rendering is improving, but the pattern group input popup still depends on exact overlay row coordinates and should be consolidated in `DisplayDriver`.

Overall recommendation: do a short stabilization pass before adding more features.

---

# 1. High-Priority Findings

## 1.1 Startup still contains legacy LittleFS pattern flow

### Severity

High

### Evidence

In `main.cpp`, setup still performs SD/sample initialization first, which is correct for the shared SPI bus. However, after display init and sequencer init, the code still performs:

- `settingsStoreLoadRuntimeSettings()` a second time
- `displaySetRotation(...)`
- `displaySetThemeColorIndex(...)`
- `input.setEncoderDirectionReversed(...)`
- `settingsStoreInitPatternStorage()`
- `settingsStoreListPatterns(...)`
- LittleFS pattern listing/status messages

This is visible in the current `setup()` structure.

### Why this matters

The new architecture says:

```text
SD Card = permanent pattern group storage
RAM     = active editable patterns
LittleFS = settings/cache/recovery only
```

Therefore, startup should not present LittleFS pattern storage as part of the normal boot path.

### Recommended action

Keep:

```text
sampleManagerInit()
settingsStoreLoadRuntimeSettings()
input.begin()
display init
sequencerInit()
audioEngineInit()
systemManagerInit()
uiManagerInit()
```

Remove or demote the old LittleFS pattern scan to diagnostics only.

Suggested replacement:

```text
bootStatusPush("Pattern storage: SD/Card model");
```

Do not list `/patterns` from LittleFS during normal boot.

---

## 1.2 Runtime settings are loaded twice during setup

### Severity

Medium

### Evidence

`main.cpp` loads runtime settings once after SD/sample init, then later loads them again after `sequencerInit()`.

### Why this matters

It is not dangerous by itself, but it creates confusion and makes logs misleading. It also increases the chance that future changes accidentally depend on the second load.

### Recommended action

Load runtime settings exactly once:

```text
after SD/sample init
before input/display setup
```

Then remove the later duplicate block.

---

## 1.3 Chain playback now uses explicit targets, but runtime chain enable is still global

### Severity

High

### Evidence

The new chain flow uses:

```cpp
uiState.chainSlotTargetPatternNames[slotIndex]
sequencerSetPatternChainTarget(...)
state.chainTargetIndex[]
state.chainTargetValid[]
```

This is the right direction.

However, playback still depends on a global:

```cpp
state.chainEnabled
```

while the actual chain relationship is now per pattern slot.

### Why this matters

If `state.chainEnabled` is false while valid per-slot targets exist, playback may not follow the chain.

If `state.chainEnabled` is true while the active/playing slot has no valid target, behavior depends on fallback logic.

### Recommended action

For the new model, playback should be based primarily on:

```cpp
state.chainTargetValid[state.playingPatternIndex]
```

A global `chainEnabled` may still exist for UI display, but the actual decision to advance should be:

```cpp
if (state.chainTargetValid[state.playingPatternIndex])
{
  state.playingPatternIndex = state.chainTargetIndex[state.playingPatternIndex];
}
else
{
  // no chain target; stay or stop depending on transport state
}
```

The UI can still show `ON/OFF`, but runtime should not rely on only one global flag for all pattern slots.

---

## 1.4 Save/load chain round-trip needs a focused test

### Severity

High

### Evidence

`saveLoadedPatternGroupToCard()` now sets:

```cpp
patternData.chainTarget = uiState.chainSlotTargetPatternNames[slotIndex];
patternData.chainEnabled = !patternData.chainTarget.isEmpty();
patternData.chainLength = loadedPatternCount;
```

This is the correct intended behavior.

`loadCardPatternGroupIntoMemory()` loads:

```cpp
uiState.chainSlotTargetPatternNames[patternIndex] = patternData.chainTarget;
```

This is also correct.

### Risk

The chain can still disappear if any of these are wrong:

- parser fails to read `chainTarget`
- `buildPatternJsonDocument()` does not write `chainTarget`
- `saveLoadedPatternGroupToCard()` is not reached after editing
- `flushPendingChainSettings()` is not called before save
- `syncSequencerChainTargetsFromUi()` is not called after load or after edit

### Recommended test

Use a four-pattern group:

```text
p01 -> p02
p02 -> p03
p03 -> p02
p04 -> --
```

Then:

1. Save group.
2. Power cycle.
3. Load group.
4. Inspect JSON files on SD.
5. Verify Edit Track popup shows:
   - p01 target p02
   - p02 target p03
   - p03 target p02
   - p04 target --
6. Start playback and confirm:
   - p01 -> p02 -> p03 -> p02 ...
7. Press STOP during p02 and confirm:
   - finish p02
   - play p04
   - stop

---

# 2. Storage Findings

## 2.1 `patternFileExtension` is still an empty string

### Severity

Medium

### Evidence

`settingsStore.cpp` still contains:

```cpp
static const char* patternFileExtension = "";
```

At least one older function still builds paths using this extension.

### Why this matters

The intended Card format is now:

```text
/patterns/<group>/pNN.json
```

The empty extension caused earlier duplicate files:

```text
p01
p01.json
```

### Recommended action

Eventually change the constant to:

```cpp
static const char* patternFileExtension = ".json";
```

or remove it entirely and make all Card pattern path construction explicit:

```cpp
groupDir + "/" + normalizedName + ".json"
```

Also audit all functions that still use `patternFileExtension`.

---

## 2.2 Legacy chain settings functions still point to old paths

### Severity

Medium

### Evidence

There are still functions such as:

```cpp
settingsStoreLoadPatternChainSettingsFromCard(...)
```

that appear to build paths without the pattern group directory.

### Why this matters

The new Card structure is:

```text
/patterns/<group>/pNN.json
```

A function that loads:

```text
/patterns/pNN
```

or:

```text
/patterns/pNN.json
```

is wrong in the new architecture.

### Recommended action

Mark old Local/LittleFS/Card chain functions as deprecated or remove them once the RAM-chain path is stable.

Preferred current chain source of truth:

```text
SD JSON -> PatternData.chainTarget -> uiState.chainSlotTargetPatternNames[] -> sequencer chain target table
```

---

## 2.3 Copy Pattern does not robustly clean up partial copies on failure

### Severity

Medium

### Evidence

`settingsStoreCopyPatternGroupOnCard()` creates the target directory, then copies files one by one.

If copying one file fails after several files were copied, the code returns false, but the partially copied target group may remain.

### Why this matters

A half-created pattern group can later appear in Load Pattern and cause inconsistent runtime behavior.

### Recommended action

Add recursive cleanup for the target group on failure.

Example behavior:

```text
copy failed
-> delete all files in /patterns/<targetGroup>
-> remove /patterns/<targetGroup>
-> return false
```

---

# 3. Sequencer / Transport Findings

## 3.1 Stop behavior is now architecturally correct, but needs integration verification

### Severity

High

### Intended behavior

Given:

```text
Loaded: p01, p02, p03, p04
Chain:  p01 -> p02 -> p03 -> p02
```

STOP during p02 should:

```text
finish p02
play p04
stop
```

Given:

```text
Chain: p01 -> p02 -> p03 -> p04 -> p01
```

STOP should:

```text
stop immediately
```

### Current direction

The UI now has:

```cpp
areAllLoadedPatternsIncludedInPlaybackChain()
```

and transport action calls either:

```cpp
sequencerStopImmediately()
```

or:

```cpp
sequencerRequestStopAfterFinalPattern(finalPatternIndex)
```

This is the right model.

### Recommended action

Add temporary logs while testing:

```cpp
ESP_LOGI(logTag, "STOP requested: playing=%u final=%u allIncluded=%s", ...);
```

Remove or demote to debug after validation.

---

## 3.2 `sequencerSetLoadedPatternCount()` is important and must be called after all add/delete/load operations

### Severity

High

### Evidence

The sequencer now tracks:

```cpp
state.loadedPatternCount
```

This prevents chain length and playback from reaching unloaded RAM slots.

### Risk

If Add Pattern or Delete Pattern changes `uiState.chainSlotPatternNames[]` but does not call:

```cpp
sequencerSetLoadedPatternCount(...)
syncSequencerChainTargetsFromUi()
```

then the UI and sequencer can disagree.

### Recommended action

After each operation:

- Load Pattern
- Add Pattern
- Delete Pattern
- Copy Pattern followed by load
- Rename Pattern if active group changes

call:

```cpp
sequencerSetLoadedPatternCount(getLoadedPatternSlotCount());
syncSequencerChainTargetsFromUi();
```

---

# 4. UI Findings

## 4.1 Pattern group input popup is now in the right direction

### Severity

Low

### Evidence

`drawPatternGroupNameInput()` now draws:

```text
Copy/Rename:
<source group>
To:
<input>
Turn=<token>
Hold=...
```

and then updates only rows 3 and 4 using:

```cpp
display.updateSelectionOverlayRow(3, inputText);
display.updateSelectionOverlayRow(4, tokenText);
```

This is a good improvement over redrawing the whole screen.

### Remaining risk

`updateSelectionOverlayRow()` must match the exact row geometry used by `drawSelectionOverlay()`. If either layout changes, partial redraw will break again.

### Recommended action

Move the row geometry into one shared helper inside `DisplayDriver`.

Ideal:

```cpp
DisplayDriver::getSelectionOverlayRowRect(rowIndex, ...)
```

Both full draw and partial update should use the same geometry.

---

## 4.2 UI Manager has accumulated too many responsibilities

### Severity

Medium

### Evidence

`uiManager.cpp` now handles:

- menu state
- sequencer view state
- Card Storage workflow
- RAM pattern add/delete
- pattern group copy/rename input
- chain target synchronization
- sample set loading
- WiFi menu state
- busy/status popups

### Why this matters

This file is now the most fragile part of the project. Small edits can easily break unrelated modes.

### Recommended refactor

Split later into:

```text
uiManager.cpp
uiPatternGroupActions.cpp
uiPatternGroupInput.cpp
uiCardStorageMenu.cpp
uiGrooveboxScreen.cpp
```

Do not refactor immediately while stabilizing. First finish functional testing.

---

# 5. Realtime Safety Findings

## 5.1 Audio task remains mostly isolated

### Severity

Good

The newer storage/UI changes do not appear to add filesystem or SD access to the audio task. That is good.

### Keep enforcing

Audio task must never:

```text
allocate memory
access filesystem
use WiFi
redraw display
wait on UI mutexes
log continuously
```

---

## 5.2 `String` usage is acceptable in UI/storage, not in audio

### Severity

Low

`String` use is heavy in `uiManager.cpp` and `settingsStore.cpp`. That is acceptable for menu/storage workflows, but should not move into audio or tight realtime paths.

---

# 6. Boot / SPI Findings

## 6.1 SD-first boot order is correct for this hardware

### Severity

Good

The code now keeps SD/sample initialization before display initialization. This matches the observed hardware behavior where initializing the display before SD broke SD card access on the shared SPI bus.

Current intended order:

```text
Serial
SD/sample init
NVS runtime settings
input init
display init
sequencer/audio/system/ui init
```

### Recommendation

Document this clearly in `developerBuildGuide.md`:

```text
Do not initialize ST7789 before SD card on R3.
TFT and SD share SPI lines.
SD must be the first real SPI owner during boot.
```

---

## 6.2 Startup still says "Init LittleFS patterns"

### Severity

Low / Medium

This is misleading under the new architecture.

### Recommended action

Replace with:

```text
Init runtime pattern group
```

or remove entirely.

---

# 7. Positive Findings

The following changes are strong improvements:

- Pattern groups are no longer mirrored into LittleFS.
- Pattern groups load directly from SD into sequencer RAM.
- Save Pattern now writes `.json` files and removes legacy extensionless files.
- Active pattern group is persisted in NVS.
- Pattern group input now supports Copy/Rename workflows.
- Load/Save/Copy/Rename now have visible busy/status feedback.
- Popup redraws are moving toward partial updates.
- Chain length is limited to the number of loaded patterns.
- Footer now distinguishes viewed pattern, playing pattern, and next chain target.
- SD-first boot order reflects actual hardware constraints.

---

# 8. Suggested Stabilization Checklist

Before adding more features:

## Build

- Clean build:
  ```bash
  pio run -e ESP32GrooveboxR3
  ```

## SD layout

Confirm only `.json` pattern files exist:

```text
/patterns/JAZZ01/p01.json
/patterns/JAZZ01/p02.json
...
```

No extensionless files:

```text
/patterns/JAZZ01/p01
/patterns/JAZZ01/p02
```

## Load/save test

1. Load JAZZ01.
2. Edit p02.
3. Set chain:
   ```text
   p01 -> p02
   p02 -> p03
   p03 -> p02
   p04 -> --
   ```
4. Save.
5. Reboot.
6. Load JAZZ01.
7. Verify steps and chain targets.

## Stop test

With:

```text
p01 -> p02 -> p03 -> p02
p04 unused
```

STOP during p02 should:

```text
finish p02
play p04
stop
```

With:

```text
p01 -> p02 -> p03 -> p04 -> p01
```

STOP should:

```text
stop immediately
```

## Rename/Copy test

1. Copy JAZZ01 to JAZZ02.
2. Verify `/patterns/JAZZ02` exists.
3. Verify all `pNN.json` files exist.
4. Verify active group becomes JAZZ02.
5. Rename JAZZ02 to JAZZ03.
6. Verify old directory is gone and NVS active group is updated.

---

# 9. Recommended Next Steps

## First

Remove or disable normal-boot LittleFS pattern scan from `main.cpp`.

## Second

Audit all functions in `settingsStore.cpp` that still use:

```cpp
patternFileExtension
patternDirectoryPath
settingsStoreLoadPattern(...)
settingsStoreSavePattern(...)
settingsStoreLoadPatternChainSettings(...)
```

Decide which ones are still needed for settings/recovery and which are obsolete.

## Third

Add temporary serial debug for chain playback:

```text
PLAY pattern pNN
CHAIN target pNN
STOP requested
FINAL pattern pNN
STOPPED
```

## Fourth

After behavior is stable, split `uiManager.cpp` into smaller files.

---

# 10. Overall Assessment

The project is moving in the right direction. The RAM-based pattern model is the right architecture for this hardware.

The main risk is not the concept anymore, but leftover code from the earlier Local/LittleFS design. The next development cycle should focus on deleting or isolating obsolete paths instead of adding more features.

Priority order:

1. Stabilize chain save/load/playback.
2. Remove misleading LittleFS pattern startup flow.
3. Harden Card Storage copy/rename failure behavior.
4. Keep SD-first boot order documented and protected.
5. Refactor UI code only after behavior is stable.
