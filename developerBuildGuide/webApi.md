# `src/webApi.cpp` — REST API Endpoints for Remote Control and Status

**Purpose:** Register and implement REST API endpoints for remote control of the Groovebox, pattern management, sequencer state, transport controls, sample set management, and real-time system status queries.

---

## Responsibilities

```
1. Register HTTP GET/POST routes with the WebServer instance
2. Parse incoming JSON request bodies and validate parameters
3. Query sequencer state, pattern data, and system status
4. Execute transport commands (play, stop, toggle, BPM, swing)
5. Load and save pattern groups from/to SD card
6. Manage pattern creation, deletion, and duplication
7. Handle track and step-level editing via API
8. List and switch between active sample sets
9. Query device memory and diagnostics
10. Return JSON responses with proper HTTP status codes
```

---

## Status Endpoints

### `GET /api/status`

Returns comprehensive device and sequencer status.

**Response:**
```json
{
  "ok": true,
  "version": "v1.4.5",
  "ip": "192.168.0.100",
  "ssid": "MyWiFi",
  "wifiConnected": true,
  "webServerRunning": true,
  "activeGroup": "DEMO",
  "activePattern": "p01",
  "activeSampleSet": "S1",
  "playing": true,
  "editMode": false,
  "bpm": 120,
  "swing": 8,
  "currentStep": 4,
  "activePatternIndex": 0,
  "playingPatternIndex": 0,
  "selectedTrack": 0,
  "selectedStep": 0,
  "patternGroupDirty": false,
  "sdCardInserted": true,
  "sdCardReady": true,
  "psramAvailable": true,
  "freePsram": 3145728,
  "freeHeap": 65536
}
```

### `GET /api/transport`

Returns transport state (play/stop, BPM, swing, current step).

### `GET /api/sequencer/view`

Returns complete sequencer view with all pattern and track state.

### `GET /api/sequencer/playhead`

Returns current playhead position (step, pattern index).

### `GET /api/sequencer/cursor`

Returns UI cursor position (track, step).

## Transport Control Endpoints

### `POST /api/transport/play`

Start playback from the active pattern.

### `POST /api/transport/stop`

Stop playback immediately.

### `POST /api/transport/toggle`

Toggle between play and stop.

### `POST /api/transport/bpm`

Set BPM via JSON body.

**Request:**
```json
{
  "bpm": 125
}
```

### `POST /api/transport/swing`

Set swing percentage via JSON body.

**Request:**
```json
{
  "swing": 12
}
```

### `POST /api/sequencer/editMode`

Set edit mode on/off.

**Request:**
```json
{
  "editMode": true
}
```

## Pattern Group Endpoints

### `GET /api/groups`

List all pattern groups on SD card.

**Response:**
```json
{
  "ok": true,
  "activeGroup": "DEMO",
  "groups": ["DEMO", "MyGroup", "Jazz"]
}
```

### `GET /api/groups/active`

Get active group metadata.

**Response:**
```json
{
  "ok": true,
  "name": "DEMO",
  "patternCount": 12
}
```

### `POST /api/groups/load`

Load a pattern group into RAM.

**Request:**
```json
{
  "groupName": "DEMO"
}
```

### `POST /api/groups/save`

Save active RAM patterns back to the active group directory.

### `POST /api/groups/new`

Create and activate a new pattern group.

**Request:**
```json
{
  "name": "NewGroup"
}
```

### `POST /api/groups/rename`

Rename an existing pattern group.

**Request:**
```json
{
  "oldName": "OldGroup",
  "newName": "RenamedGroup"
}
```

### `POST /api/groups/copy`

Duplicate a pattern group on SD card.

**Request:**
```json
{
  "source": "DEMO",
  "destination": "DEMO_Copy"
}
```

### `POST /api/groups/delete`

Delete a pattern group from SD card.

**Request:**
```json
{
  "groupName": "OldGroup"
}
```

## Pattern Endpoints

### `GET /api/patterns`

List all patterns currently in RAM.

**Response:**
```json
{
  "ok": true,
  "patterns": ["p01", "p02", "p03"]
}
```

### `GET /api/patterns/active`

Get the active (editing) pattern index and name.

### `POST /api/patterns/active`

Set the active pattern by slot index.

**Request:**
```json
{
  "patternIndex": 2
}
```

### `GET /api/patterns/<name>`

Get complete pattern data for pattern `p01`, `p02`, etc.

**Response:**
```json
{
  "ok": true,
  "name": "p01",
  "bpm": 120,
  "swing": 8,
  "chainEnabled": false,
  "chainLength": 1,
  "chainTarget": 1,
  "tracks": [
    {
      "name": "KICK",
      "mute": false,
      "steps": [
        {
          "trigger": 1,
          "mute": 0,
          "velocity": 100,
          "probability": 100,
          "lockEnabled": false,
          "lockPitch": 0,
          "lockDecay": 100
        },
        ...
      ]
    },
    ...
  ]
}
```

### `PUT /api/patterns/<name>`

Update entire pattern from JSON body.

