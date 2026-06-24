# `src/uiPatternGroupInput.cpp` — Pattern Group Name Input Editor

**Purpose:** Character-by-character text input UI for naming pattern groups during copy and rename operations. Supports token rotation and character validation.

---

## Responsibilities

```
1. Initialize input state
2. Open input in copy mode (new name) or rename mode (edit existing)
3. Track active input cursor
4. Rotate through available characters
5. Accept characters one by one
6. Backspace or cancel input
7. Return trimmed group name
8. Draw input field on screen
```

---

## Input States

**UiPatternGroupInputState struct:**

```cpp
struct UiPatternGroupInputState {
  bool isOpen;
  bool isCopyMode;           // true = new name, false = rename existing
  String buffer;
  uint8_t cursorPos;
  uint8_t currentTokenIndex; // position in character token set
  uint32_t lastAcceptTime;   // debounce on KEY0 confirm
};
```

---

## Character Set (Tokens)

Available characters for input (order matters for encoder rotation):

```
Uppercase:   A-Z
Digits:      0-9
Symbols:     - _ . ( )
Special:     [SPACE] [BACKSPACE] [ACCEPT]
```

Encoder rotation cycles through available characters at current cursor position.

---

## Typical Workflow

**Copy Group Flow:**

```
User selects "Copy Group" from menu
  ↓
uiManager opens text input: uiPatternGroupInputOpen(true)
  ↓
Screen shows: "Enter name: [_______]"
  ↓
User rotates encoder → cycles through A-Z, 0-9, symbols
  ↓
User presses button short → accept character, move to next position
  ↓
Repeat until name entered (e.g., "DEMO2")
  ↓
User presses button long or encoder medium → confirm
  ↓
uiPatternGroupInputGetTrimmedName() → "DEMO2"
  ↓
uiManager calls uiCardStorageCopyPatternGroup("DEMO", "DEMO2")
```

---

## Public Functions

### `uiPatternGroupInputInit()`

**Purpose:** Initialize input state to defaults.

**Call during:** `uiManagerInit()`

### `uiPatternGroupInputOpen(bool copyMode)`

**Purpose:** Open the group name input editor.

**Parameters:**

- `copyMode` — true for new name (copy), false for rename (edit existing)

**Actions:**

1. Set `isOpen = true`
2. Set `isCopyMode = copyMode`
3. Clear or populate buffer (if rename, start with old name)
4. Reset cursor position and token index
5. Flag screen for redraw

### `uiPatternGroupInputClose()`

**Purpose:** Close the input editor.

**Actions:**

1. Set `isOpen = false`
2. Clear buffer
3. Return to previous UI state

### `uiPatternGroupInputIsOpen() → bool`

**Purpose:** Check if input editor is currently active.

**Returns:** true if input is open, false otherwise

### `uiPatternGroupInputIsCopyMode() → bool`

**Purpose:** Return whether input is copy mode (new name) vs rename mode.

**Returns:** true if copy mode, false if rename mode

### `uiPatternGroupInputGetTrimmedName() → String`

**Purpose:** Return the entered group name without padding or trailing spaces.

**Returns:** Trimmed name (e.g., "DEMO2")

**Note:** Call only after user confirms input. Caller uses this name for actual copy/rename operation.

### `uiPatternGroupInputDraw()`

**Purpose:** Draw the group name input screen.

**Display:**

```
Enter Group Name:
[D][E][M][O][2][_][_][_]
 ^-- cursor here (highlighted)
```

Shows buffer with cursor at current position. Character selector rotates as user changes token index.

### `uiPatternGroupInputRotate(int direction)`

**Purpose:** Move through available characters at current position.

**Parameters:**

- `direction` — +1 for next character, -1 for previous

**Actions:**

1. Advance `currentTokenIndex` by direction
2. Wrap token index at boundaries
3. Flag screen for redraw

**Called by:** `uiManager` on encoder rotate events

### `uiPatternGroupInputAcceptCharacter()`

**Purpose:** Accept the current character and move to next position.

**Actions:**

1. Append current token to buffer
2. Advance cursor position
3. Reset token index to first character
4. Flag screen for redraw
5. If buffer full (max length reached), auto-confirm

**Called by:** `uiManager` on encoder short press

### `uiPatternGroupInputBackspaceOrCancel()`

**Purpose:** Backspace or cancel input.

**Actions (KEY0 press):**

1. If buffer not empty → backspace (remove last char, move cursor left)
2. If buffer empty → cancel input (close editor, discard entry)
3. Flag screen for redraw

**Called by:** `uiManager` on KEY0 short press

---

## Character Validation

**Accepted characters:**

- Uppercase A–Z (recommended for group names)
- Digits 0–9
- Special chars: hyphen, underscore, dot, parentheses
- Space (for multi-word names)

**Rejected:**

- Lowercase (auto-uppercase)
- Symbols /\:|*?"<> (invalid for filesystem)
- Control characters

---

## Dependencies

- `DisplayDriverClass.h` — input screen rendering

---

## Important Implementation Notes

1. **Modal isolation.** While text input is open, global UI state (like long-press settings) must not interfere. uiManager coordinates this.

2. **UTF-8 not supported.** Input accepts ASCII only for filesystem compatibility.

3. **Max length enforcement.** Group names limited to ~20 characters (filesystem path limitations).

4. **Cursor wrapping.** If user presses accept at buffer end, auto-confirm and close input (prevent infinite entry).

5. **Token rotation wraps.** Encoder rotation at A wraps around to last token (Z, 9, _, etc.) to support cycling back.

---

[⬆ UP](developerBuildGuide.md#20-source-file-reference) | [📖 README](../README.md#)
