# ESP32 MicroCycles Groovebox R3
# Developer Build Guide

**Current firmware version:** `v0.8.5`  
**Source of version:** `src/main.cpp`

```cpp
//-- PROG_VERSION.
const char* PROG_VERSION = "v0.8.5";
```

This guide describes the current codebase and is intended to let a developer rebuild the project from zero.

---

## 1. Scope

This guide targets:

- Hardware revision: R3
- Firmware environment: `ESP32GrooveboxR3`
- Firmware version: `v0.8.5`
- Runtime pattern model: RAM-based editing
- Persistent pattern storage: SD Card pattern groups
- Sample storage: SD Card sample sets
- Configuration storage: NVS and LittleFS settings only

This guide does not target R2.

---

## 2. High-Level Architecture

The current system is a realtime embedded groovebox.

The current storage model is:

```text
SD Card  = persistent pattern groups and sample sets
RAM      = active editable patterns
NVS      = selected runtime values
LittleFS = settings/configuration only
```

The normal workflow is:

```text
Boot
  -> SD initializes first
  -> active sample set is loaded
  -> runtime settings are read
  -> display starts
  -> sequencer/audio/system/UI start
  -> active Card pattern group is loaded into RAM
  -> editing happens in RAM
  -> Save Pattern writes RAM patterns back to SD Card
```

---

## 3. Hardware

Required hardware:

- ESP32 R3 target with PSRAM support
- TFT LCD DISPLAY EC11 piggy back BOARD
- ST7789 display, 320x240
- Rotary encoder with push button
- KEY0 button
- I2S DAC or DAC/amplifier board
- microSD card module in SPI mode
- microSD card, FAT16 or FAT32
- USB cable for flashing and serial monitor

---

## 4. Software Requirements

Required software:

- Visual Studio Code
- PlatformIO extension
- PlatformIO Core CLI available as `pio`
- Repository opened as a PlatformIO project

Build environment:

```text
ESP32GrooveboxR3
```

Build command:

```bash
pio run -e ESP32GrooveboxR3
```

---

## 5. R3 Pin Mapping

Pin mapping is defined in `platformio.ini`.

### Display

```text
PIN_TFT_BLK  = 2
PIN_TFT_RST = 4
PIN_TFT_CS  = 5
PIN_TFT_SCLK = 18
PIN_TFT_MOSI = 23
PIN_TFT_DC  = 15
```

### Input

```text
PIN_ENC_BTN = 35
PIN_KEY0    = 0
PIN_ENC_A   = 36
PIN_ENC_B   = 39
```

### Audio / I2S

```text
PIN_I2S_BCLK = 26
PIN_I2S_WS   = 25
PIN_I2S_DOUT = 27
PIN_I2S_SD   = 14
```

`PIN_I2S_SD` is used as enable/shutdown control for the audio board and is driven HIGH by firmware.

### SD Card / SPI

```text
PIN_SD_CS   = 13
PIN_SD_SCK  = 18
PIN_SD_MISO = 19
PIN_SD_MOSI = 23
```

Important R3 hardware rule:

```text
TFT and SD share SPI lines.
SD card initialization must happen before ST7789 display initialization.
Do not move display initialization before sampleManagerInit().
```

---

## 6. SD Card Layout

The current SD Card layout is:

```text
/
  /patterns
    /<GROUPNAME>
      p01.json
      p02.json
      p03.json
      ...
      pNN.json

  /samples
    /S1
      kick.wav
      snare.wav
      ch.wav
      oh.wav
      tone.wav
      metal.wav
      setGain.json

    /S2
      kick.wav
      snare.wav
      ch.wav
      oh.wav
      tone.wav
      metal.wav
      setGain.json

    ...
    /S9
      kick.wav
      snare.wav
      ch.wav
      oh.wav
      tone.wav
      metal.wav
      setGain.json
```

Pattern groups are stored under:

```text
/patterns/<GROUPNAME>/
```

Patterns inside a group are strict `pNN.json` files:

```text
p01.json
p02.json
...
p99.json
```

The active pattern group name is stored in NVS.

---

## 7. Sample Sets

Samples are loaded from the active sample set:

```text
/samples/Sn/
```

where `Sn` is `S1` through `S9`.

Required WAV files per sample set:

```text
kick.wav
snare.wav
ch.wav
oh.wav
tone.wav
metal.wav
```

