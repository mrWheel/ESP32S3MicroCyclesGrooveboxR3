# `src/uiSequencerInput.cpp`

[Back](developerBuildGuide.md)

---

## Purpose

This tiny utility file contains reusable list/menu navigation helpers.

---

## Responsibilities

```text
normalize circular menu selection
keep selected list row visible in a scrolling viewport
```

---

## Important Implementation Notes

- Use these helpers instead of duplicating list wrap/scroll code.
- They are display-independent and safe to reuse in UI modules.

---

## Important Internal Areas

```text
uiNormalizeMenuSelection()
uiUpdateListFirstVisibleIndex()
```

---

## Public Functions

### `uiNormalizeMenuSelection(int&, int)`

Wraps a selection index into a valid range.

### `uiUpdateListFirstVisibleIndex(...)`

Scrolls a list viewport to keep the selected item visible.


---

[UP](developerBuildGuide.md) | [README](../README.md)
