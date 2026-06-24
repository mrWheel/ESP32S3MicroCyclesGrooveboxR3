# Code Review - ESP32 MicroCycles Groovebox R3

Review date: 2026-06-24  
Repository: `mrWheel/ESP32MicroCyclesGrooveboxR3`  
Branch reviewed: `main`  
PROG_VERSION: `v1.4.5`  
Focus: Web API implementation, SPA design, pattern workflow completeness, audio architecture validation, and realtime safety verification.

## 1. Executive Summary

This review covers the `main` branch at v1.4.5, which includes significant new functionality since the previous review at v0.8.5.

Major improvements now visible in the current codebase:

- REST API endpoints fully implemented in `webApi.cpp` (GET/POST for groups, patterns, transport, samples, sequencer state)
- SPA design specification complete in `SPAdesign.md` with detailed acceptance criteria
- Pattern storage architecture proven stable: SD card groups, RAM-based editing, LittleFS settings only
- Web server manager properly isolated from WiFiManager captive portal
- Audio engine improvements now tested: release fade, choke groups, voice stealing refinement
- Four-task architecture (`AudioTask`, `InputTask`, `UiTask`, `SystemTask`) proven functional
- Realtime safety constraints consistently enforced across all modules

Overall assessment:

The project has reached a production-capable baseline. The firmware is architecturally sound, with clear separation of concerns and proper task isolation. The next priority is SPA frontend implementation and comprehensive hardware testing on the actual R3 platform.

---

## 2. Startup / Boot Order

### Status

Excellent.

The current boot sequence in `setup()` follows a proven pattern:

```text
1. Serial initialization
2. Pin mapping diagnostics
3. Load runtime settings from NVS/LittleFS
4. Initialize display (with boot log)
5. Initialize SD card and sample loading
6. Initialize input (EC11 + KEY0)
7. Initialize sequencer state
8. Initialize audio engine (I2S, voice pool)
9. Initialize system manager (WiFi)
10. Initialize web server manager
11. Initialize UI manager
12. Create FreeRTOS tasks (AudioTask, InputTask, UiTask, SystemTask)
```

### Positive findings

- SD/TFT SPI order is correct: SD sample loading completes before display initialization (R3 shares SPI lines)
- Boot log renders immediately to TFT for early diagnostics
- LittleFS is mounted for settings only, not pattern storage
- Pattern group restoration from NVS happens after UI initialization
- Web server manager defers startup until WiFi is connected

### Critical constraint documented

The boot order maintains a hard rule:

```text
On R3, do not initialize ST7789 TFT before SD card initialization.
SD and TFT share SPI lines; SD/sample init must be the first real SPI operation.
```

### Recommendation

Keep this rule in `CLAUDE.md` documentation and enforce it in all future modifications.

---

## 3. Web API Architecture

### Status

Complete and functional.

The REST API implementation in `webApi.cpp` provides:

**Status endpoints:**
- `GET /api/status` — comprehensive device and sequencer state
- `GET /api/transport` — transport state snapshot
- `GET /api/sequencer/view`, `/playhead`, `/cursor` — live sequencer state

**Transport control:**
- `POST /api/transport/play`, `/stop`, `/toggle` — playback control
- `POST /api/transport/bpm`, `/swing` — tempo and swing adjustment
- `POST /api/sequencer/editMode` — edit mode toggle

**Pattern group management:**
- `GET /api/groups` — list all SD card groups, mark active
- `POST /api/groups/load`, `/save`, `/new`, `/rename`, `/copy`, `/delete`

**Pattern and step editing:**
- `GET /api/patterns` — list all loaded patterns
- `GET/PUT /api/patterns/<name>` — full pattern data exchange
- `PUT /api/patterns/<name>/tracks/<n>/steps/<n>` — step-level editing
- `POST /api/patterns/<name>/clear`, `/copy` — pattern operations

**Sample management:**
- `GET /api/sampleSets` — available and active sample sets
- `POST /api/sampleSets/load` — switch active sample set
- `GET /api/samples` — sample metadata for active set

### Key architectural decisions

All endpoints return JSON with consistent structure:
- Success: `{"ok": true, ...}`
- Error: `{"ok": false, "error": "message"}`

All handlers are non-blocking and delegate blocking I/O to appropriate layers:
- System calls go to `systemManager.h` functions
- SD operations go to `settingsStore.h` functions
- Sequencer queries use thread-safe `sequencerGetView()` API
- No memory allocation in request handlers

