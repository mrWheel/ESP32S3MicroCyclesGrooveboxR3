# ESP32S3MicroCyclesGrooveboxR3 SPA GUI

## Goal

Build a complete working SPA web GUI for the ESP32-S3 MicroCycles Groovebox R3.

This document describes how to add the current Web GUI implementation to a Groovebox firmware that already has the normal hardware UI and sequencer, but does not yet have the web server, REST API, or SPA frontend.

The Groovebox already runs on an ESP32-S3 with:

* TFT_LCD_Display_EC11 hardware
* ESP32-S3 piggyback board
* ST7789 TFT
* EC11 encoder
* KEY0 button
* SD card storage
* I2S audio output
* pattern groups stored on SD card
* sample sets stored on SD card
* existing physical UI
* existing sequencer and audio engine

The Web GUI must expose the Groovebox hardware functionality through a browser GUI.

Important: do not break the existing hardware UI. The Web GUI operates alongside the physical display, encoder and buttons. The firmware remains the source of truth.

# CRITICAL NON-NEGOTIABLE REQUIREMENTS

[⬆ UP](developerBuildGuide.md#20-source-file-reference) | [📖 README](../README.md#)

The Groovebox is an audio instrument first. Audio playback timing always has the highest priority.

## 0. Playback priority is absolute

Pattern playback must always remain reliable.

The SPA is mainly intended for comfortable pattern setup, editing and management. It is not primarily intended for timing-critical live performance.

Therefore:

* The audio task / sequencer playback must never be blocked by webserver work.
* HTTP request handling must never delay audio rendering.
* SD-card operations triggered by the GUI may cause a short GUI delay, but must not destabilize playback timing.
* Pattern mutation from the GUI may briefly pause or defer UI updates if needed.
* If a web operation is too heavy to run safely while playing, the firmware must either:
  * reject it with a clear JSON error,
  * defer it until a safe moment,
  * or require STOP first.
* Never prioritize SPA responsiveness over audio playback correctness.
* The firmware remains the source of truth for actual playback state.
* The browser GUI is for editing and management convenience, not for timing-critical live sequencing.
* The GUI must not reload all pattern JSON while playback is running.

## 1. Main Pattern Grid is mandatory

[⬆ UP](developerBuildGuide.md#20-source-file-reference) | [📖 README](../README.md#)

The SPA is not acceptable unless the main screen shows the real Groovebox sequencer grid:

* 6 tracks vertically:
  * KICK
  * SNARE
  * CH
  * OH
  * TONE
  * METAL

* 48 visible steps horizontally:
  * exactly 3 visible patterns
  * each pattern is 16 steps
  * total = 3 x 16 = 48 cells per track

This grid must be visible on the main screen immediately after loading the active group.

Do not replace this with cards, placeholder panels, a status dashboard, or a simplified pattern list.

The main grid must render real pattern data from the firmware.

Each cell must display state:

* empty step: `-`
* active triggered step: `x`
* muted triggered step: `m`

Track mute must be shown separately from step mute.

The playhead cursor must be shown while playing by highlighting the currently playing step cell. The current implementation uses the CSS class `step-playhead`.

## 2. Load Group must list SD card pattern groups

[⬆ UP](developerBuildGuide.md#20-source-file-reference) | [📖 README](../README.md#)

The button must be named:

`LOAD GROUP`

not:

`LOAD PATTERN`

When the user presses `LOAD GROUP`, the SPA must call:

`GET /api/groups`

The firmware must return all pattern groups found on the SD card.

The SPA must show these group names in a themed popup window.

The currently active group must be marked with:

`*`

The popup must have:

* `[CANCEL]`
* `[ACCEPT]`

When a group is selected and `[ACCEPT]` is pressed, the SPA must call:

`POST /api/groups/load`

with:

```json
{
  "groupName": "GROUPNAME"
}
```

The SPA must show a busy / tumbling-wheel overlay while loading.

After loading, the SPA must reload the pattern list and all pattern JSON for that group into browser memory.

## General Requirements

[⬆ UP](developerBuildGuide.md#20-source-file-reference) | [📖 README](../README.md#)

Implement both:

1. ESP32 firmware API endpoints.
2. Browser SPA frontend.

Use the Arduino `WebServer` infrastructure.

The SPA is served from LittleFS and must be usable from:

```text
http://<groovebox-ip>/
```

Add JSON API endpoints under:

```text
/api/...
```

All API responses must be JSON unless serving static SPA assets.

The current implementation uses polling, not WebSockets.

Use polling as follows:

* `/api/status` every 1000 ms
* `/api/transport` every 1000 ms
* `/api/sequencer/playhead` every 100 ms
* pattern/group data only on explicit load/save/group switch or when safely idle

WebSockets may be added later, but they are not part of the current implementation.

The code style must match this repository:

* Allman braces
* 2-space indentation
* lowerCamelCase
* comments above code
* comments in English
* no emoji/icons in program code
* use fixed log prefixes such as Info:, Warning:, Error:, Debug: where user-visible text needs prefixes
* keep loop() minimal
* setup() and loop() remain at bottom of main.cpp
* when adding a new function to an existing file, place it explicitly between existing functions and document the placement

## Architecture

[⬆ UP](developerBuildGuide.md#20-source-file-reference) | [📖 README](../README.md#)

Add a web control layer that maps HTTP requests to existing Groovebox functions.

Create or extend these modules:

```text
include/webServerManager.h
src/webServerManager.cpp
include/webApi.h
src/webApi.cpp
data/index.html
data/app.js
data/style.css
```

### `src/webServerManager.cpp`

Responsibilities:

* own the Arduino `WebServer` instance
* mount LittleFS for SPA assets
* serve `/` from `/index.html`
* serve `/app.js`
* serve `/style.css`
* start the webserver when WiFi is connected
* stop or avoid normal server use while WiFiManager portal owns port 80 if needed
* call `webApiRegisterRoutes(webServer)`
* log the webserver URL to serial and, during boot only, optionally to the display

### `src/webApi.cpp`

Responsibilities:

* implement all `/api/...` handlers
* register all REST API routes
* parse JSON request bodies
* generate JSON responses
* use `UriRegex` for dynamic pattern routes
* call into the existing sequencer, settings store, sample manager, audio engine, system manager and UI manager wrappers
* never duplicate timing-critical sequencer logic in the web layer

### SPA frontend

The frontend consists of:

```text
data/index.html
data/app.js
data/style.css
```

`index.html` defines:

* header
* transport controls
* group controls
* sample controls
* pattern grid
* popups
* busy overlay

`app.js` owns:

* SPA state model
* polling
* REST API calls
* pattern grid rendering
* chain-aware window scrolling
* step editor popup behavior
* group and sample operations

`style.css` defines:

* light theme
* grid appearance
* playhead and cursor styling
* popup styling
* busy overlay
* hover states

## Firmware/API Requirements

[⬆ UP](developerBuildGuide.md#20-source-file-reference) | [📖 README](../README.md#)

The API must expose the Groovebox functionality required by the current Web GUI.

### Common JSON conventions

Successful responses include:

```json
{
  "ok": true
}
```

Error responses include:

```json
{
  "ok": false,
  "error": "No SD card"
}
```

All handlers must return `application/json`.

### Status

Endpoint:

```text
GET /api/status
```

Handled by:

```text
src/webApi.cpp -> handleStatusRequest()
```

Example response:

```json
{
  "ok": true,
  "version": "v1.4.5",
  "ip": "192.168.1.83",
  "ssid": "yourApName",
  "wifiConnected": true,
  "webServerRunning": true,
  "activeGroup": "TEST1",
  "activePattern": "p01",
  "activeSampleSet": "S3",
  "playing": false,
  "editMode": false,
  "bpm": 120,
  "swing": 8,
  "currentStep": 0,
  "selectedTrack": 0,
  "selectedStep": 0,
  "patternGroupDirty": false,
  "sdCardInserted": true,
  "sdCardReady": true,
  "psramAvailable": true,
  "freePsram": 8388608,
  "freeHeap": 180000
}
```

Status must include enough information to update the header and transport UI.

### Transport

Endpoints:

```text
GET  /api/transport
POST /api/transport/play
POST /api/transport/stop
POST /api/transport/toggle
POST /api/transport/bpm
POST /api/transport/swing
```

Handlers:

```text
GET  /api/transport        -> handleTransportRequest()
POST /api/transport/play   -> handleTransportPlayRequest()
POST /api/transport/stop   -> handleTransportStopRequest()
POST /api/transport/toggle -> handleTransportToggleRequest()
POST /api/transport/bpm    -> handleTransportBpmRequest()
POST /api/transport/swing  -> handleTransportSwingRequest()
```

`GET /api/transport` response:

```json
{
  "ok": true,
  "bpm": 120,
  "swing": 8,
  "playing": false,
  "editMode": false,
  "chainEnabled": true,
  "chainLength": 6,
  "currentStep": 0,
  "activePatternIndex": 0,
  "playingPatternIndex": 0
}
```

BPM request:

```json
{
  "bpm": 120
}
```

Swing request:

```json
{
  "swing": 8
}
```

STOP must mean immediate stop. It must not play an extra final pattern.

### Pattern groups

Endpoints:

```text
GET  /api/groups
GET  /api/groups/active
POST /api/groups/load
POST /api/groups/save
POST /api/groups/new
POST /api/groups/rename
POST /api/groups/copy
POST /api/groups/delete
```

Handlers:

```text
GET  /api/groups        -> handleGroupsListRequest()
GET  /api/groups/active -> handleGroupsActiveRequest()
POST /api/groups/load   -> handleGroupsLoadRequest()
POST /api/groups/save   -> handleGroupsSaveRequest()
POST /api/groups/new    -> handleGroupsNewRequest()
POST /api/groups/rename -> handleGroupsRenameRequest()
POST /api/groups/copy   -> handleGroupsCopyRequest()
POST /api/groups/delete -> handleGroupsDeleteRequest()
```

`GET /api/groups` response:

```json
{
  "ok": true,
  "activeGroup": "TEST1",
  "groups": [
    "DEMO",
    "JAZZ01",
    "TEST1"
  ]
}
```

`GET /api/groups/active` response:

```json
{
  "ok": true,
  "name": "TEST1",
  "patternCount": 6
}
```

Load group request:

```json
{
  "groupName": "TEST1"
}
```

Load group success response:

```json
{
  "ok": true,
  "groupName": "TEST1",
  "patternCount": 6
}
```

New group request:

```json
{
  "name": "NEWGROUP"
}
```

Current New Group behavior:

* group names are normalized to uppercase in the SPA
* the active group is copied to the new group
* the new group is then loaded and made active
* the SPA shows a busy overlay while creating/loading
* the header active group is updated after success

Rename group request:

```json
{
  "from": "OLDNAME",
  "to": "NEWNAME"
}
```

Copy group request:

```json
{
  "from": "SOURCE",
  "to": "COPYNAME"
}
```

Delete group request:

```json
{
  "name": "GROUPNAME"
}
```

The GUI must not allow deleting the currently active group.

Group actions should call `uiManagerReturnToGrooveboxScreen()` after successful firmware-side actions so the physical display returns to the Groovebox screen.

### Pattern data

The GUI must load all patterns belonging to the active group into browser memory because switching patterns from SD while playing is too slow.

Endpoints:

```text
GET  /api/patterns
GET  /api/patterns/active
POST /api/patterns/active
GET  /api/patterns/{patternName}
PUT  /api/patterns/{patternName}
POST /api/patterns/{patternName}/clear
POST /api/patterns/{patternName}/copy
```

Handlers:

```text
GET  /api/patterns              -> handlePatternsListRequest()
GET  /api/patterns/active       -> handlePatternsActiveGetRequest()
POST /api/patterns/active       -> handlePatternsActiveSetRequest()
GET  /api/patterns/pXX          -> handlePatternsGetRequest()
PUT  /api/patterns/pXX          -> handlePatternsPutRequest()
POST /api/patterns/pXX/clear    -> handlePatternsClearRequest()
POST /api/patterns/pXX/copy     -> handlePatternsCopyRequest()
```

Dynamic pattern routes must use `UriRegex`.

`GET /api/patterns` response:

```json
{
  "ok": true,
  "activePatternIndex": 0,
  "patterns": [
    {
      "name": "p01",
      "slotIndex": 0
    },
    {
      "name": "p02",
      "slotIndex": 1
    }
  ]
}
```

`GET /api/patterns/p01` response:

```json
{
  "ok": true,
  "name": "p01",
  "bpm": 120,
  "swing": 8,
  "chainEnabled": true,
  "chainLength": 6,
  "chainTarget": "p02",
  "tracks": [
    {
      "name": "KICK",
      "mute": false,
      "steps": [
        {
          "trigger": false,
          "mute": false,
          "velocity": 100,
          "probability": 100,
          "lockEnabled": false,
          "lockPitch": 0,
          "lockDecay": 100
        }
      ]
    }
  ]
}
```

The pattern JSON returned by the API must include everything required to edit and redraw the GUI without extra API calls:

* pattern name
* BPM
* swing
* chain enabled
* chain target
* chain length
* tracks
* track mute
* 16 steps per track
* trigger
* step mute
* velocity
* probability
* lock enabled
* lock pitch
* lock decay

Chain information must match the hardware display and sequencer behavior. The browser must not invent or guess the chain.

### Step editing

Endpoint:

```text
PUT /api/patterns/{patternName}/tracks/{trackIndex}/steps/{stepIndex}
```

Handler:

```text
handleStepEditRequest()
```

Payload fields:

```json
{
  "trigger": true,
  "mute": false,
  "velocity": 128,
  "probability": 100,
  "lockEnabled": false,
  "lockPitch": 0,
  "lockDecay": 100
}
```

The field `mute` means step mute, not track mute.

The SPA keeps changes in `state.stepEditorDraft` while the step popup is open.

When the popup closes, the SPA compares the draft with the original step data. If anything changed, the SPA sends the `PUT` request and updates the local `state.patterns` copy on success.

### Track editing

Endpoints:

```text
PUT  /api/patterns/{patternName}/tracks/{trackIndex}
POST /api/tracks/{trackIndex}/mute-toggle
```

Handlers:

```text
PUT  /api/patterns/pXX/tracks/N -> handleTrackEditRequest()
POST /api/tracks/N/mute-toggle  -> handleTrackMuteToggleRequest()
```

Track mute is independent from step mute.

Step mute is shown as a lower-case `m` in the step grid.

A normal triggered step is shown as `x`.

An empty step is shown as `-`.

### Sample sets

Endpoints:

```text
GET  /api/sample-sets
GET  /api/sample-sets/active
POST /api/sample-sets/load
GET  /api/samples
PUT  /api/samples/gain
```

Handlers:

```text
GET  /api/sample-sets        -> handleSampleSetsListRequest()
GET  /api/sample-sets/active -> handleSampleSetsActiveRequest()
POST /api/sample-sets/load   -> handleSampleSetsLoadRequest()
GET  /api/samples            -> handleSamplesListRequest()
PUT  /api/samples/gain       -> handleSamplesGainRequest()
```

`GET /api/sample-sets` response:

```json
{
  "ok": true,
  "activeSampleSet": "S3",
  "sampleSets": [
    "S1",
    "S2",
    "S3"
  ]
}
```

Load sample request:

```json
{
  "name": "S3"
}
```

`GET /api/samples` response:

```json
{
  "ok": true,
  "activeSampleSet": "S3",
  "samples": [
    {
      "name": "KICK",
      "index": 0,
      "gainPercent": 100,
      "frameCount": 14974,
      "valid": true
    }
  ]
}
```

Sample gain request:

```json
{
  "sampleIndex": 0,
  "gainPercent": 120
}
```

The sample button in the SPA must be labelled:

`LOAD SAMPLE`

### Sequencer live state

Endpoints:

```text
GET  /api/sequencer/view
GET  /api/sequencer/playhead
POST /api/sequencer/cursor
POST /api/sequencer/edit-mode
```

Handlers:

```text
GET  /api/sequencer/view      -> handleSequencerViewRequest()
GET  /api/sequencer/playhead  -> handleSequencerPlayheadRequest()
POST /api/sequencer/cursor    -> handleSequencerCursorRequest()
POST /api/sequencer/edit-mode -> handleSequencerEditModeRequest()
```

`GET /api/sequencer/view` response:

```json
{
  "ok": true,
  "bpm": 120,
  "swing": 8,
  "currentStep": 0,
  "selectedTrack": 0,
  "cursorStep": 0,
  "activePatternIndex": 0,
  "playingPatternIndex": 0,
  "chainLength": 6,
  "playing": false,
  "editMode": false,
  "chainEnabled": true
}
```

`GET /api/sequencer/playhead` response:

```json
{
  "ok": true,
  "bpm": 120,
  "currentStep": 4,
  "playing": true,
  "activePatternIndex": 0,
  "playingPatternIndex": 2,
  "chainEnabled": true,
  "chainLength": 6
}
```

Cursor request:

```json
{
  "stepIndex": 0,
  "trackIndex": 0
}
```

Edit mode request:

```json
{
  "enabled": true
}
```

Pattern indexes used by the SPA are zero-based.

### Pattern change behavior

* If chain is OFF and the user changes the active visible pattern while playing, the currently playing pattern must complete first.
* Playback then switches to the selected pattern at the next pattern boundary.
* STOP always stops immediately.
* Chain-aware pattern display must use `chainEnabled` and `chainTarget` from pattern JSON.
* The GUI must not assume sequential numeric pattern order.

Example chain:

```text
p01 -> p02 -> p03 -> p05 -> p04 -> p06 -> p02
```

Expected GUI windows:

```text
p01 | p02 | p03
p02 | p03 | p05
p03 | p05 | p04
p05 | p04 | p06
p04 | p06 | p02
```

## UI/SPA Requirements

[⬆ UP](developerBuildGuide.md#20-source-file-reference) | [📖 README](../README.md#)

### Main Layout

The main SPA screen must show:

1. Header:
   * Groovebox name
   * firmware version
   * WiFi/IP
   * active group
   * active sample set
   * dirty indicator

2. Transport controls:
   * PLAY
   * STOP
   * TOGGLE
   * EDIT STEPS toggle
   * BPM slider/input
   * swing slider/input

3. Pattern group controls:
   * SAVE GROUP
   * LOAD GROUP
   * NEW GROUP
   * RENAME GROUP
   * COPY GROUP
   * DELETE GROUP

4. Sample controls:
   * sample set selector
   * LOAD SAMPLE

5. Main pattern grid:
   * show 3 chain-aware visible patterns at once
   * each pattern has 16 steps
   * total visible steps = 48
   * all 6 tracks visible vertically
   * show track names
   * show step states:
     * `-` empty
     * `x` triggered
     * `m` muted triggered step
   * show selected step cursor
   * show playhead cursor

6. Popups:
   * Load Group popup
   * Action popup for Save/New/Rename/Copy/Delete messages
   * Step popup
   * Busy overlay

### Pattern Grid Scrolling

All patterns in the active group must be loaded into GUI memory.

The GUI displays 3 visible patterns at the same time:

```text
3 patterns x 16 steps = 48 visible steps
```

The current implementation keeps these indices:

* `visiblePatternStartIndex`
* `visiblePatternMiddleIndex`
* `visiblePatternRightIndex`

The next visible pattern is determined by:

1. current pattern `chainEnabled`
2. current pattern `chainTarget`
3. fallback to next numeric pattern only when chain info is disabled or missing

This logic belongs in `data/app.js` in `getNextPatternIndex()`.

The visible window must follow the actual playback chain, not the numeric order of files.

The scrolling is visual only. It must not cause SD reads during playback.

### Step Popup

The SPA has an `EDIT STEPS` toggle.

When `EDIT STEPS` is ON:

* hovering over a step opens the step editor popup
* the popup is positioned over the pattern grid
* the popup top-left corner is placed on the hovered step
* leaving the popup commits changed values and closes it
* pressing Close also commits changed values and closes it

When `EDIT STEPS` is OFF:

* hovering over a step must not open the step editor popup

The panel must be titled:

```text
Step
```

It must contain:

* Trigger
* Step OFF
* velocity slider + numeric input
* probability slider + numeric input
* lock enabled toggle
* pitch slider + numeric input
* decay slider + numeric input
* pattern name
* track name
* step number
* Close button

`Step OFF` controls step mute only.

It must not change whole-track mute.

### Popup behavior

Browser-native dialogs must not be used.

Do not use:

* `alert()`
* `prompt()`
* `confirm()`

Use themed SPA popup windows instead.

Only one popup should be open at a time.

Opening a new main popup must first close the existing popup as if Cancel or Close was selected.

Long-running operations must show the busy overlay / tumbling wheel.

Long-running operations include:

* Save Group
* Load Group
* New Group
* Rename Group
* Copy Group
* Delete Group
* Load Sample

### Dirty State

Whenever a pattern or group setting changes in the SPA:

* update local GUI state
* send the change to the hardware when appropriate
* mark the hardware group dirty
* make the SPA show the unsaved indicator

Saving the group clears the dirty state.

The SPA reads dirty state from:

```text
GET /api/status -> patternGroupDirty
```

### Polling

Use polling:

* `updateGuiSync()` every 1000 ms
* `/api/status` every 1000 ms
* `/api/transport` every 1000 ms
* `/api/sequencer/playhead` every 100 ms

Do not reload all pattern JSON while playing.

Pattern data should be refreshed:

* after group load
* after new group
* after rename if the active group changed
* after explicit pattern save/load operations
* when safely idle if needed

Avoid pattern refresh while:

* playing
* busy
* step editor is open

## SPA State Model

[⬆ UP](developerBuildGuide.md#20-source-file-reference) | [📖 README](../README.md#)

The frontend must maintain:

```js
const state = {
  status : {},
  groups : [],
  activeGroup : "",
  patterns : [],
  activePatternIndex : 0,
  playingPatternIndex : 0,
  visiblePatternStartIndex : 0,
  selectedTrackIndex : 0,
  selectedStepGlobalIndex : 0,
  selectedStepLocalIndex : 0,
  selectedPatternIndex : 0,
  stepEditorOpen : false,
  stepEditorDraft : {},
  dirty : false,
  actionPopupMode : "",
  actionPopupValue : "",
  selectedGroupForLoad : "",
  editStepsEnabled : true,
  busy : false,
  lastScrollPatternIndex : -1,
  lastScrollStepIndex : -1,
  lastPatternSyncMs : 0,
  visiblePatternMiddleIndex : 1,
  visiblePatternRightIndex : 2,
  visiblePatternWindowReady : false,
};
```

Pattern indexing:

* local pattern step index: 0..15
* visible global step index: 0..47
* pattern window: 3 visible patterns
* pattern indexes are zero-based in JavaScript
* pattern names are `p01`, `p02`, etc.
* chain targets use pattern names such as `p02`

## Acceptance Criteria

[⬆ UP](developerBuildGuide.md#20-source-file-reference) | [📖 README](../README.md#)

The implementation is complete when:

1. The Groovebox can be opened in a browser at `http://<ip>/`.
2. `/api/status` returns valid JSON.
3. The SPA loads the active group and all patterns into GUI memory.
4. The SPA shows 48 steps as 3 x 16-step patterns.
5. The visual window follows the playback chain and appends the chain target at the right side.
6. The playhead cursor is visible during playback.
7. The Step editor appears immediately on hover when EDIT STEPS is ON.
8. The Step editor does not appear when EDIT STEPS is OFF.
9. Step editor changes are sent back to the ESP32 when closing/leaving the panel.
10. Track mute and step mute are independent.
11. STOP stops immediately.
12. Pattern changes while playing are deferred until pattern boundary.
13. Save Group writes the current in-memory group to SD.
14. New Group copies the active group, creates the new group, then loads it.
15. Rename/Copy/Delete group actions use themed popups.
16. Active group cannot be deleted from the GUI.
17. Sample set loading works from the Web GUI.
18. No native browser dialogs are used.
19. Busy overlay appears during long operations.
20. Web GUI does not break the hardware UI.
21. The webserver does not redraw the boot log after the Groovebox UI is open.
22. All new code follows the repository coding style.

## Implementation Notes

[⬆ UP](developerBuildGuide.md#20-source-file-reference) | [📖 README](../README.md#)

Before coding, inspect:

* `src/webServerManager.cpp`
* `include/webServerManager.h`
* `src/webApi.cpp`
* `include/webApi.h`
* `src/systemManager.cpp`
* `src/uiManager.cpp`
* `include/uiManager.h`
* `src/sequencer.cpp`
* `include/sequencer.h`
* `src/settingsStore.cpp`
* `include/settingsStore.h`
* `src/sampleManager.cpp`
* `include/sampleManager.h`
* `src/uiGrooveboxScreen.cpp`
* `data/index.html`
* `data/app.js`
* `data/style.css`

Rules:

1. Do not invent API logic that duplicates sequencer state incorrectly.
2. Reuse existing public functions where possible.
3. Add small wrapper functions where existing functions are static/internal.
4. Avoid large blocking SD operations during playback.
5. Keep all group patterns loaded in RAM once a group is loaded.
6. Register API routes only in `webApiRegisterRoutes()`.
7. Use `UriRegex` for dynamic pattern and track routes.
8. Browser chain display must use pattern API `chainEnabled` and `chainTarget`.
9. Browser native popups must not be used.
10. Long API operations must use the busy overlay.
11. Return meaningful errors.
12. Return success responses consistently.

## Final Deliverables

[⬆ UP](developerBuildGuide.md#20-source-file-reference) | [📖 README](../README.md#)

Produce:

1. Firmware webserver manager implementation.
2. Firmware REST API implementation.
3. SPA files:
   * `data/index.html`
   * `data/app.js`
   * `data/style.css`
4. Any new headers:
   * `include/webServerManager.h`
   * `include/webApi.h`
5. Updates to existing modules needed for public wrapper functions.
6. Documentation updates.
7. A short test checklist.
8. A commit message.

Do not remove existing hardware UI functionality.

Do not change existing coding style.

Do not replace the current sequencer model unless absolutely required.

---

[⬆ UP](developerBuildGuide.md#20-source-file-reference) | [📖 README](../README.md#)