Accepted WAV format:

```text
PCM audioFormat = 1
44.1 kHz only
16-bit or 24-bit
mono or stereo
```

Samples are decoded from SD into sample buffers. PSRAM is used when available.

If a sample is missing or invalid, the firmware uses generated fallback samples for that slot.

---

## 8. setGain.json

Each sample set may contain:

```text
setGain.json
```

This file defines per-sample gain values used by the mixer.

Supported JSON forms:

```json
{
  "setGain": {
    "kick": 100,
    "snare": 90,
    "ch": 70,
    "oh": 80,
    "tone": 100,
    "metal": 85
  }
}
```

Missing or invalid gain files are not fatal. The default value is 100 percent per sample.

---

## 9. NVS Responsibilities

NVS stores persistent runtime choices that should survive reboot.

Current NVS responsibilities include:

```text
active pattern group name
active sample set name
display rotation
theme color index
encoder order
WiFi credentials
```

The current runtime settings are loaded during boot after SD/sample initialization.

---

## 10. LittleFS Responsibilities

LittleFS may be used for settings/configuration support, diagnostics, or legacy-safe settings handling.

---

## 11. Boot Sequence

Current boot sequence in `src/main.cpp`:

```text
Serial start
print version
log pins
check pin conflicts
optional SD_SMOKE_TEST
sampleManagerInit()
settingsStoreLoadRuntimeSettings()
input queue creation
input.begin()
apply encoder order
display init / boot status
sequencerInit()
audioEngineInit()
systemManagerInit()
uiManagerInit()
uiManagerUpdate()
start AudioTask
start UiTask
start InputTask
start SystemTask
loop() remains fallback-only
```

Important:

```text
sampleManagerInit() must happen before display initialization.
```

Reason:

```text
SD and TFT share SPI lines on R3.
```

---

## 12. Tasks

The firmware uses FreeRTOS tasks:

### AudioTask

Runs audio timing and rendering.

Responsibilities:

```text
sequencerConsumeDueStep()
audioEngineTriggerSample()
audioEngineRenderBlock()
```

AudioTask must remain realtime-safe.

It must not:

```text
allocate memory
access filesystem
access WiFi
redraw display
block on UI work
log continuously
```

### UiTask

Consumes input messages and calls UI handlers.

Responsibilities:

```text
uiManagerHandleEncoderEvent()
uiManagerHandleAuxButtonEvent()
uiManagerUpdate()
```

### InputTask

Polls encoder and buttons.

Responsibilities:

```text
input.update()
input.getEncoderEvent()
input.getAuxButtonEvent()
queue events to UiTask
```

### SystemTask

Runs system/WiFi background work.

Responsibilities:

```text
systemManagerUpdate()
```

### loop()

`loop()` stays minimal. It only provides fallback behavior if task creation fails.

---

## 13. Runtime Pattern Model

Patterns are edited in RAM.

A Card pattern group is loaded into sequencer memory.

The UI tracks:

```text
loaded pattern names
chain targets
active pattern name
active group name
```

The sequencer tracks:

```text
patterns[]
loadedPatternCount
activePatternIndex
playingPatternIndex
chainTargetIndex[]
chainTargetValid[]
```

The UI stores chain targets as pattern names such as `p02`.

The sequencer stores chain targets as slot indexes.

Whenever UI chain targets change, the sequencer chain table must be synchronized.

---

## 14. Pattern JSON Schema

Current top-level pattern fields include:

```text
version
name
bpm
swing
chainEnabled
chainLength
chainTarget
masterLevel
tracks
```

Each track contains:

```text
name
machine
sample
mute
solo
level
pan
steps
```

Each step contains:

```text
trigger or trig
velocity
probability
lockEnabled or locks.enabled
lockPitch or locks.pitch
lockDecay or locks.decay
microTiming
flam
accent
ratchet
reverse
sampleStart
sampleEnd
pan
chokeGroup
```

The loader accepts both old and newer key names for trigger and lock fields where supported.

---

## 15. UI Philosophy

The Groovebox UI must behave like a dedicated realtime musical instrument.

Priorities:

```text
rhythm
immediacy
live usability
low cognitive load
muscle memory
fast editing
```

Avoid:

```text
deep menus
desktop-style workflows
modal confusion
spreadsheet-like editing
fullscreen redraws during playback
```

