# `src/settingsStore.cpp` — Persistent Storage (NVS, LittleFS, SD, JSON)

**Purpose:** Multi-layer persistent storage abstraction: NVS for runtime UI settings, LittleFS for local flash settings, SD card for pattern groups (JSON), and pattern import/export with ArduinoJson serialization.

---

## Responsibilities

```
1. Persist active pattern group name (NVS)
2. Persist active sample set name (NVS)
3. Persist display rotation (NVS)
4. Persist theme color index (NVS)
5. Persist encoder direction reversal (NVS)
6. Persist master output gain (NVS)
7. Persist WiFi credentials (NVS via WiFiManagerExt)
8. Mount LittleFS for local runtime settings
9. Serialize PatternData to JSON (pretty-printed)
10. Deserialize JSON to PatternData (strict parsing)
11. List pattern groups on SD card
12. Load/save/copy/rename/delete pattern groups from SD
13. Report SD card usage and free space
```

---

## Storage Layers

**Layer 1: NVS (Non-Volatile Storage)**

- **Purpose:** Persist UI state across power-cycles (rotation, theme, encoder, gain)
- **Namespace:** "groovebox"
- **Key-value pairs:** All strings or uint8_t

| Key | Type | Example |
|-----|------|---------|
| `activeGroup` | String | "DEMO" |
| `activeSampleSet` | String | "S1" |
| `displayRot` | uint8_t | 0 (portrait) |
| `themeIdx` | uint8_t | 0 (light) |
| `encoderReverse` | uint8_t | 0 (normal) |
| `masterGain` | uint8_t | 90 |

**Layer 2: LittleFS**

- **Purpose:** Flash filesystem for local settings (future expansion)
- **Mount point:** Typically `/littlefs` or `/data`
- **Currently:** Minimal usage; reserved for future runtime settings

**Layer 3: SD card (FAT filesystem)**

- **Purpose:** Persist pattern groups as JSON files
- **Directory:** `/patterns/<GROUP>/pNN.json`
- **Format:** Pretty-printed JSON (human-readable)

---

## Core Data Structures

**RuntimeSettings struct:**

```cpp
struct RuntimeSettings {
  String activePatternGroup;
  String activeSampleSet;
  uint8_t displayRotation;
  uint8_t themeColorIndex;
  bool encoderDirectionReversed;
  uint8_t masterGainPercent;
};
```

Snapshot of all runtime UI settings. Loaded at boot, saved on change.

