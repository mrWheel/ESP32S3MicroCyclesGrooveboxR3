# `src/uiCardStorageActions.cpp` — Pattern Group SD Card Operations

**Purpose:** Non-interactive pattern group operations (copy, rename, delete stale patterns). Delegates to `settingsStore.h` for actual SD I/O. Returns status messages for UI display.

---

## Responsibilities

```
1. Copy a pattern group directory on SD card
2. Rename a pattern group directory
3. Delete stale pattern files (numbered above current count)
4. Return status/error messages to UI
5. Assume SD card is ready (caller pre-checks)
```

---

## Typical Workflow

**Copy Group:**

```
User selects "Copy Group" → uiManager opens text input
User enters new name (e.g., "DEMO2")
User confirms → uiCardStorageCopyPatternGroup("DEMO", "DEMO2")
  ↓
settingsStore copies /patterns/DEMO/* → /patterns/DEMO2/*
  ↓
Return status message ("Copied: DEMO2")
  ↓
UI closes menu and redraws
```

**Rename Group:**

```
User selects "Rename Group" → text input with old name pre-filled
User edits name → uiCardStorageRenamePatternGroup("DEMO", "DEMO_Old")
  ↓
settingsStore renames /patterns/DEMO → /patterns/DEMO_Old
  ↓
Return status message ("Renamed: DEMO_Old")
```

**Delete Stale Patterns:**

```
After Save Group: p01–p16 were saved, but old group had p01–p32
  ↓
uiCardStorageDeleteStalePatterns("DEMO", 16)
  ↓
Delete p17.json through p32.json
  ↓
Return count deleted
```

---

## Public Functions

### `uiCardStorageCopyPatternGroup(const char* sourceGroupName, const char* destGroupName) → String`

**Purpose:** Copy a pattern group on SD card.

**Parameters:**

- `sourceGroupName` — original group (e.g., "DEMO")
- `destGroupName` — new group name (e.g., "DEMO2")

**Returns:** Status message (e.g., "Copied DEMO → DEMO2" or "Error: Destination exists")

**Actions:**

1. Validate source group exists
2. Assert destination doesn't exist (prevent overwrite)
3. Call `settingsStore.settingsStoreCopyPatternGroupOnCard(...)`
4. Return status string for UI display

### `uiCardStorageRenamePatternGroup(const char* oldGroupName, const char* newGroupName) → String`

**Purpose:** Rename a pattern group directory.

**Parameters:**

- `oldGroupName` — current name (e.g., "DEMO")
- `newGroupName` — new name (e.g., "DEMO_Archive")

**Returns:** Status message

**Actions:**

1. Validate old group exists
2. Assert new name is unique
3. Call `settingsStore.settingsStoreRenamePatternGroupOnCard(...)`
4. Return status for UI

### `uiCardStorageDeleteStalePatterns(const char* groupName, uint8_t validPatternCount) → uint8_t`

**Purpose:** Clean up old pattern files after saving a shorter pattern count.

**Scenario:**

A group was loaded with 32 patterns. User deletes some, now only 16 are used. Save Group writes p01–p16. Stale files p17–p32 remain on SD.

**Parameters:**

- `groupName` — group to clean (e.g., "DEMO")
- `validPatternCount` — number of patterns to keep (e.g., 16)

**Returns:** Number of files deleted

**Actions:**

1. For each file `p<NN>.json` where NN > validPatternCount
2. Delete the file
3. Return count deleted

---

## Dependencies

- `settingsStore.h` — actual SD operations
- Display messages (returned as strings, caller displays to UI)

---

## Important Implementation Notes

1. **UI prompts outside.** This file does not show confirmation dialogs. Caller (uiManager) handles user confirmation before calling these functions.

2. **Active group protection.** Do NOT delete the active group here. That protection belongs to the UI workflow (uiManager checks if group is active before calling delete).

3. **SD ready assumed.** Caller must verify `sampleManagerIsSdCardReady()` before invoking these functions.

4. **Status messages user-facing.** Return strings are displayed in UI message popup (e.g., "Copied: DEMO2" or "Error: Not enough space").

5. **Non-blocking.** These operations delegate to settingsStore which handles I/O. No malloc() or blocking waits here.

---

[⬆ UP](developerBuildGuide.md#20-source-file-reference) | [📖 README](../README.md#)