The main sequencer screen [Groovebox] is the primary interaction surface.

---

## 16. Main Groovebox Screen

The Groovebox screen shows:

```text
header with BPM, swing, transport state, viewed pattern
six track rows
contextual parameter/edit row
footer with step, playing pattern, next chain target, group name
```

Track rows:

```text
KICK
SNARE
CH
OH
TONE
METAL
```

Parameter pages:

```text
TRIG
VEL
PITCH
DECAY
PROB
MUTE
CHAIN
```

---

## 17. Controls

Controls:

```text
rotary encoder rotation
encoder button
KEY0
```

Normal mode:

```text
Encoder rotate = select track / navigate pattern edge
Encoder short  = enter edit mode
Encoder medium = open Tempo Edit popup
Encoder long   = open System Settings
KEY0 short     = play/stop
KEY0 medium    = tempo edit BPM
KEY0 long      = tempo edit Swing
```

Edit mode:

```text
TRIG  = move cursor / toggle step
VEL   = edit velocity
PITCH = edit lock pitch
DECAY = edit lock decay
PROB  = edit probability
MUTE  = edit selected track mute
CHAIN = edit chain settings
```

---

## 18. Transport Strategy

Start behavior:

```text
STOP -> PLAY always starts at p01
```

Stop behavior:

If all loaded patterns are included in the active chain loop:

```text
PLAY -> STOP stops immediately
```

If not all loaded patterns are included:

```text
PLAY -> STOP
  finish currently playing pattern
  play highest loaded pattern
  stop
```

Example:

```text
Loaded: p01 p02 p03 p04
Chain:  p01 -> p02 -> p03 -> p02

STOP during p02:
  finish p02
  play p04
  stop
```

---

## 19. Audio Architecture

Audio path:

```text
Sequencer step events
  -> audioEngineTriggerSample()
  -> fixed voice pool
  -> mono mixer
  -> master gain
  -> headroom limiter
  -> stereo duplicated output buffer
  -> I2S DMA
  -> DAC/amplifier
```

Current audio improvements:

```text
fixed voice pool
sample overlap
release fade
choke group release
voice stealing policy
velocity curve
soft limiter / headroom
sample set gain file
```

Current hardware output is effectively mono, even though the I2S buffer is stereo interleaved.

---

## 20. Build Flags

Important build flags in `platformio.ini`:

```text
TEST_TONE
TEST_TONE_FREQUENCY_HZ
AUDIO_MASTER_GAIN_PERCENT
AUDIO_HEADROOM_LIMITER_ENABLE
AUDIO_HEADROOM_LIMITER_THRESHOLD_PERCENT
SD_SMOKE_TEST
NO_DAC_HARDWARE
DISPLAY_DEBUG_INFO
BOARD_HAS_PSRAM
-mfix-esp32-psram-cache-issue
```

Keep diagnostic flags disabled for normal use:

```text
TEST_TONE
SD_SMOKE_TEST
NO_DAC_HARDWARE
DISPLAY_DEBUG_INFO
```

---

# 21. Source File Overview

This section describes each `.cpp` file in `src/`, beginning with `main.cpp`.

The function lists are intended as a developer orientation. Static helper functions may change over time, but the responsibilities should remain stable.

---

## 21.1 `src/main.cpp`

Purpose:

`main.cpp` is the firmware entry point and runtime orchestrator.

Responsibilities:

```text
define PROG_VERSION
initialize Serial
log hardware pins
run optional SD smoke test
initialize samples before display
load runtime settings
initialize input
initialize display boot screen
initialize sequencer
initialize audio engine
initialize system manager
initialize UI manager
start FreeRTOS tasks
provide fallback loop behavior
```

Important functions:

```text
bootStatusPush()
bootStatusInit()
buildFilesystemChildPath()
logFilesystemDirectoryRecursive()
displayFilesystemDirectoryRecursive()
runSdSmokeTestAndHalt()
logPinConflictWarnings()
runInputUiFallbackCycle()
audioTask()
inputTask()
uiTask()
systemTask()
setup()
loop()
```

Developer notes:

- Do not move `displayInit()` before `sampleManagerInit()`.
- `loop()` must remain minimal and delegated.
- Audio runs on its own task.
- UI and input communicate through a queue.
- `SD_SMOKE_TEST` intentionally halts after diagnostics.

