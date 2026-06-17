# `src/uiCardStorageMenu.cpp`

[Back](developerBuildGuide.md)

---

## Purpose

This file draws the Card Storage submenu. It is only the view layer; storage actions are executed in `uiManager.cpp` and `uiCardStorageActions.cpp`.

---

## Responsibilities

```text
draw Save Group / Load Group / Rename Group / Copy Group / Delete Group / Exit
show dirty marker on Save Group
clip row text
keep selected row visible in scroll window
```

---

## Important Implementation Notes

- Keep row labels short enough for the screen.
- Menu index mapping must match `handleCardStorageMenuEncoderEvent()` in `uiManager.cpp`.
- Delete Group must never expose the active group as a deletable target.

---

## Important Internal Areas

```text
cardStorageMenuEntryCount
updateCardStorageFirstVisibleIndex()
patternGroupDirty dependent label
```

---

## Public Functions

### `uiCardStorageMenuDraw(...)`

Draws the Card Storage menu and updates the first-visible index.


---

[UP](developerBuildGuide.md) | [README](../README.md)
