# `src/uiGrooveboxScreen.cpp` — Main Sequencer Screen Rendering

**Purpose:** Build and render the main `[Groovebox]` sequencer screen layout, track rows, parameter pages, Step popups, and footer status. Pure view layer; state comes from sequencer.

---

## Responsibilities

```
1. Format track row text (name, step triggers, page-specific values)
2. Format step trigger pattern visualization
3. Format velocity/probability/decay/pitch per step
4. Format footer status (pattern name, BPM, chain info)
5. Format chain target names
6. Format Step popup title and values
7. Render full Groovebox screen
8. Render Step popup overlay (partial update)
9. Update footer during playback (minimal redraw)
10. Handle text clipping/wrapping for 320×240 display
```

---

## Screen Layout

**Header** (1 row)

```
[V1.3.7] [G: DEMO] [♪ 120] [⛓ 1/8]
```

- Version, group name, BPM, chain info

**Track rows** (6 rows)

```
[KICK  ] [●●●●●●●●●●●●●●●●] [VEL 100] [P 100]
[SNARE ] [●●●●●●●●●●●●●●●●] [VEL 090] [P 100]
[CH    ] [●●○●●○●●●○●●●○●●] [VEL 080] [P 100]
...
```

Shows track name, 16-step pattern, active page value (varies by mode)

**Footer** (1 row)

```
Pat: Groove1 | Chain: Var→Break | Mode: TRIG
```

Currently selected pattern name, chain status, edit mode

---

## Core Functions

**uiGrooveboxScreenBuildTrackRowText(uint8_t trackIndex, const SequencerView& view, const char* pageLabel) → String:**

Builds one track row display string based on the active parameter page:

- `pageLabel` = "TRIG" → show trigger pattern (●○●●○...)
- `pageLabel` = "VEL" → show velocity values
- `pageLabel` = "PITCH" → show pitch offset per step
- `pageLabel` = "DECAY" → show decay per step
- `pageLabel` = "PROB" → show probability per step
- `pageLabel` = "STEP" → show STEP ON/OFF for the selected step
- `pageLabel` = "CHAIN" → show chain target
- `pageLabel` = "MASTER" → show master level

**formatGrooveboxFooter(const SequencerView& view, uint8_t editMode) → String:**

Builds the footer line from pattern name, chain status, and edit mode.

**buildEditPopupRows(const SequencerView& view, uint8_t pageIndex, String lines[], uint8_t maxLines) → uint8_t:**

Builds text for tempo/master-level Step popup. Returns number of visible lines.

**fitTextToWidth(const String& text, uint16_t maxPixels) → String:**

Clips or abbreviates text to fit within pixel width (typically 40 chars at text size 2).

---

## Public Functions

### `uiGrooveboxScreenDraw(const SequencerView& view)`

**Purpose:** Draw the full Groovebox sequencer screen.

**Actions:**

1. Call `display.drawStatusScreen(...)`
2. Pass header, track rows, footer
3. Pass parameters for current page mode
4. Clear any stale overlays

**Called when:**

- Screen first shows
- Mode changes (Groovebox → Settings)
- Major state update (pattern loaded, etc.)

### `uiGrooveboxScreenDrawEditPopupOverlayOnly(const SequencerView& view, uint8_t editMode)`

**Purpose:** Render the Step popup overlay only (tempo, master level).

**Actions:**

1. Build popup title and values
2. Call `display.drawSelectionOverlay(...)`
3. Highlight selected popup line
4. Wait for encoder input to modify and confirm

**Used when:** User presses encoder medium press to edit tempo/master.

### `uiGrooveboxScreenDrawFooterUpdate(const SequencerView& view)`

**Purpose:** Partially update footer without full screen redraw.

**Actions:**

1. Rebuild footer text
2. Call `display.drawListLine(...)` for footer row only
3. Minimal screen redraw

**Called frequently during playback** to show:

- BPM changes
- Pattern name updates
- Chain status changes

---

## Text Formatting Details

**Step trigger pattern:**

- `x` = active trigger
- `m` = muted step trigger (STEP OFF)
- `-` = no trigger
- 16 symbols total per track

**Velocity/Probability per step:**

Displayed as two-digit numbers (00–100) or abbreviated values.

**Track names:**

Fixed strings: KICK, SNARE, CH, OH, TONE, METAL.

**Parameter page labels:**

- TRIG — trigger editing
- VEL — velocity
- PITCH — pitch transposition
- DECAY — decay time
- PROB — filter/probability
- STEP — per-step ON/OFF; STEP OFF is displayed as lowercase `m`
- CHAIN — chain settings
- MASTER — master output gain

---

## Display Constants

- **Screen width:** 320 pixels
- **Screen height:** 240 pixels
- **Text size:** 2 (8×16 px per character)
- **Max chars per row:** ~40 at text size 2
- **Track rows:** 6
- **Total usable rows:** ~14–15 at text size 2

---

## Dependencies

- `DisplayDriverClass.h` — screen drawing
- `sequencer.h` — SequencerView struct

---

## Important Implementation Notes

1. **Keep formatting here, not in sequencer.** Sequencer generates data; UI formats for display.

2. **Text must fit the 320×240 screen.** Abbreviate long names if necessary.

3. **Partial update for footer.** During playback, avoid full redraws; update only footer row if BPM or pattern name changes.

4. **Parameter page context.** The displayed value depends on the active parameter page. Same track row shows different data depending on `TRIG` vs `VEL` vs `DECAY`.

5. **Chain target display.** If chaining is active, the track row or footer shows the next pattern name or index instead of normal values.

---

[⬆ UP](developerBuildGuide.md#20-source-file-reference) | [📖 README](../README.md#)