---

## 21.2 `src/audioEngine.cpp`

Purpose:

Owns I2S output, voice mixing, sample triggering, gain, limiter, and output block rendering.

Responsibilities:

```text
initialize I2S
manage fixed voice pool
trigger sample playback
mix active voices
apply sample gain
apply velocity level
apply master gain
apply limiter
write audio block to I2S DMA
report audio stats
optional test tone
```

Important functions:

```text
applyHeadroomLimiter()
applyMasterGainAndClamp()
mixNextFrame()
audioEngineInit()
audioEngineIsOutputReady()
audioEngineTriggerSample(SampleId, uint8_t, uint16_t, int8_t, uint8_t)
audioEngineTriggerSample(SampleId, uint8_t)
audioEngineRenderBlock()
audioEngineSetTestToneEnabled()
audioEngineGetStats()
```

Developer notes:

- No dynamic allocation in realtime render path.
- Do not access SD, LittleFS, WiFi, or display from audio rendering.
- `audioEngineRenderBlock()` must stay deterministic.
- The output buffer is stereo interleaved but currently receives duplicated mono samples.

---

## 21.3 `src/sampleManager.cpp`

Purpose:

Owns SD initialization, sample set selection, WAV decoding, PSRAM allocation, fallback samples, and per-sample gain loading.

Responsibilities:

```text
initialize SD card
track active sample set
list sample sets
load sample set from SD
load setGain.json
decode WAV files
allocate sample buffers
prefer PSRAM when available
build fallback samples when needed
provide sample slots to audio engine
```

Important functions:

```text
getSampleSetDir()
loadSampleGainPercent()
sampleManagerSampleSetExists()
sampleManagerListSampleSets()
sampleManagerLoadSampleSet()
sampleManagerSetActiveSampleSet()
sampleManagerGetActiveSampleSet()
sampleManagerGetSampleGainPercent()
readLe16()
readLe32()
logSampleAllocationHeapState()
buildFallbackSample()
logSdDirectoryRecursive()
parseWavLayoutFromFile()
readMonoSampleFromFile()
loadSampleFromSdPath()
initSdCard()
sampleManagerInit()
sampleManagerIsSdCardReady()
sampleManagerGetSample()
```

Developer notes:

- `setGain.json` belongs inside the active sample set directory.
- Active sample set must be selected before loading `setGain.json`.
- Missing samples fall back to generated internal waveforms.
- Keep SD diagnostics behind debug flags where possible.

---

## 21.4 `src/sequencer.cpp`

Purpose:

Owns realtime pattern playback, step timing, chain advancement, transport state, and editable pattern memory.

Responsibilities:

```text
store RAM pattern slots
advance step timing
apply swing timing
generate track trigger masks
apply probability
apply velocity and decay lock output level
track active and playing pattern indexes
handle chain target indexes
handle stop-after-final-pattern behavior
provide editing functions to UI
```

Important functions:

```text
clampToByte()
getSelectedStep()
loadDefaultPattern()
clearPattern()
seedPatternByNumber()
getStepIntervalUs()
sequencerInit()
sequencerConsumeDueStep()
sequencerTogglePlay()
sequencerStopImmediately()
sequencerSetLoadedPatternCount()
sequencerClearPatternChainTargets()
sequencerSetPatternChainTarget()
sequencerRequestStopAfterFinalPattern()
sequencerToggleEditMode()
sequencerMoveSelection()
sequencerMoveCursor()
sequencerSetActivePatternIndex()
sequencerToggleCurrentStep()
sequencerAdjustCurrentStepVelocity()
sequencerAdjustCurrentStepProbability()
sequencerAdjustCurrentStepLockPitch()
sequencerAdjustCurrentStepLockDecay()
sequencerToggleMuteForSelectedTrack()
sequencerAdjustBpm()
sequencerAdjustSwing()
sequencerAdjustChainLength()
sequencerToggleChainEnabled()
sequencerGetView()
sequencerExportPattern()
sequencerExportPatternFromSlot()
sequencerImportPattern()
sequencerImportPatternToSlot()
sequencerCreatePatternSlot()
sequencerDeletePatternSlot()
```

Developer notes:

