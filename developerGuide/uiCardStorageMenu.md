# `src/uiCardStorageMenu.cpp` — Card Storage Menu View

**Purpose:** View layer for Card Storage submenu. Renders list of pattern group operations. Does not execute actions; `uiManager.cpp` dispatches user input.

---

## Menu Items

```
1. Save Group *               (with * if RAM patterns modified)
2. Load Group
3. New Group
4. Rename Group
5. Copy Group
6. Delete Group
7. ─────────────────          (separator)
8. Exit
```

Save Group shows * (asterisk) if `patternGroupDirty` flag is set, alerting user of unsaved changes.

---

## Responsibilities

```
1. Build menu item strings
2. Show Save Group dirty marker (*)
3. Manage first-visible index for scrolling
4. Format text to fit 320×240 screen
5. Render menu list
```

---

## Public Functions

### `uiCardStorageMenuDraw(const UiState& state, uint8_t selectedIndex)`

**Purpose:** Draw the Card Storage submenu.

**Parameters:**

- `state` — UI state
- `selectedIndex` — currently highlighted item

**Actions:**

1. Build menu item strings
2. Add * to "Save Group" if `patternGroupDirty` is true
3. Call `display.drawListScreen(...)`
4. Highlight selected item

---

## Dependencies

- `DisplayDriverClass.h` — menu drawing

---

## Important Implementation Notes

1. **Keep order synchronized.** Action mapping in `uiManager.cpp` must match menu item order.

2. **Dirty marker critical.** Asterisk on "Save Group" prevents accidental pattern loss.

3. **No action execution.** This file is view-only. Actions (save, load, rename, delete) dispatched in `uiManager.cpp` and `uiCardStorageActions.cpp`.

4. **Protected active group.** Delete Group action must never offer the currently loaded group as a deletable target (protection in `uiManager.cpp`).

---

[⬆ UP](developerBuildGuide.md#20-source-file-reference) | [📖 README](../README.md#)