**PatternData struct:**

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
  // Array of 6 tracks with trigger/mute/velocity/probability/lock data
};
```

JSON-serializable pattern format. Mapped 1:1 to file format.

---

## JSON Format (Pattern File)

**Example: `/patterns/DEMO/p01.json`**

```json
{
  "version": 1,
  "name": "Groove 1",
  "bpm": 120,
  "swing": 0,
  "chainEnabled": false,
  "chainTarget": 1,
  "chainLength": 1,
  "masterLevel": 90,
  "tracks": [
    {
      "triggers": [1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0],
      "velocity": [100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100],
      "probability": [100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100],
      "pitchLocked": false,
      "pitchValue": 0,
      "decayLocked": false,
      "decayValue": 100,
      "mute": false
    },
    // 5 more track objects...
  ]
}
```

---

## Key Internal Functions

**settingsStoreLoadRuntimeSettings(RuntimeSettings& out) → bool:**

```
1. Open NVS namespace "groovebox"
2. Read all runtime keys (activeGroup, displayRot, etc.)
3. Populate RuntimeSettings struct
4. Close NVS
5. Return true if successful
```

**settingsStoreSaveRuntimeSettings(const RuntimeSettings& in) → bool:**

```
1. Open NVS namespace "groovebox"
2. Write all runtime keys
3. Close NVS
4. Return true if successful
```

**buildPatternJsonDocument(const PatternData& pd) → JsonDocument:**

Uses ArduinoJson to construct a JSON object tree from PatternData. Ensures consistent formatting and field order.

**parsePatternJsonDocument(const JsonDocument& doc) → PatternData:**

Extracts PatternData from JSON with error checking. Returns default pattern if parse fails.

**settingsStoreListPatternGroupsOnCard(String groupNames[], uint8_t maxCount) → uint8_t:**

```
1. Open SD at /patterns
2. List subdirectories (group names)
3. Filter valid group names
4. Return count, populate array
```

**settingsStoreLoadPatternFromCard(const char* groupName, uint8_t patternIndex, PatternData& out) → bool:**

```
1. Build path: /patterns/<groupName>/p<NN>.json
2. Open file
3. Parse JSON with ArduinoJson
4. Populate PatternData
5. Close file
6. Return true if successful
```

**settingsStoreSavePatternToCard(const char* groupName, uint8_t patternIndex, const PatternData& data) → bool:**

```
1. Check if /patterns/<groupName> exists; create if needed
2. Build path: /patterns/<groupName>/p<NN>.json
3. Serialize PatternData to JSON
4. Write with pretty formatting (2-space indent)
5. Close file
6. Return true if successful
```

**settingsStoreRenamePatternGroupOnCard(const char* oldName, const char* newName) → bool:**

```
1. Check /patterns/<oldName> exists
2. Check /patterns/<newName> does NOT exist
3. Rename directory via SD.rename()
4. Return true if successful
```

**settingsStoreCopyPatternGroupOnCard(const char* sourceName, const char* destName) → bool:**

```
1. Check /patterns/<sourceName> exists
2. Check /patterns/<destName> does NOT exist
3. Create dest directory
4. Copy all pNN.json files from source to dest
5. Return true if successful
```

**settingsStoreDeletePatternGroupFromCard(const char* groupName) → bool:**

```
1. Check SD card ready
2. Check group is not the currently active group (protect)
3. Delete all pNN.json files in group
4. Delete group directory
5. Return true if successful OR false with boot log warning
```

---

## Public Functions

### `settingsStoreLoadRuntimeSettings(RuntimeSettings& out) → bool`

**Purpose:** Load all UI settings from NVS.

**Actions:**

1. Open NVS namespace "groovebox"
2. Read each setting with default fallback
3. Populate output struct

**Call during:** `setup()`, after display init

**Returns:** true if load succeeded

### `settingsStoreSaveRuntimeSettings(const RuntimeSettings& in) → bool`

**Purpose:** Save UI settings to NVS.

**Actions:**

1. Open NVS namespace "groovebox"
2. Write each setting
3. Close and commit

**Call when:** User changes rotation, theme, encoder direction, etc.

### **NVS Accessor Functions**

```cpp
String settingsStoreGetActivePatternGroup();
void settingsStoreSetActivePatternGroup(const String& name);

String settingsStoreGetActiveSampleSet();
void settingsStoreSetActiveSampleSet(const String& name);

uint8_t settingsStoreGetDisplayRotation();
void settingsStoreSetDisplayRotation(uint8_t rot);

uint8_t settingsStoreGetThemeColorIndex();
void settingsStoreSetThemeColorIndex(uint8_t idx);

bool settingsStoreGetEncoderDirectionReversed();
void settingsStoreSetEncoderDirectionReversed(bool reversed);