- Sequencer functions are shared between UI and AudioTask.
- Critical sections protect shared sequencer state.
- AudioTask calls `sequencerConsumeDueStep()`.
- UI must synchronize chain target names into sequencer slot indexes.

---

## 21.5 `src/settingsStore.cpp`

Purpose:

Owns persistent settings, NVS access, runtime settings, pattern JSON serialization, and SD Card pattern group operations.

Responsibilities:

```text
read/write NVS runtime values
read/write WiFi credentials
read/write active pattern group
read/write active sample set
mount LittleFS for settings support
build pattern JSON documents
parse pattern JSON documents
list SD Card pattern groups
list patterns inside a group
load pattern JSON from SD
save pattern JSON to SD
delete pattern JSON from SD
copy pattern group on SD
rename pattern group on SD
```

Important functions:

```text
openNvsHandle()
settingsStoreGetActivePatternGroup()
settingsStoreSetActivePatternGroup()
settingsStoreGetActiveSampleSet()
settingsStoreSetActiveSampleSet()
settingsStoreGetDisplayRotation()
settingsStoreSetDisplayRotation()
settingsStoreGetEncoderOrder()
settingsStoreSetEncoderOrder()
settingsStoreGetThemeColorIndex()
settingsStoreSetThemeColorIndex()
settingsStoreGetWifiCredentials()
settingsStoreSetWifiCredentials()
normalizePatternName()
normalizePatternSlotName()
patternNameFromPath()
defaultRuntimeSettings()
ensureSettingsFsMounted()
isSettingsFsUsable()
buildPatternJsonDocument()
parsePatternJsonDocument()
readChainSettingsFromJson()
settingsStoreLoadRuntimeSettings()
settingsStoreSaveRuntimeSettings()
settingsStoreListPatternGroupsOnCard()
settingsStoreListPatternsInGroupOnCard()
settingsStoreLoadPatternFromCard()
settingsStoreSavePatternToCard()
settingsStoreDeletePatternFromCard()
settingsStoreRenamePatternGroupOnCard()
settingsStoreCopyPatternGroupOnCard()
settingsStoreGetLittleFsUsage()
```

Developer notes:

- Do not reintroduce normal LittleFS pattern storage.
- Card patterns must be saved as `pNN.json`.
- Avoid writing extensionless `pNN` files.
- Pattern JSON schema should be extended, not replaced.

---

## 21.6 `src/systemManager.cpp`

Purpose:

Owns system-level commands, WiFi manager control, and background system updates.

Responsibilities:

```text
initialize system manager
read stored WiFi info
control WiFi manager enable/disable
queue system commands
erase WiFi credentials
start WiFi manager portal
track WiFi portal status
run background system update
```

Important functions:

```text
systemManagerInit()
systemManagerUpdate()
systemManagerQueueCommand()
systemManagerIsWifiPortalActive()
systemManagerGetStoredWifiInfo()
```

Developer notes:

- System commands should not run in the audio task.
- WiFi manager actions may interrupt normal instrument workflow.
- Storage/system actions may stop playback when needed.

---

## 21.7 `src/DisplayDriverClass.cpp`

Purpose:

Owns ST7789 drawing and generic display primitives.

Responsibilities:

```text
initialize display
set rotation
set theme
draw headers
draw list screens
draw popups
draw number editors
draw enum editors
draw text input
draw status/timer screens
draw selection overlays
perform partial popup row updates
```

Important functions and methods:

```text
displayInit()
displaySetRotation()
displayGetRotation()
displaySetThemeColorIndex()
displayGetThemeColorIndex()
displayDrawListScreen()
displayDrawListScreenWithDisabledItems()
displayRefreshHeaderIfNeeded()
displayDrawNumberEditor()
displayDrawEnumEditor()
displayDrawTextInput()
displayDraw24hTimerEditor()
displayDrawFieldInput()
DisplayDriver::drawStatusScreen()
DisplayDriver::drawSelectionOverlay()
DisplayDriver::updateSelectionOverlayRow()
DisplayDriver::drawMessage()
DisplayDriver::drawListScreen()
DisplayDriver::setTheme()
DisplayDriver::setRotation()
```

Developer notes:

- UI modules should use display-driver functions instead of drawing directly where possible.
- Partial redraws should use the same geometry as the full popup draw.
- Avoid full-screen redraws during playback unless necessary.

---

