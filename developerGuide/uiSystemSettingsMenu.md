# `src/uiSystemSettingsMenu.cpp` — System Settings Menu View

**Purpose:** View layer for System Settings menu. Renders list of settings and status values. Does not execute actions; `uiManager.cpp` handles user input dispatch.

---

## Menu Items

Menu entries displayed in order:

```text
1. SSID: <ssid>                (info, disabled)
2. IP: <ip>                    (info, disabled)
3. MAC: <mac>                  (info, disabled)
4. Save Group *                (with * if modified)
5. Add Pattern
6. Delete Pattern
7. Card Storage
8. Load Sample Set
9. Erase WiFi credentials
10. Start WiFiManager
11. Set Theme (<name>)
12. Rotate Display (<rotation>)
13. Encoder Order (<A-B/B-A>)
14. Restart Groovebox
15. Exit
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
