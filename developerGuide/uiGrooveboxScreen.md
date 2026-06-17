# `src/uiGrooveboxScreen.cpp`

[Back](developerBuildGuide.md)

---

## Purpose

This file builds and draws the main `[Groovebox]` sequencer screen and related edit overlays. It converts `SequencerView` state into compact display strings.

---

## Responsibilities

```text
format track rows
format step trigger text
format footer status
format chain target labels
format edit popup values
draw full Groovebox screen
draw edit popup overlay
partially update footer
```

---

## Important Implementation Notes

- Keep display formatting here, not in sequencer core.
- Text must fit the 320x240 ST7789 at text size 2.
- Main screen redraws should be minimized while playing.

---

## Important Internal Areas

```text
buildTrackStepText()
formatGrooveboxFooter()
buildParameterOverlayLine()
buildEditPopupTitle()
uiGrooveboxScreenDrawFooterUpdate()
pattern slot name/number helpers
```

---

## Public Functions

### `uiGrooveboxScreenBuildTrackRowText(...)`

Builds one row string for a track.

### `uiGrooveboxScreenDraw(...)`

Draws the full main Groovebox screen.

### `uiGrooveboxScreenDrawEditPopupOverlayOnly(...)`

Draws only the edit popup overlay.

### `uiGrooveboxScreenDrawFooterUpdate(...)`

Partially updates the footer when the footer text changes.


---

[UP](developerBuildGuide.md) | [README](../README.md)