## 21.8 `src/InputClass.cpp`

Purpose:

Owns encoder and button polling.

Responsibilities:

```text
initialize encoder/button pins
poll encoder rotation
detect encoder button events
detect KEY0 events
debounce inputs
classify short/medium/long presses
provide events to input task
```

Important methods:

```text
InputClass::begin()
InputClass::update()
InputClass::getEncoderEvent()
InputClass::getAuxButtonEvent()
InputClass::setEncoderDirectionReversed()
```

Developer notes:

- Input polling happens in `InputTask`.
- UI should respond to events, not raw pins.
- Keep debounce and event classification inside this module.

---

## 21.9 `src/WiFiManagerExtClass.cpp`

Purpose:

Wraps WiFiManager behavior and runtime enable/disable handling.

Responsibilities:

```text
start WiFi portal when requested
track whether portal is active
save WiFi credentials
disable WiFi manager during normal boot when needed
```

Important methods:

```text
WiFiManagerExtClass::begin()
WiFiManagerExtClass::update()
WiFiManagerExtClass::startPortal()
WiFiManagerExtClass::isPortalActive()
WiFiManagerExtClass::setDisabled()
```

Developer notes:

- WiFi should not start automatically during normal instrument boot unless explicitly requested.
- WiFi actions are System Settings actions and may stop playback.

---

## 21.10 `src/uiManager.cpp`

Purpose:

Owns global UI state and routes UI events to focused modules.

Responsibilities:

```text
own UiState
route encoder events
route button events
manage menu open/close state
manage tempo edit state
manage edit popup state
manage pattern list state
manage Card Storage workflow state
synchronize UI chain names to sequencer indexes
coordinate pattern group load/save/copy/rename/delete
coordinate sample set selection
coordinate WiFi confirmation UI
call render modules
```

Important functions:

```text
getLoadedPatternSlotCount()
patternSlotNumberFromName()
patternSlotIndexFromName()
assignActivePatternNameToCurrentSlot()
syncActivePatternNameFromSlot()
syncSequencerChainTargetsFromUi()
areAllLoadedPatternsIncludedInPlaybackChain()
popupSelectionFromParameterPage()
popupSelectionToParameterPage()
applyEditPopupValueDelta()
selectNextPatternInSeries()
refreshChainSeriesPatternCache()
saveChainSettingsForPattern()
loadChainSettingsForActivePattern()
flushPendingChainSettings()
refreshPatternList()
showPatternStatus()
drawBusyPopupNow()
commitPatternGroupNameInput()
refreshSampleSetList()
loadSelectedSampleSetFromMenu()
deleteSelectedPatternFromMemory()
addPatternInMemory()
moveGrooveboxCursorAcrossPatterns()
loadCardPatternGroupIntoMemory()
loadSelectedCardPatternGroup()
saveLoadedPatternGroupToCard()
handleGrooveboxTransportButton()
executeMenuAction()
uiManagerInit()
uiManagerUpdate()
uiManagerHandleEncoderEvent()
uiManagerHandleAuxButtonEvent()
```

Developer notes:

- `uiManager.cpp` remains the UI coordinator.
- Avoid adding new rendering code here if it belongs in a focused UI module.
- Avoid expanding `UiState` unless required.
- Prefer small context structs for future render-module interfaces.

---

## 21.11 `src/uiPatternGroupInput.cpp`

Purpose:

Owns pattern group name input UI for Copy/Rename workflows.

Responsibilities:

```text
initialize name input state
open input in copy or rename mode
handle encoder rotation for character selection
handle encoder short press for character accept
handle KEY0 backspace/cancel behavior
build visible input string
draw popup
partially update input rows
return trimmed accepted name
```

Important functions:

```text
uiPatternGroupInputInit()
uiPatternGroupInputOpen()
uiPatternGroupInputClose()
uiPatternGroupInputIsOpen()
uiPatternGroupInputIsCopyMode()
uiPatternGroupInputRotate()
uiPatternGroupInputAcceptCharacter()
uiPatternGroupInputBackspaceOrCancel()
uiPatternGroupInputGetTrimmedName()
uiPatternGroupInputDraw()
```

Developer notes:

- This module should remain independent from Card Storage actions.
- It only edits text and reports accepted input.
- The caller decides whether the input means Copy or Rename.

---