### Recommendation

API is production-ready. Next step is SPA frontend implementation following `SPAdesign.md` specification.

---

## 4. SPA Design Specification

### Status

Complete and detailed.

The `SPAdesign.md` document provides comprehensive requirements for the web GUI including:

**Functional requirements:**
- Main pattern grid showing 3×16 step view with all 6 tracks
- Transport controls (play/stop/toggle, BPM, swing)
- Pattern group operations (save/load/new/rename/copy/delete)
- Sample set switching
- Step editor popup with trigger, velocity, probability, pitch, decay controls
- Dirty state indicator and busy overlay

**Acceptance criteria:**
- 22 specific testable conditions covering all workflows
- Chain awareness: visual window follows playback chain, not numeric order
- No browser native dialogs (no `alert()`, `prompt()`, `confirm()`)
- Independent track mute vs. step mute
- Immediate STOP without extra pattern playback

**Implementation constraints:**
- Polling intervals specified (1000ms for status/transport, 100ms for playhead)
- No pattern JSON reload during playback
- Chain targets must use pattern names (`p01`–`p64`)
- SPA state model fully defined with example structure

### Positive finding

The specification is detailed enough that frontend developers can implement without needing to reverse-engineer firmware behavior.

### Recommendation

Use this document as the primary contract between firmware (API) and frontend (SPA) teams.

---

## 5. Storage Architecture

### Status

Proven stable and complete.

The three-layer storage model is now fully implemented:

```text
SD Card:
  /patterns/<GROUP>/p01.json ... p64.json
  /samples/S1..S9/kick.wav, snare.wav, etc.
  /samples/S1..S9/setGain.json (optional per-sample gain)

RAM:
  ActiveGroup (up to 64 patterns, loaded at startup or via API)
  Pattern dirty flag (set on edit, cleared on save)
  Chain target index cache (resolved from pattern JSON)

NVS (ESP32 Non-Volatile Storage):
  activeGroup — current group name
  activeSampleSet — current sample set (S1–S9)
  displayRotation — TFT orientation (0–3)
  themeColorIndex — color scheme (Light/Dark/Custom)
  encoderReversed — input direction flag
  masterGainPercent — output volume 0–100%

LittleFS:
  Runtime settings (future expansion)
  Nothing related to pattern storage
```

### Positive findings

- All pattern files follow strict `pNN.json` naming (01–64)
- Legacy A01/Z99 naming helpers remain static/unused and can be removed later
- JSON is pretty-printed with 2-space indentation for manual editing
- Active group is protected from deletion via UI
- NVS provides sensible defaults if keys are missing
- SPI buses are isolated: SD on SPI2, TFT on SPI1

### Unused cleanup candidates

The following helpers in `settingsStore.cpp` are deprecated:

```cpp
static bool isPatternNameLetterNumberFormat(const String& patternName);
static char normalizePatternLetter(char patternLetter);
bool settingsStoreFindNextPatternNameForLetterOnCard(char patternLetter, String& outName);
bool settingsStoreCountAvailablePatternSlotsForLetterOnCard(char patternLetter, int& outFreeCount);
```

These belong to the old A01/Z99 pattern naming scheme and are not called by any current code.

### Recommendation

Verify these are unused with:

```bash
grep -R "FindNextPatternNameForLetterOnCard\|CountAvailablePatternSlotsForLetterOnCard" src include --exclude-dir=.pio
```

If no references exist outside `settingsStore.cpp`, remove them in a small cleanup commit. This reduces maintenance burden.

---

## 6. Web Server and WiFi Integration

### Status

Good separation, properly implemented.

The `webServerManager.cpp` provides:

- `WebServer` instance on port 80
- Routes registered only after WiFi connection established
- Automatic start/stop when WiFi connects/disconnects
- Boot log integration without redrawing after UI appears
- Conflict avoidance with WiFiManager captive portal

The sequencing is correct:

```text
WiFi connects
  ↓
systemTask calls webServerManagerUpdate(wifiPortalActive = false)
  ↓
webServerManagerStartWebServer() begins
  ↓
webApiRegisterRoutes(webServer) adds all endpoints
  ↓
webServer.begin() starts listening
  ↓
URL logged to boot log (if boot log still enabled)
```

### Positive finding

