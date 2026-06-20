# `src/uiSequencerInput.cpp` — Common UI List Navigation Utilities

**Purpose:** Reusable helper functions for menu/list navigation. Prevents code duplication across different UI modules.

---

## Responsibilities

```
1. Normalize circular menu selection (wrapping)
2. Update list viewport scroll position
3. Keep selected item visible in scrolling window
4. No display-specific logic (generic utilities)
```

---

## Public Functions

### `uiNormalizeMenuSelection(int& index, int totalCount)`

**Purpose:** Wrap a menu selection index into valid range with circular wrapping.

**Parameters:**

- `index` — current selection index (may be < 0 or >= totalCount)
- `totalCount` — number of menu items

**Behavior:**

```
index = -1, totalCount = 5  →  index becomes 4 (wrap to end)
index = 5,  totalCount = 5  →  index becomes 0 (wrap to start)
index = 2,  totalCount = 5  →  index stays 2 (in range)
```

**Used by:** All menu modules (Groovebox, Settings, Card Storage, etc.)

### `uiUpdateListFirstVisibleIndex(uint8_t& firstVisible, uint8_t selectedIndex, uint8_t visibleRowCount, uint8_t totalCount)`

**Purpose:** Scroll a list viewport to keep the selected item visible.

**Parameters:**

- `firstVisible` — index of first visible row (maintained by caller)
- `selectedIndex` — currently selected item
- `visibleRowCount` — max rows visible on screen (e.g., 6 for Groovebox tracks or 8 for menu lists)
- `totalCount` — total items in list

**Behavior:**

```
If selected row is above visible area:
  Scroll up → firstVisible = selectedIndex

If selected row is below visible area:
  Scroll down → firstVisible = selectedIndex - visibleRowCount + 1

If selected row is in visible area:
  No change
```

**Example:**

```
List of 12 items, 6 visible:
  selectedIndex = 2    → firstVisible = 0 (stay at top)
  selectedIndex = 5    → firstVisible = 0 (last visible row)
  selectedIndex = 6    → firstVisible = 1 (scroll one row)
  selectedIndex = 11   → firstVisible = 6 (scroll to show item 11)
```

---

## Usage Across UI Modules

**In menu drawing:**

```cpp
// Draw Settings menu with scrolling
uiUpdateListFirstVisibleIndex(uiState.settingsFirstVisible, 
                               selectedIndex, 
                               8,  // max visible rows
                               totalSettingsItems);

for (int i = uiState.settingsFirstVisible; 
     i < uiState.settingsFirstVisible + 8; 
     i++) {
  drawMenuRow(i, /* ... */);
}
```

**In menu input handling:**

```cpp
// Encoder rotate
selectedIndex--;
uiNormalizeMenuSelection(selectedIndex, totalSettingsItems);
uiUpdateListFirstVisibleIndex(/* scroll params */);
```

---

## Design Pattern

These utilities are **display-independent**:

- No screen dimensions hardcoded
- No call to display library functions
- Generic enough to work in any menu context

This allows reuse across:

- Groovebox track list (6 visible)
- Settings menu (8 visible)
- Card Storage menu (varies)
- Pattern selection menu
- Sample set menu

---

## Dependencies

None. Pure utilities with no external dependencies.

---

## Important Implementation Notes

1. **Wrapping is circular.** Going offscreen in one direction wraps to opposite end (standard UI behavior).

2. **Scroll smoothing optional.** These functions calculate final scroll position immediately; callers can animate if desired.

3. **Viewport size flexible.** Any `visibleRowCount` works; caller specifies based on screen layout.

4. **Efficiency.** O(1) operations, safe to call every frame without performance cost.

---

[⬆ UP](developerBuildGuide.md#20-source-file-reference) | [📖 README](../README.md#)