## 21.12 `src/uiCardStorageActions.cpp`

Purpose:

Contains Card Storage action helpers that operate on pattern groups.

Responsibilities:

```text
load selected Card pattern group
save RAM pattern group to Card
copy Card pattern group
rename Card pattern group
delete Card pattern group
coordinate active group name updates
coordinate NVS changes
coordinate reload after copy/rename when needed
```

Important functions:

```text
uiCardStorageLoadPatternGroup()
uiCardStorageSavePatternGroup()
uiCardStorageCopyPatternGroup()
uiCardStorageRenamePatternGroup()
uiCardStorageDeletePatternGroup()
```

Developer notes:

- Keep blocking SD actions out of audio task.
- Actions may show busy/status messages through callbacks or UI manager coordination.
- Do not make this module responsible for rendering menus.

---

## 21.13 `src/uiCardStorageMenu.cpp`

Purpose:

Renders the Card Storage menu and pattern group list screens.

Responsibilities:

```text
draw Card Storage menu
draw Card pattern group selection
format menu rows
support menu first-visible index
support highlighted selection
```

Important functions:

```text
uiCardStorageMenuDraw()
uiCardStorageMenuBuildRows()
uiCardStorageMenuDrawPatternGroupList()
```

Developer notes:

- Rendering only.
- Do not put SD file operations here.
- Do not put encoder routing here.

---

## 21.14 `src/uiGrooveboxScreen.cpp`

Purpose:

Renders the main Groovebox screen and Groovebox edit popup.

Responsibilities:

```text
draw main sequencer screen
draw track rows
format step pattern rows
draw footer
show active/viewed/playing/next pattern info
draw edit popup overlay
draw tempo/status overlays where delegated
```

Important functions:

```text
uiGrooveboxScreenDraw()
uiGrooveboxScreenDrawFooterUpdate()
uiGrooveboxScreenDrawEditPopupOverlay()
buildTrackRowText()
buildParameterOverlayLine()
formatGrooveboxFooter()
buildEditPopupRows()
```

Developer notes:

- Keep Groovebox rendering here, not in `uiManager.cpp`.
- Rendering functions should not mutate sequencer state.
- Future improvement: replace long function argument lists with render context structs.

---

## 21.15 `src/uiSystemSettingsMenu.cpp`

Purpose:

Renders System Settings menu screens.

Responsibilities:

```text
draw System Settings menu
show settings entries
show selected row
show active values for theme/rotation/encoder order/sample set
```

Important functions:

```text
uiSystemSettingsMenuDraw()
uiSystemSettingsMenuBuildRows()
```

Developer notes:

- Rendering only.
- Keep System Settings actions in UI manager or a separate future action module.
- WiFi and reset confirmations should stay explicit and safe.

---

## 21.16 `src/uiSequencerInput.cpp`

Purpose:

Contains small generic UI input helpers.

Responsibilities:

```text
normalize menu selection
keep list selection visible
wrap selection indexes
```

Important functions:

```text
uiNormalizeMenuSelection()
uiEnsureSelectionVisible()
```

Developer notes:

- Keep this module generic.
- Do not add feature-specific UI logic here.

---

## 21.17 Other Headers and Support Modules

Important headers:

```text
appConfig.h             build flags and pin definitions
progVersion.h          version declaration
audioEngine.h          audio public API
sampleManager.h        sample API and SampleId definitions
sequencer.h            sequencer types and API
settingsStore.h        settings and pattern storage API
systemManager.h        system command API
uiManager.h            UI manager public API
uiGrooveboxScreen.h    Groovebox rendering API
uiCardStorageMenu.h    Card Storage rendering API
uiCardStorageActions.h Card Storage action API
uiPatternGroupInput.h  pattern group input API
uiSystemSettingsMenu.h System Settings rendering API
uiSequencerInput.h     generic UI input helper API
```

---

# 22. Rebuild Procedure From Zero

1. Clone or copy the repository.
2. Open the repository folder in VS Code.
3. Confirm `platformio.ini` exists at repository root.
4. Confirm environment `ESP32GrooveboxR3`.
5. Prepare SD Card with:
   - `/patterns/<GROUPNAME>/pNN.json`
   - `/samples/S1..S9/*.wav`
   - `/samples/Sn/setGain.json`
6. Insert SD Card.
7. Build:
   ```bash
   pio run -e ESP32GrooveboxR3
   ```