### `POST /api/patterns/<name>/clear`

Clear all triggers in a pattern.

### `POST /api/patterns/<name>/copy`

Duplicate a pattern to another slot.

**Request:**
```json
{
  "targetSlot": 5
}
```

## Step and Track Editing Endpoints

### `POST /api/patterns/<name>/steps/<stepIndex>`

Edit a single step across all tracks.

**Request:**
```json
{
  "track": 0,
  "trigger": 1,
  "velocity": 100,
  "probability": 100,
  "lockPitch": 2,
  "lockDecay": 80
}
```

### `POST /api/patterns/<name>/tracks/<trackIndex>`

Edit an entire track data.

**Request:**
```json
{
  "mute": false
}
```

### `POST /api/patterns/<name>/tracks/<trackIndex>/mute`

Toggle track mute state.

## Sample Set Endpoints

### `GET /api/sampleSets`

List all sample sets (S1–S9) on SD card.

**Response:**
```json
{
  "ok": true,
  "activeSampleSet": "S1",
  "sampleSets": ["S1", "S2", "S3"]
}
```

### `GET /api/sampleSets/active`

Get active sample set name.

### `POST /api/sampleSets/load`

Load and activate a sample set.

**Request:**
```json
{
  "sampleSet": "S2"
}
```

### `GET /api/samples`

List all samples in the active sample set.

**Response:**
```json
{
  "ok": true,
  "activeSampleSet": "S1",
  "samples": [
    {
      "name": "kick",
      "duration": 1.5,
      "sampleCount": 66150
    },
    ...
  ]
}
```

### `GET /api/samples/<sampleName>/gain`

Get gain profile for a single sample.

---

## Internal Helpers

### Helper Functions

**`sendJson(WebServer& server, JsonDocument& doc, int code = 200)`**

Serialize a JSON document and send it as an HTTP response with optional status code.

**`sendOk(WebServer& server)`**

Send a simple JSON success response: `{"ok": true}`.

**`sendError(WebServer& server, const char* message, int code = 400)`**

Send a JSON error response: `{"ok": false, "error": "<message>"}`.

**`slotIndexToPatternName(uint8_t slotIndex)`**

Convert slot index (0–47) to pattern name (`p01`–`p48`).

**`patternNameToSlotIndex(const String& name)`**

Convert pattern name to slot index; returns -1 if invalid.

**`buildPatternJson(JsonDocument& doc, uint8_t slotIndex)`**

Build and populate a complete pattern JSON object from a slot.

**`parsePatternFromJson(const JsonDocument& doc, PatternData& patternData)`**

Parse pattern JSON from request body into internal PatternData structure.

---

## Public Functions

### `webApiRegisterRoutes(WebServer& server)`

**Purpose:** Register all API endpoints with the WebServer instance.

**Called by:** `webServerManagerInit()` in `main.cpp`.

**Behavior:**

- Iterates through all handler functions and registers routes via `server.on(path, method, handler)`
- Routes are registered exactly once during boot
- Both `GET` and `POST` methods use their respective HTTP semantics

**Important:** This function must be called before `webServer.begin()` to ensure all routes are available.

---

## Error Responses

All errors return JSON with `"ok": false` and an error message:

```json
{
  "ok": false,
  "error": "SD card not ready"
}
```

**HTTP status codes:**

- `200` — Success
- `400` — Bad request (missing/invalid JSON)
- `404` — Endpoint not found
- `409` — Conflict (e.g., SD card not inserted)
- `503` — Service unavailable (e.g., SD card not ready)
- `500` — Internal error

---

## Dependencies

- `ArduinoJson` — JSON serialization and parsing
- `WebServer` — HTTP request/response handling
- `sequencer.h` — Pattern state and control
- `settingsStore.h` — Group/pattern persistence
- `sampleManager.h` — Sample set management
- `audioEngine.h` — Audio state
- `uiManager.h` — UI state queries
- `systemManager.h` — WiFi and device info
- `webServerManager.h` — Server state
- `progVersion.h` — Firmware version string
- `esp_heap_caps.h` — Memory diagnostics

---

## Important Implementation Notes

1. **No blocking operations:** API handlers run on the same core as the web server and must not block for more than a few milliseconds.

2. **Thread safety:** All API handlers call thread-safe query functions (e.g., `sequencerGetView()`, `sampleManagerGetActiveSampleSet()`). Avoid direct mutation from API handlers.

3. **No SD I/O during playback:** All SD card operations should be fast or delegated to SystemTask to avoid audio dropouts. Pattern loads should validate the SD card is ready first.

4. **JSON document size:** Use `JsonDocument` without explicit size for automatic sizing; the stack may be limited on Core 1.

5. **Error handling:** Always check return values from query functions and return appropriate HTTP status codes.

6. **No memory allocation in handlers:** Already guaranteed by using static JsonDocument templates.

7. **Version string:** Use `PROG_VERSION` macro from `progVersion.h`, not a hardcoded string.

---

[⬆ UP](developerBuildGuide.md#20-source-file-reference) | [📖 README](../README.md#)