uint8_t settingsStoreGetMasterGainPercent();
void settingsStoreSetMasterGainPercent(uint8_t percent);
```

Each reads/writes a single NVS key.

### **SD Pattern I/O Functions**

### `settingsStoreListPatternGroupsOnCard(String groupNames[], uint8_t maxCount) → uint8_t`

**Purpose:** Enumerate all pattern groups on SD card.

**Returns:** Number of groups found, up to maxCount

Used by UI pattern group menu.

### `settingsStoreLoadPatternFromCard(const char* groupName, uint8_t patternIndex, PatternData& out) → bool`

**Purpose:** Load one pattern JSON file from SD.

**Parameters:**

- `groupName` — "DEMO", "MyGroup", etc.
- `patternIndex` — 1–64 (mapped to p01.json–p64.json)
- `out` — PatternData struct to populate

**Returns:** true if load succeeded, false on file not found or parse error

### `settingsStoreSavePatternToCard(const char* groupName, uint8_t patternIndex, const PatternData& data) → bool`

**Purpose:** Save one pattern JSON file to SD.

**Actions:**

1. Create `/patterns/<groupName>` if needed
2. Serialize `data` to pretty JSON
3. Write to `/patterns/<groupName>/p<NN>.json`
4. Return true if successful

**Overwrites:** Existing file without confirmation (caller should prompt user)

### `settingsStoreListPatternsInGroupOnCard(const char* groupName, String names[], uint8_t maxCount) → uint8_t`

**Purpose:** List pNN.json files in one group.

**Returns:** Number of valid pattern files (1–64)

### `settingsStoreCopyPatternGroupOnCard(const char* sourceName, const char* destName) → bool`

**Purpose:** Duplicate one group directory entirely.

**Actions:** Create dest directory and copy all pattern files.

### `settingsStoreRenamePatternGroupOnCard(const char* oldName, const char* newName) → bool`

**Purpose:** Rename a group directory.

**Constraints:** Destination name must not exist.

### `settingsStoreDeletePatternGroupFromCard(const char* groupName) → bool`

**Purpose:** Delete a group directory and all patterns.

**Protection:** Refuses to delete the currently active group (avoid data loss).

### `settingsStoreGetSdUsage(uint64_t& used, uint64_t& total) → bool`

**Purpose:** Report SD card usage statistics.

Used by UI diagnostics screen.

### `settingsStoreDeletePatternFromCard(const char* groupName, uint8_t patternIndex) → bool`

**Purpose:** Delete one pattern file from a group.

Used when UI offers individual pattern delete action.

---

## NVS Namespace and Keys

All settings stored under NVS namespace "groovebox":

- `activeGroup` — active pattern group name
- `activeSampleSet` — active sample set (S1–S9)
- `displayRot` — display rotation (0–3)
- `themeIdx` — theme color index (0–2)
- `encoderReverse` — encoder direction (0 or 1)
- `masterGain` — master output level (0–100)

Each key persists independently; reading a missing key returns a sensible default.

---

## Pattern Filename Convention

**Strict format:** `p<NN>.json` where NN is 01–64

- `p01.json` — pattern 1
- `p16.json` — pattern 16
- `p64.json` — pattern 64

No other filenames are recognized. Legacy patterns or misnamed files are ignored.

---

## JSON Pretty Printing

Pattern files are written with 2-space indentation for readability:

```json
{
  "version": 1,
  "name": "Groove",
  "bpm": 120,
  ...
}
```

Not minified. This uses slightly more SD space but aids manual editing and debugging.

---

## Dependencies

- `ArduinoJson` — JSON serialization/deserialization
- `SD` library — SD card I/O
- `Preferences` / NVS — non-volatile storage
- `LittleFS` — local flash filesystem (prepared for future use)
- `sequencer.h` — PatternData struct definition

---

## Important Implementation Notes

1. **No blocking I/O from audioTask().** All SD/NVS operations happen in UiTask or SystemTask.

2. **Active group protection.** Never delete the currently loaded group without explicit user confirmation (already implemented via UI).

3. **JSON version field.** All patterns include a version number. If JSON format changes, increment this and add migration logic.

4. **Pretty JSON for human readability.** Developers and users can edit pattern JSON directly on SD card if needed.

5. **NVS commit atomicity.** Each NVS write is committed immediately (no batch updates).

6. **Default fallback.** Missing NVS keys return sensible defaults (e.g., "DEMO" group, "S1" sample set). Device always boots with valid state.

7. **SD card errors logged.** IO errors return false and log to boot log. Device remains playable, just can't save/load.

---

[⬆ UP](developerBuildGuide.md#20-source-file-reference) | [📖 README](../README.md#)


## v1.3.8 Step Mute Storage

Each step stores its own `mute` boolean. This field is independent from the track-level `mute` flag. Existing pattern files that do not contain the per-step `mute` field should load with `mute=false` for every step. New pattern files should save the field so STEP OFF states survive reloads.
