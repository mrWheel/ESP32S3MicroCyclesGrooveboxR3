# `src/uiSystemSettingsMenu.cpp` — System Settings Menu View

**Purpose:** View layer for System Settings menu. Renders list of settings and status values. Does not execute actions; `uiManager.cpp` handles user input dispatch.

---

## Menu Items

Menu entries displayed in order:

```
1. [SSID: MyNetwork]           (info, disabled)
2. [IP: 192.168.1.100]         (info, disabled)
3. [MAC: AA:BB:CC:DD:EE:FF]    (info, disabled)
4. ─────────────────            (separator)
5. WiFi Setup
6. Save Group *                 (with * if modified)
7. Load Sample Set
8. Card Storage
9. ─────────────────           (separator)
10. Display Theme
11. Display Rotation
12. Encoder Direction
13. ─────────────────           (separator)
14. Exit
```

Informational rows (SSID, IP, MAC) are disabled (not selectable).

---

## Responsibilities

```
1. Build menu item strings
2. Include SSID/IP/MAC from systemManager
3. Show Save Group dirty marker (*)
4. Manage first-visible index for scrolling
5. Format text to fit 320×240 screen
6. Render full menu list
```

---

## Public Functions

### `uiSystemSettingsMenuDraw(const UiState& state, uint8_t selectedIndex)`

**Purpose:** Draw the System Settings menu list.

**Parameters:**

- `state` — UI state with settings
- `selectedIndex` — currently highlighted item

**Actions:**

1. Build all menu item strings
2. Include current SSID/IP/MAC from systemManager
3. Add * (asterisk) to "Save Group" if `patternGroupDirty` flag set
4. Call `display.drawListScreen(...)`
5. Highlight selected item

---

## Dependencies

- `DisplayDriverClass.h` — menu drawing
- `systemManager.h` — SSID/IP/MAC retrieval

---

## Important Implementation Notes

1. **Keep order synchronized.** Update both entry count and action mapping in `uiManager.cpp` when adding/removing items.

2. **Informational rows non-selectable.** SSID/IP/MAC rows are display-only, can't be highlighted.

3. **Dirty marker visibility.** The asterisk (*) on "Save Group" reminds user of unsaved changes—very important user-facing feedback.

4. **No action execution here.** This file only renders; action dispatch lives in `uiManager.cpp`.

---

[⬆ UP](developerBuildGuide.md#20-source-file-reference) | [📖 README](../README.md#)
