# `src/settingsStore.cpp`

[Back](developerBuildGuide.md)

---

## Purpose

This file owns persistent storage: NVS runtime values, LittleFS support, SD card pattern group operations and pattern JSON serialization/parsing.

---

## Responsibilities

```text
persist active pattern group
persist active sample set
persist display rotation, theme and encoder order
persist master gain
read/write WiFi credentials
mount LittleFS for runtime settings
serialize PatternData to JSON
parse PatternData from JSON
list, load, save, rename, copy and delete SD pattern groups
report SD/LittleFS usage
```

---

## Important Implementation Notes

- Do not reintroduce legacy JSON compatibility unless intentionally versioned.
- Pattern writes must be pretty/maintainable JSON.
- Do not silently delete or overwrite the active group.
- UI must guard SD-card actions before calling SD functions.
- Keep schema changes documented in `developerBuildGuide.md`.

---

## Important Internal Areas

```text
NVS_NAMESPACE groovebox
RuntimeSettings defaults
pattern filename normalization
buildPatternJsonDocument()
parsePatternJsonDocument()
strict pNN.json pattern names
settingsStoreListPatternGroupsOnCard()
settingsStoreDeletePatternGroupFromCard()
LittleFS settings file handling
```

---

## Public Functions

### `settingsStoreListPatternGroupsOnCard(...)`

Lists pattern group directories under /patterns.

### `settingsStoreGetActivePatternGroup()`

Reads active pattern group name.

### `settingsStoreSetActivePatternGroup(...)`

Writes active pattern group name.

### `settingsStoreGetActiveSampleSet()`

Reads active sample set name.

### `settingsStoreSetActiveSampleSet(...)`

Writes active sample set name.

### `settingsStoreGetDisplayRotation()`

Reads display rotation from NVS.

### `settingsStoreSetDisplayRotation(uint8_t)`

Writes display rotation.

### `settingsStoreGetEncoderOrder()`

Reads encoder reversed setting.

### `settingsStoreSetEncoderOrder(bool)`

Writes encoder reversed setting.

### `settingsStoreGetThemeColorIndex()`

Reads theme color index.

### `settingsStoreSetThemeColorIndex(int)`

Writes theme color index.

### `settingsStoreGetWifiCredentials(...)`

Reads WiFi credentials.

### `settingsStoreSetWifiCredentials(...)`

Writes WiFi credentials.

### `settingsStoreLoadRuntimeSettings(...)`

Loads combined runtime settings.

### `settingsStoreSaveRuntimeSettings(...)`

Saves combined runtime settings.

### `settingsStoreGetMasterGainPercent()`

Reads master gain.

### `settingsStoreSetMasterGainPercent(uint8_t)`

Writes master gain.

### `settingsStoreGetSdUsage(...)`

Returns SD usage values.

### `settingsStoreSavePatternToCard(...)`

Writes one PatternData JSON file to a group.

### `settingsStoreListPatternsInGroupOnCard(...)`

Lists pNN.json files in a group.

### `settingsStoreLoadPatternFromCard(...)`

Loads one PatternData JSON file.

### `settingsStoreRenamePatternGroupOnCard(...)`

Renames one group directory.

### `settingsStoreCopyPatternGroupOnCard(...)`

Copies one group directory.

### `settingsStoreLoadPatternChainSettings(...)`

Loads only chain settings from a pattern file.

### `settingsStoreDeletePatternFromCard(...)`

Deletes one pattern file.

### `settingsStoreDeletePatternGroupFromCard(...)`

Deletes one complete group directory.


---

[UP](developerBuildGuide.md) | [README](../README.md)
