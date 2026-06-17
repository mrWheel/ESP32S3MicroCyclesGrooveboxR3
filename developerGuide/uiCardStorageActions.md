# `src/uiCardStorageActions.cpp`

[Back](developerBuildGuide.md)

---

## Purpose

This file contains non-interactive SD card group utility actions used by the Card Storage workflow.

---

## Responsibilities

```text
copy pattern groups
rename pattern groups
delete stale pattern files after saving a shorter group
return status messages to UI layer
```

---

## Important Implementation Notes

- Keep UI prompts outside this file.
- Do not delete the active group here; active-group protection belongs to the UI workflow before action execution.
- This module assumes SD card availability has already been checked.

---

## Important Internal Areas

```text
uiCardStorageCopyPatternGroup()
uiCardStorageRenamePatternGroup()
uiCardStorageDeleteStalePatterns()
pattern slot naming assumptions
```

---

## Public Functions

### `uiCardStorageCopyPatternGroup(...)`

Copies one pattern group and returns a status message.

### `uiCardStorageRenamePatternGroup(...)`

Renames one pattern group and returns a status message.

### `uiCardStorageDeleteStalePatterns(...)`

Deletes pNN.json files above the current loaded pattern count after save.


---

[UP](developerBuildGuide.md) | [README](../README.md)
