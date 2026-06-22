# You are working on the repository ESP32S3MicroCyclesGrooveboxR3.

## Goal

Build a complete working SPA web GUI for the ESP32-S3 MicroCycles Groovebox R3.

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
* existing webServerManager infrastructure

The SPA must expose all Groovebox hardware functionality through the browser GUI.

Important: do not break the existing hardware UI. The web GUI must operate alongside the physical display/encoder/buttons.

# CRITICAL NON-NEGOTIABLE REQUIREMENTS

The Groovebox is an audio instrument first. Audio playback timing always has the highest priority.

## 0. Playback priority is absolute

Pattern playback must always remain 100% reliable.

The SPA is mainly intended for comfortable pattern setup, editing and management. It is not primarily intended for live performance.

Therefore:

- The audio task / sequencer playback must never be blocked by webserver work.
- HTTP request handling must never delay audio rendering.
- SD-card operations triggered by the GUI may cause a short GUI delay, but must not destabilize playback timing.
- Pattern mutation from the GUI may briefly pause or defer UI updates if needed.
- If a web operation is too heavy to run safely while playing, the firmware must either:
  - reject it with a clear JSON error, or
  - defer it until a safe moment, or
  - require STOP first.
- Never prioritize SPA responsiveness over audio playback correctness.
- The firmware remains the source of truth for actual playback state.
- The browser GUI is for editing and management convenience, not for timing-critical live sequencing.

## 1. Main Pattern Grid is mandatory

The SPA is not acceptable unless the main screen shows the real Groovebox sequencer grid:

- 6 tracks vertically:
  - Kick
  - Snare
  - CH
  - OH
  - Tone
  - Metal

- 48 visible steps horizontally:
  - exactly 3 consecutive patterns
  - each pattern is 16 steps
  - total = 3 x 16 = 48 cells per track

This grid must be visible on the main screen immediately after loading the active group.

Do not replace this with cards, placeholder panels, a status dashboard, or a simplified pattern list.

The main grid must render real pattern data from the firmware.

Each cell must display state:

- empty step: `-` (or an empty square)
- active triggered step: `x` (or a tick)
- muted triggered step: `m` (or a striked throug tick or so)

Track mute must be shown separately as `*` after the track name.

## 2. Load Group must list SD card pattern groups

The button must be named:

`Load Group`

not:

`Load Pattern`

When the user presses `Load Group`, the SPA must call:

`GET /api/groups`

The firmware must return all pattern groups found on the SD card.

The SPA must show these group names in a selectable list/window.

When a group is selected, the SPA must call:

`POST /api/groups/load`

with:

```json
{
  "groupName": "GROUPNAME"
}
```
## General Requirements

Implement both:

1. ESP32 firmware API endpoints.
2. Browser SPA frontend.

Use the existing Arduino WebServer infrastructure in src/webServerManager.cpp.

The SPA may initially be served from LittleFS or embedded static strings, but the final implementation must be cleanly structured so the frontend can later be replaced or expanded.

The SPA must be usable from:
```
http://<groovebox-ip>/
```
Add JSON API endpoints under:
```
/api/...
```
All API responses must be JSON unless serving static SPA assets.

*** Use WebSockets! ***

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

Add a web control layer that maps HTTP requests to existing Groovebox functions.

Create or extend these modules as needed:

include/webServerManager.h
src/webServerManager.cpp
include/webApi.h
src/webApi.cpp
data/index.html
data/app.js
data/style.css

If the project does not currently use LittleFS SPA assets, add the needed PlatformIO/LittleFS support without breaking existing LittleFS usage.

## Firmware/API Requirements

The API must expose all Groovebox functionality that is available from the hardware UI.

At minimum provide endpoints for:

System
```
GET  /api/status
GET  /api/system
POST /api/system/restart
GET  /api/wifi/status
POST /api/wifi/start-manager
POST /api/wifi/erase-credentials
```
Status must include:

* firmware version
* current IP
* WiFi SSID
* WiFi connected yes/no
* webserver running yes/no
* active sample set
* active pattern group
* active pattern name
* transport state
* playing yes/no
* edit mode yes/no
* BPM
* swing
* selected track
* selected step
* selected parameter page
* pattern group dirty yes/no
* SD card inserted yes/no
* SD card ready yes/no
* PSRAM available yes/no
* free heap
* free PSRAM

### Transport
```
GET  /api/transport
POST /api/transport/play
POST /api/transport/stop
POST /api/transport/toggle
POST /api/transport/bpm
POST /api/transport/swing

STOP must mean immediate stop. It must not play an extra final pattern.
```

### Pattern groups
```
GET  /api/groups
GET  /api/groups/active
POST /api/groups/load
POST /api/groups/save
POST /api/groups/new
POST /api/groups/rename
POST /api/groups/copy
POST /api/groups/delete

POST /api/groups/new must create a group with one pattern p01:
```

