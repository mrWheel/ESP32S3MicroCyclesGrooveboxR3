# `src/uiPatternGroupInput.cpp`

[Back](developerBuildGuide.md)

---

## Purpose

This file owns the small name-input editor used for pattern group copy and rename operations.

---

## Responsibilities

```text
open/close group name input
track copy-vs-rename mode
rotate through character options
accept characters
backspace or cancel
return trimmed group name
draw input field
```

---

## Important Implementation Notes

- Long press and medium press behavior is routed in `uiManager.cpp`.
- Keep accepted names compatible with settingsStore group-name validation.
- Avoid modal leakage: while this editor is open, global long-press settings must not trigger.

---

## Important Internal Areas

```text
character token list
input buffer
copyMode flag
buildPatternGroupNameInputText()
KEY0 backspace/cancel behavior
encoder medium/long commit handled by uiManager
```

---

## Public Functions

### `uiPatternGroupInputInit()`

Initializes input state.

### `uiPatternGroupInputOpen(bool copyMode)`

Opens the input editor in copy or rename mode.

### `uiPatternGroupInputClose()`

Closes the input editor.

### `uiPatternGroupInputIsOpen()`

Returns whether input is active.

### `uiPatternGroupInputIsCopyMode()`

Returns whether input is copy mode.

### `uiPatternGroupInputGetTrimmedName()`

Returns the entered group name without padding.

### `uiPatternGroupInputDraw(...)`

Draws the group name input screen.

### `uiPatternGroupInputRotate(int)`

Moves through available characters.

### `uiPatternGroupInputAcceptCharacter()`

Accepts the current character.

### `uiPatternGroupInputBackspaceOrCancel()`

Backspaces or cancels input.


---

[UP](developerBuildGuide.md) | [README](../README.md)
