# `src/DisplayDriverClass.cpp`

[Back](developerBuildGuide.md)

---

## Purpose

This file owns all direct ST7789 drawing. It wraps Adafruit GFX/ST7789, applies the project color theme, renders list screens and editors, provides the boot log, and exposes a shared `DisplayDriver display` instance.

---

## Responsibilities

```text
initialize TFT on its dedicated SPI bus
apply rotation and theme
draw themed headers
draw Groovebox/UI list screens
draw popups, text editors, field editors and test screens
provide partial row updates for overlays and boot logs
provide severity-aware boot logging
handle panel color inversion through PANEL_COLOR()
```

---

## Important Implementation Notes

- All hardware TFT access should remain inside this file.
- Use `drawHeader()` for standard screen headers.
- Boot log error/warning colors are intentionally not theme-dependent.
- This panel uses inverted color correction; use `PANEL_COLOR()` for fill colors that represent visual RGB565 values.
- Avoid full-screen redraws when a partial row update is enough.

---

## Important Internal Areas

```text
Dedicated TFT SPI object: tftSpi(TFT_SPI_HOST)
PANEL_COLOR() color correction
drawHeader() shared header renderer
boot log state and severity rows
list screen drawing
selection overlay drawing and partial update
text input partial update
generic tile registry
color palette test helpers
```

---

## Public Functions

### `displayInit()`

Initializes the global display wrapper and applies the active theme.

### `displaySetRotation(int rotation)`

Sets display rotation through the global wrapper.

### `displayGetRotation()`

Returns the active TFT rotation.

### `displaySetThemeColorIndex(int index)`

Applies one of the configured color profiles.

### `displayGetThemeColorIndex()`

Returns the active color profile index.

### `displayForceStatusScreenRebuild()`

Invalidates the cached status screen so the next draw rebuilds everything.

### `displayDrawListScreen(...)`

C-style wrapper for drawing a list screen.

### `displayDrawListScreenWithDisabledItems(...)`

C-style wrapper for drawing a list with disabled rows.

### `displayRefreshHeaderIfNeeded(...)`

Refreshes the header right-text when needed.

### `displayDrawNumberEditor(...)`

Draws the numeric editor screen.

### `displayDrawEnumEditor(...)`

Draws the enum editor screen.

### `displayDrawTextInput(...)`

Draws a text input editor.

### `displayDraw24hTimerEditor(...)`

Draws the 24-hour timer editor.

### `displayDrawFieldInput(...)`

Draws the generic field input screen.

### `displayDrawMessage(...)`

Draws a simple message screen.

### `displayDrawWifiPortalScreen(...)`

Draws WiFi portal information.

### `displayDrawStartupConnectionScreen(...)`

Draws startup WiFi connection information.

### `displayDrawTestColorPattern()`

Draws the default color palette test.

### `displayDrawTestColorPalette(int selectedIndex)`

Draws the dark color palette diagnostic screen.

### `displayDrawTestColorFade(...)`

Draws shade/fade diagnostics for one color.

### `displaySetBacklight(bool enabled)`

Enables or disables the TFT backlight.

### `displayBootLogClear(const char* title)`

Clears the boot log and draws the standard header.

### `displayBootLogInfo(const String& line)`

Appends a normal boot log row.

### `displayBootLogWarning(const String& line)`

Appends a highlighted warning row and pauses.

### `displayBootLogError(const String& line)`

Appends a highlighted error row and pauses.

### `DisplayDriver::init(...)`

Initializes the ST7789 hardware, dedicated SPI bus, rotation and screen state.

### `DisplayDriver::drawHeader(...)`

Draws the standard themed header.

### `DisplayDriver::drawListScreen(...)`

Draws a full list screen with optional right text.

### `DisplayDriver::drawListLine(...)`

Partially redraws one list row.

### `DisplayDriver::drawSelectionOverlay(...)`

Draws a modal selection overlay.

### `DisplayDriver::updateSelectionOverlayRow(...)`

Partially updates a row inside a selection overlay.

### `DisplayDriver::drawButton(...)`

Draws one themed button.


---

[UP](developerBuildGuide.md) | [README](../README.md)