* all triggers off
* all step mutes false
* velocity 128
* probability 100
* pitch lock off
* decay lock default
* chain off
* chain length 1

### Pattern data

The GUI must load all patterns belonging to the active group into browser memory because switching patterns from SD while playing is too slow.

Provide:
```
GET  /api/patterns
GET  /api/patterns/active
POST /api/patterns/active
GET  /api/patterns/{patternName}
PUT  /api/patterns/{patternName}
POST /api/patterns/{patternName}/clear
POST /api/patterns/{patternName}/copy
```
The pattern JSON returned by the API must include everything required to edit and redraw the GUI without extra API calls:

* pattern name
* BPM
* swing
* tracks
* 16 steps per track
* trigger
* step mute
* velocity
* probability
* lock enabled
* lock pitch
* lock decay
* track mute
* chain enabled
* chain target
* chain length

Step editing

Provide:
```
PUT /api/patterns/{patternName}/tracks/{trackIndex}/steps/{stepIndex}
```
Payload fields:
```
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
The field mute means step mute, not track mute.

### Track editing
```
PUT  /api/patterns/{patternName}/tracks/{trackIndex}
POST /api/tracks/{trackIndex}/mute-toggle
```
Track mute is independent from step mute.

Track mute is shown as * after the track name in the hardware UI.

Step mute is shown as a lower-case m (or strike throug tick) in the step grid.

A normal triggered step is shown as x (or as a tick symbol).

An empty step is shown as -.

### Sample sets
```
GET  /api/sample-sets
GET  /api/sample-sets/active
POST /api/sample-sets/load
GET  /api/samples
PUT  /api/samples/gain
```
Sample status must include:

* sample set name
* sample names
* loaded/fallback status
* frame count
* RAM/PSRAM storage
* gain percentage

### Sequencer live state
```
GET /api/sequencer/view
GET /api/sequencer/playhead
POST /api/sequencer/cursor
POST /api/sequencer/edit-mode
```
The playhead endpoint must return:

* currently playing pattern index/name
* active/visible pattern index/name
* current step
* global absolute step if available
* playing state
* pending pattern switch if any

### Pattern change behavior:

* If chain is OFF and the user changes the active visible pattern while playing, the currently playing pattern must complete first.
* Playback then switches to the selected pattern at the next pattern boundary.
* STOP always stops immediately.

## UI/SPA Requirements

Main Layout

The main SPA screen must show:

1. Header:
    * Groovebox name
    * firmware version
    * WiFi/IP
    * active group
    * active sample set
    * transport state
    * dirty indicator
2. Transport controls:
    * PLAY
    * STOP
    * BPM slider/input
    * swing slider/input
3. Pattern group controls:
    * Save Group
    * Load Group
    * New Group
    * Rename Group
    * Copy Group
    * Delete Group
4. Sample controls:
    * Load Sample Set
    * sample gain controls
5. Main pattern grid:
    * show 3 consecutive patterns at once
    * each pattern has 16 steps
    * total visible steps = 48
    * all tracks visible vertically
    * show track names
    * show track mute state
    * show step states:
        * - empty
        * x triggered
        * m muted triggered step
    * show current cursor
    * show playhead

### Pattern Grid Scrolling

All patterns in the active group must be loaded into GUI memory.

The GUI must display 3 consecutive patterns at the same time:

3 patterns x 16 steps = 48 visible steps

Example with patterns:
```
p01, p02, p03, p04
```
Initial visible window:
```
p01, p02, p03
```
When the playhead reaches the end of the middle pattern on screen, which is visible step 32, the GUI must scroll one pattern to the left and append the next pattern.

Example:

Visible:
```
p01, p02, p03
```
At the last step of p02, visible step 32, scroll left:
```
p02, p03, p04
```
At the last step of p03, scroll left:
```
p03, p04, p01
```
This must wrap around.

The scrolling is visual only. It must not cause SD reads during playback.

### Step Popup

When the cursor is on a step, a step editor panel must appear immediately.

This is a modal popup with left- or right-corner at the step origen.
It must be a separate window/panel over the main pattern screen.

The panel must be titled:

## Step

It must contain all editable settings for the selected step:

* Step ON/OFF
* velocity slider
* probability slider
* lock enabled toggle
* pitch slider
* decay slider
* optional trigger toggle
* track name
* pattern name
* step number
* Close button

Numeric values must use sliders and numeric input fields where useful.

If the cursor leaves the step popup, the SPA must send the changes to the hardware and close the popup.

If [Close] is pressed, the SPA must also send pending changes to the hardware and close the popup.

The popup must not change the whole track mute when editing STEP ON/OFF.

### STEP ON/OFF rules:

* STEP ON means the step can play if trigger is enabled.
* STEP OFF means this triggered step is muted.
* STEP OFF must display as m in the grid.
* Track mute remains separate and displays as *.

## Hardware popup menus

Hardware display popup menus must map to SPA panels/windows below the main pattern grid.

Do not replicate tiny embedded display popups. Instead, expose full browser controls with all settings visible.

Examples:

* Step editor panel
* Tempo panel
* Pattern group panel
* Sample set panel
* System settings panel

Each panel must include a clear [Close] button.

### Dirty State

Whenever a pattern or group setting changes in the SPA:

* mark the local GUI state dirty
* send the change to the hardware when appropriate
* make the hardware show unsaved group changes as Save Group *
* make the SPA show an unsaved indicator

Saving the group clears the dirty state.

### Polling

Use polling initially:

* /api/status every 1000 ms
* /api/sequencer/playhead every 100-250 ms while playing
* pattern/group data only on explicit load/save/group switch

Do not reload all pattern JSON while playing.

## SPA State Model

The frontend must maintain:
```
state = {
  status: {},
  groups: [],
  activeGroup: "",
  patterns: [],
  activePatternIndex: 0,
  playingPatternIndex: 0,
  visiblePatternStartIndex: 0,
  selectedTrackIndex: 0,
  selectedStepGlobalIndex: 0,
  selectedStepLocalIndex: 0,
  selectedPatternIndex: 0,
  stepEditorOpen: false,
  stepEditorDraft: {},
  dirty: false
}
```
Pattern indexing:

* local pattern step index: 0..15
* visible global step index: 0..47
* pattern window: 3 consecutive patterns
* wrap pattern indices at group length

Acceptance Criteria

The implementation is complete when:

1. The Groovebox can be opened in a browser at http://<ip>/.
2. /api/status returns valid JSON.
3. The SPA loads active group and all patterns into GUI memory.
4. The SPA shows 48 steps as 3 x 16-step patterns.
5. The visual window scrolls at visible step 32.
6. The Step editor appears immediately when a step is selected.
7. Step editor changes are sent back to the ESP32 when closing/leaving the panel.
8. Track mute and step mute are independent.
9. STOP stops immediately.
10. Pattern changes while playing are deferred until pattern boundary.
11. Save Group writes the current in-memory group to SD.
12. New Group creates one empty p01.
13. Sample set loading works from the web GUI.
14. Web GUI does not break the hardware UI.
15. The webserver does not redraw the boot log after the Groovebox UI is open.
16. All new code follows the repository coding style.

## Implementation Notes

Before coding:

1. Inspect:
    * src/webServerManager.cpp
    * include/webServerManager.h
    * src/systemManager.cpp
    * src/uiManager.cpp
    * src/sequencer.cpp
    * include/sequencer.h
    * src/settingsStore.cpp
    * include/settingsStore.h
    * src/sampleManager.cpp
    * include/sampleManager.h
    * src/uiGrooveboxScreen.cpp
2. Do not invent API logic that duplicates sequencer state incorrectly.
3. Reuse existing public functions where possible.
4. Add small wrapper functions where existing functions are static/internal.
5. Avoid large blocking SD operations during playback.
6. Keep all group patterns loaded in RAM once a group is loaded.
7. Make API updates deterministic and explicit.
8. Return meaningful errors:
```
{
  "ok": false,
  "error": "No SD card"
}
```
9. Return success responses consistently:
```
{
  "ok": true
}
```
Suggested Endpoint Response Shapes

Status:
```
{
  "ok": true,
  "version": "v1.3.9",
  "wifiConnected": true,
  "ssid": "AandeWiFi",
  "ip": "192.168.12.83",
  "webServerRunning": true,
  "activeGroup": "TEST1",
  "activePattern": "p01",
  "activeSampleSet": "S3",
  "playing": false,
  "editMode": false,
  "bpm": 120,
  "swing": 8,
  "selectedTrack": 0,
  "selectedStep": 0,
  "patternGroupDirty": false,
  "sdCardInserted": true,
  "sdCardReady": true,
  "psramAvailable": true
}
```
Pattern:
```
{
  "ok": true,
  "name": "p01",
  "bpm": 120,
  "swing": 8,
  "chainEnabled": false,
  "chainTarget": "",
  "chainLength": 1,
  "tracks": [
    {
      "name": "kick",
      "mute": false,
      "steps": [
        {
          "trigger": false,
          "mute": false,
          "velocity": 128,
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
## Final Deliverables

Produce:

1. Firmware API implementation.
2. SPA files.
3. Any new headers.
4. Updates to existing web server manager.
5. Documentation updates in developerGuide.
6. A short test checklist.
7. A commit message.

Do not remove existing hardware UI functionality.
Do not change existing coding style.
Do not replace the current sequencer model unless absolutely required.