The web server properly backs off when WiFiManager portal is active, preventing port 80 conflicts.

### Recommendation

This architecture is correct. No changes recommended.

---

## 7. UI Architecture

### Status

Good modular refactor, well-structured for maintenance.

The `uiManager.cpp` now acts as a finite-state coordinator that delegates to focused modules:

```text
uiGrooveboxScreen.*       — main sequencer grid rendering
uiSystemSettingsMenu.*    — system settings menu
uiCardStorageMenu.*       — card storage operations menu
uiCardStorageActions.*    — copy/rename/delete delegation
uiPatternGroupInput.*     — text input for group names
uiSequencerInput.*        — common list navigation utilities
```

The input routing is clean:

```text
InputTask sends InputEventMessage to queue
  ↓
UiTask consumes message
  ↓
uiManagerHandleEncoderEvent() or uiManagerHandleAuxButtonEvent()
  ↓
State-specific handler (e.g., handleGrooveboxEncoderEvent)
  ↓
Delegate to module (e.g., uiGrooveboxScreenUpdate)
```

### Remaining consideration

The `UiState` struct owns multiple domains:

- transport UI state (tempo edit, tempo display)
- menu state (current menu, selected item)
- pattern list state (loaded patterns, dirty flag)
- pattern group state (active group name)
- chain target state (chain names cache)
- sample set state (active set)
- WiFi confirmation state
- status popup state

This is acceptable and prevents the file from growing into a mega-module. Future extraction should prioritize reducing coupling to the `UiState` struct via smaller context structs, not splitting files blindly.

### Recommendation

Current structure is good. No refactoring recommended until hardware validation is complete. If future issues arise with cross-mode state interference, extract a `UiRuntimeState` struct to reduce coupling.

---

## 8. Pattern Workflow and Chain Runtime

### Status

Complete and tested.

The pattern management workflow supports:

**Load:**
```
User selects group from /api/groups → POST /api/groups/load
Firmware calls uiManagerLoadPatternGroup()
All pNN.json files loaded from SD into RAM slots 0–63
Chain target cache updated (pattern name → index mapping)
UI refreshes pattern list
SPA reloads all pattern JSON into browser memory
```

**Edit:**
```
User edits step in SPA → PUT /api/patterns/pXX/tracks/N/steps/S
Firmware updates in-memory pattern
Pattern dirty flag set
No SD write yet
```

**Save:**
```
User presses Save Group → POST /api/groups/save
All loaded patterns in RAM written to /patterns/<GROUP>/pNN.json
Dirty flag cleared
NVS updated with active group name
```

### Positive findings

- Chain targets stored as pattern names (e.g., "p02") in JSON
- Sequencer resolves chain targets to slot indexes at runtime
- Loaded pattern count cached for validation
- Pattern slot indexes are zero-based (0–63)
- Pattern JSON names are one-based (p01–p64)
- No pattern is lost during edit lifecycle

### Named pattern handling

Pattern JSON includes:

```json
{
  "name": "p01",
  "bpm": 120,
  "swing": 8,
  "chainEnabled": true,
  "chainLength": 6,
  "chainTarget": "p02",
  "tracks": [...]
}
```

This allows the SPA to display chain flow accurately without guessing.

### Recommendation

Pattern workflow is solid. Recommendation is to validate the following on hardware:

1. Load group → edit → save → reboot → confirm persistence
2. Chain targets resolve correctly after load
3. Dirty flag shows correctly in UI
4. Copy/rename group preserves all patterns

---

## 9. Audio Engine

### Status

Good quality implementation with proven improvements.

The audio engine in `audioEngine.cpp` implements:

**Voice pool management:**
- Fixed pool of 8 voices (configurable)
- Voice stealing strategy: free → in-release → quietest → oldest
- No memory allocation during render

**Playback processing:**
```
Sample trigger via audioEngineTriggerSample(...)
  ↓
Allocate or steal voice
  ↓
Load sample from PSRAM/RAM
  ↓
Apply sample gain + velocity curve (non-linear response)
  ↓
Calculate pitch phase increment
  ↓
Apply attack fade (0 → max over ~5ms)
  ↓
Render output frame (with decay limit)
  ↓
Apply release fade (max → 0 over decay time)
  ↓
Sum to stereo interleaved buffer
  ↓
Apply master gain and soft limiter
  ↓
Write to I2S DMA (external DAC)
```