8. Upload to hardware manually.
9. Open serial monitor at 115200.
10. Confirm:
    ```text
    Booting ESP32 MicroCycles Groovebox (v0.8.5)
    SD card ready
    Active sample set: Sn
    Loaded sample gains from /samples/Sn/setGain.json
    Loaded SD sample kick/snare/ch/oh/tone/metal
    Audio engine initialized
    System manager initialized
    Groovebox UI opens
    ```

---

# 23. Validation Checklist

## 23.1 Boot

Confirm:

```text
SD initializes before display
no SD mount errors
samples load from active sample set
display shows Groovebox UI
```

## 23.2 Pattern Group

Confirm:

```text
Card Storage -> Load Pattern lists groups
selected group loads into RAM
active group stored in NVS
reboot restores active group
```

## 23.3 Pattern Editing

Confirm:

```text
edit p01
navigate to p01-pNN
edit steps
save group
reboot
load group
steps remain
```

## 23.4 Chain

Confirm:

```text
p01 -> p02 -> p03 -> p02
p04 unused
STOP during p02 finishes p02, plays p04, then stops
```

Confirm also:

```text
p01 -> p02 -> p03 -> p04 -> p01
STOP stops immediately
```

## 23.5 Sample Set

Confirm:

```text
select active sample set
system loads /samples/Sn/*.wav
system loads /samples/Sn/setGain.json
sample gains affect playback
```

## 23.6 Audio

Confirm:

```text
no clicks at sample end
hats choke naturally
dense patterns do not hard-cut voice 0 repeatedly
velocity differences are audible
limiter avoids harsh clipping
```

---

# 24. Common Failure Modes

## 24.1 SD Mount Fails

Check:

```text
SD CS/SCK/MISO/MOSI wiring
shared SPI wiring with TFT
card format FAT16/FAT32
SD inserted before boot
display was not initialized before SD
```

Use `SD_SMOKE_TEST` for diagnostics.

## 24.2 Samples Do Not Load

Check:

```text
active sample set in NVS
/samples/Sn/ directory exists
all required WAV files exist
WAV format is PCM 44.1 kHz
```

## 24.3 setGain.json Does Not Load

Check:

```text
file name is setGain.json
file is inside active sample set directory
JSON contains kick/snare/ch/oh/tone/metal keys
boot log shows active sample set before gain loading
```

## 24.4 Patterns Do Not Persist

Check:

```text
Card Storage -> Save Pattern was used
files are pNN.json
active group is correct
SD Card is writable
```

## 24.5 Chain Does Not Persist

Check:

```text
chain target visible before save
save operation completes
JSON contains chainTarget
load operation restores target
syncSequencerChainTargetsFromUi() is called after load/edit
```

---

# 25. Developer Rules

The developer must follow these project rules:

```text
Use PlatformIO
Use Arduino framework style
Use Allman braces
Use lowerCamelCase
Use 2-space indentation
Keep loop() minimal
Keep setup() and loop() at bottom of main.cpp
Show complete functions when modifying code
Do not add logic directly to loop()
Do not access filesystem from audio task
Do not redraw display from audio task
Do not block audio timing
Do not reintroduce LittleFS pattern storage
Do not move display init before sampleManagerInit()
Use "Error: ", "Warning: ", "Info: ", "Debug: " prefixes in program messages where applicable
Use Espressif logging where appropriate
```

---

# 26. Current Technical Debt

The current codebase is usable, but future cleanup should focus on:

```text
context structs for UI rendering APIs
shared patternNameUtils module
debug validation for UI/sequencer chain sync
further audio gain staging
limiter refinement
sample start fade-in
removal of unused A01/Z99 helper code if grep proves unused
```

Do not do large refactors immediately after major structural changes. First validate hardware behavior.

---

# 27. Suggested Next Development Order

Recommended order:

```text
1. Hardware validation of boot, SD, samples, patterns, chain, save/load
2. Remove remaining proven-unused old naming helpers
3. Add chain sync debug validation
4. Improve audio gain staging
5. Refine limiter
6. Add optional sample start fade-in
7. Introduce UI render context structs
8. Add patternNameUtils module
```

---

# 28. Related Documentation

```text
docs/README.md
docs/index.html
codingRules.md
platformio.ini
```