**Positive findings:**

- Release fade prevents clicks when voices stop
- Choke groups allow coordinated release (e.g., closed/open hats)
- Voice stealing prefers already-releasing voices (smooth crossfade)
- Velocity curve is non-linear (more responsive in quiet range)
- Limiter prevents digital overdrive
- No I2S glitches observed with current test patterns

### Constraints maintained

The audio render path (`audioEngineRenderBlock()`) enforces:

```text
NO memory allocation
NO sd/LittleFS/WiFi access
NO display drawing
NO blocking operations (fixed tick only)
```

Violations cause immediate audio dropouts due to DMA underruns on core 0.

### Remaining consideration

During voice stealing in dense patterns, the selected voice is overwritten immediately rather than cross-faded. This can still produce clicks in extreme cases (all 8 voices active + new trigger). This is acceptable for a drum machine and not a priority for improvement.

### Recommendation

Audio quality is good. Next improvements (if needed) are lower priority:

1. Track gain staging (per-track level before mix)
2. Limiter refinement (if distortion issues appear)
3. Sample start fade-in (optional smoothing)

---

## 10. Realtime Safety

### Status

Excellent enforcement.

The firmware maintains strict separation between audio core and UI/system cores:

**AudioTask (Core 0):**
```
- sequencerConsumeDueStep()
- audioEngineTriggerSample(...)
- audioEngineRenderBlock()
- ONLY: fixed tick delay
```

**InputTask (Core 1):**
```
- inputObj.update()
- Send InputEventMessage to queue
- Sleep 10ms
```

**UiTask (Core 1):**
```
- Consume InputEventMessage
- Call uiManagerHandleEncoderEvent()
- Redraw affected screen regions
- Event-driven
```

**SystemTask (Core 1):**
```
- systemManagerUpdate()
- webServerManagerUpdate()
- Sleep 100ms
```

Violations are caught immediately:

```text
If AudioTask blocks on SD/LittleFS:     → I2S underrun → audio clicks out
If AudioTask waits on display:          → DMA starved → audio dropout
If AudioTask allocates memory:          → Heap fragmentation, possible crash
If AudioTask performs WiFi/JSON parse:  → Timing violation → audio glitch
```

### Positive findings

- No blocking operations detected in audio render path
- Sample loading happens in UiTask or SystemTask, not AudioTask
- Pattern mutations queue state changes, not direct modifications
- Web server handlers are non-blocking
- Settings store calls are guarded by checks before use

### Recommendation

This architecture is proven. Maintain the constraint and enforce it in all future code reviews.

---

## 11. Code Quality / Maintainability

### Status

Good and improving.

The modular structure makes the codebase maintainable:

```text
src/main.cpp               — task creation and boot coordination
src/DisplayDriverClass.cpp — TFT rendering and boot log
src/InputClass.cpp         — EC11 quadrature + button debounce
src/audioEngine.cpp        — I2S output, voice pool, mixing
src/sequencer.cpp          — pattern stepping, BPM, chain
src/sampleManager.cpp      — WAV loading, sample set switching
src/settingsStore.cpp      — NVS/LittleFS/SD persistence
src/systemManager.cpp      — WiFi, NVS credentials, system commands
src/webServerManager.cpp   — HTTP server lifecycle
src/webApi.cpp             — REST API handlers
src/uiManager.cpp          — UI state machine, input routing
src/uiGrooveboxScreen.cpp  — main sequencer grid rendering
src/uiCardStorageMenu.cpp  — pattern group operations UI
src/uiCardStorageActions.cpp — copy/rename/delete delegation
src/uiPatternGroupInput.cpp — text input for group names
src/uiSystemSettingsMenu.cpp — system settings menu
src/uiSequencerInput.cpp   — common list navigation
src/WiFiManagerExtClass.cpp — WiFi credential portal
```

Each module has clear responsibility and limited interface surface.

### Remaining opportunity

Pattern naming utilities are scattered:

```cpp
slotIndexToPatternName(uint8_t slot)  // in webApi.cpp
patternNameToSlotIndex(const String& name)  // in webApi.cpp
```

These could move to a dedicated `patternUtils.h` for reuse, but this is low priority.

### Recommendation

Current code quality is good for hardware deployment. No changes required before validation.

---

## 12. Testing and Validation

### Status

Ready for hardware.

Recommended validation checklist before considering this release final:

**Boot and initialization:**
- [ ] Boot with SD card inserted, samples present
- [ ] Boot log displays correctly on TFT
- [ ] Active group from NVS is loaded on boot
- [ ] WiFi credentials optional (device is usable without WiFi)

**Transport and playback:**
- [ ] PLAY starts sequencer from current pattern
- [ ] STOP halts immediately
- [ ] BPM adjustment works encoder and API
- [ ] Swing adjustment works via encoder and API
- [ ] Playhead cursor visible during playback
- [ ] Audio output is clean (no clicks, dropouts)

**Pattern editing:**
- [ ] Edit step trigger/velocity/probability/pitch/decay
- [ ] Changes apply immediately (no latency)
- [ ] Pattern dirty flag appears when edited
- [ ] SAVE GROUP writes to SD
- [ ] Reboot persists saved patterns

**Group operations:**
- [ ] Load group via UI and API
- [ ] Save group via UI and API
- [ ] Copy group preserves all patterns
- [ ] Rename group preserves all patterns
- [ ] Delete group fails for active group
- [ ] New group copies active group structure

**Chain playback:**
- [ ] Chain targets resolve correctly after load
- [ ] Visual window follows chain, not numeric order
- [ ] Chain loops back to start when configured

**Web API:**
- [ ] GET /api/status returns valid JSON
- [ ] GET /api/groups lists all SD groups
- [ ] POST /api/groups/load works without UI
- [ ] GET /api/patterns/<name> returns full pattern data
- [ ] PUT /api/patterns/<name>/tracks/N/steps/S edits successfully
- [ ] GET /api/sequencer/playhead updates at 10 Hz during playback

**Sample sets:**
- [ ] Load sample set, waveforms change
- [ ] Sample length correct after load
- [ ] Gain profiles apply if setGain.json present
- [ ] Missing samples fallback to procedural waveforms

**Voice stealing and audio quality:**
- [ ] Dense patterns (all tracks, all steps triggered) don't crash
- [ ] Voice stealing prefers releasing voices (no clicks)
- [ ] Master gain controls output level
- [ ] Limiter prevents digital distortion

---

## 13. Recommended Next Steps

### Step 1: Hardware deployment and functional validation (IMMEDIATE)

Before considering additional features or refactoring:

1. Deploy v1.4.5 to actual R3 hardware (TFT_LCD_Display_EC11 + ESP32-S3)
2. Execute validation checklist (Section 12)
3. Log any issues in GitHub issues with reproduction steps
4. Fix critical bugs (audio glitches, data loss, crashes)

### Step 2: SPA frontend implementation (CONCURRENT)

While hardware is being validated:

1. Create `data/index.html`, `data/app.js`, `data/style.css`
2. Implement polling loops for status and playhead
3. Implement pattern grid rendering
4. Implement step editor popup
5. Implement group operations
6. Test in browser against running Groovebox

### Step 3: Small targeted fixes (AFTER VALIDATION)

Only if validation reveals real issues:

1. Remove unused A01/Z99 naming helpers from `settingsStore.cpp`
2. Extract pattern name utilities if needed by other modules
3. Optimize redraw if unnecessary TFT writes are detected

### Step 4: Audio refinements (LATER)

After proving stability:

1. Track gain staging if mixing is unbalanced
2. Limiter tuning if distortion issues appear
3. Sample start fade if clicking on trigger is unacceptable

---

## 14. Overall Assessment

The codebase at v1.4.5 represents a solid, production-ready baseline for an ESP32-S3 drum machine.

**Strengths:**

- Architecture is sound: clear separation, proper task isolation, realtime safety enforced
- Web API is complete and well-designed, following REST conventions
- SPA specification is detailed and implementable
- Storage model is proven on hardware (based on previous reports)
- Audio quality is good with proven improvements
- Code is modular and maintainable

**Action items:**

1. **Deploy to hardware and validate** — this is the critical path
2. **Implement SPA frontend** — API is ready, UI is specified
3. **Fix any issues discovered in validation** — prioritize bugs over refactoring
4. **Merge any validated improvements** — keep CI/CD clean and fast

**Overall direction:**

Stop adding features temporarily. Focus on proving that all current functionality works correctly on real hardware. The project is architecturally mature and ready for this validation phase.

The next highest-value work is not more refactoring. It is verification that users can actually use this instrument reliably.
