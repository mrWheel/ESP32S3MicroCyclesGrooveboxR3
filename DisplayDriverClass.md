# DisplayDriver Class

## Purpose

`DisplayDriverClass` is the reusable hardware-facing display layer for the current ST7789-based 2.4 inch SPI TFT panel.
It is designed to be copied into other projects that use the same display hardware, while keeping application-specific logic outside the class.

The class focuses on:

- display startup and rotation
- theme application
- backlight control
- reusable layout primitives
- generic tiles and buttons
- common screen helpers such as headers, centered text, lists, messages, and editor screens

## What Changed (Current Code)

The current class implementation includes the following recent updates:

- Added `drawListLine(int lineIndex, const String& text, bool selected)`.
  - This enables partial updates on active list screens without redrawing the full display.
  - It is used by UI code to keep dynamic row updates stable and low-flicker.
- List drawing style is now standardized as:
  - selected row: `>item<`
  - non-selected row: one leading space before item text
- The public class API in [include/DisplayDriverClass.h](include/DisplayDriverClass.h) is now the authoritative source.
  - This document reflects that API and should be updated together with header changes.

The status screen is also supported, but only through a precomputed view-model.
The timer application is responsible for assembling status data before calling the driver.

The current firmware still keeps a compatibility layer of free functions such as `displayInit()` and `displayDrawListScreen()`.
Those functions now delegate to a global `DisplayDriver display` instance so existing code keeps working.

## Hardware Assumptions

This class targets the hardware used by this repository:

- ST7789 controller
- SPI connection
- 320x240 visible layout used by the current panel configuration
- inverted panel color behavior
- LED backlight controlled by `PIN_TFT_BLK`

If you reuse the class in another project, keep the same hardware assumptions or adapt the pin and dimension values at initialization time.

## Design Notes

The class is intentionally split into two layers:

1. Generic hardware/display primitives
2. Higher-level screen helpers that are still usable outside this project

The public API does not require `AppSettings` or `RuntimeStatus`.
That is important because those types are application-specific, while the display driver should remain reusable.

Application logic should build the values to display and pass them into the driver.

## Theme Model

The driver uses `DisplayTheme` for the active UI palette.

`DisplayTheme` contains:

- `selectedFillColor`
- `selectedBorderColor`
- `selectedTextColor`
- `inactiveFillColor`
- `inactiveBorderColor`
- `inactiveTextColor`
- `accentColor`

The class does not generate themes on its own.
The caller is expected to build the theme and pass it through `setTheme()`.

For this project, the compatibility layer builds the theme from the configured color profile.

## Tile Model

`DisplayTile` is a small generic data structure for tile-like content.

Fields:

- `name` - identifier for lookup
- `row` - tile row in the current grid
- `column` - tile column in the current grid
- `size` - horizontal span in grid cells
- `text` - text shown inside the tile
- `align` - left, center, or right alignment
- `fillColor` - optional override fill color
- `borderColor` - optional override border color
- `textColor` - optional override text color

The class keeps a small tile registry internally so tiles can be added and updated by index or name.

## API Reference

### Initialization

#### `init(uint16_t width, uint16_t height, int rotation)`

Initializes the display hardware, SPI bus, backlight, and TFT text configuration.

Typical usage:

```cpp
display.init(TFT_WIDTH, TFT_HEIGHT, settingsStoreLoadDisplayRotation());
```

Notes:

- the rotation is validated internally
- the display is cleared during initialization
- text wrap is disabled and the default bitmap font is selected

### Rotation

#### `setRotation(int rotation)`

Sets the display rotation.

Valid values in the current project are `1` and `3`.

#### `getRotation()`

Returns the active rotation value.

### Theme

#### `setTheme(const DisplayTheme& theme)`

Applies the active UI theme colors.

#### `getTheme() const`

Returns the currently active theme.

### Backlight

#### `setBacklight(bool enabled)`

Turns the panel backlight on or off.

### Geometry and Screen Control

#### `setTileGrid(int originX, int originY, int cellWidth, int cellHeight, int gap)`

Configures the generic tile grid used by `addTile()` and `updateTile()`.

This is useful when you want a project-specific tile layout without hard-coding positions into the class.

#### `clearScreen(uint16_t color)`

Fills the entire screen with one color.

#### `drawHeader(const char* title, const char* rightText = nullptr)`

Draws the standard header bar.

The caller is responsible for supplying the right-side text. The driver does not fetch WiFi, time, or any other application state.

#### `drawCenteredLine(const std::string& lineText, int y, uint16_t textColor, uint16_t backgroundColor)`

Draws one centered line of text.

### Generic Tiles

#### `addTile(const char* name, int row, int column, int size, const std::string& text, DisplayTextAlign align = DisplayTextAlign::Left)`

Creates a generic tile entry and draws it immediately.

Returns the tile index, or `-1` on failure.

#### `updateTile(int tileIndex, const std::string& text)`

Updates the text of an existing tile by index.

#### `updateTile(const char* name, const std::string& text)`

Updates the text of an existing tile by name.

#### `clearTiles()`

Clears the internal tile registry.

### Generic Screen Helpers

#### `drawStatusScreen(const DisplayStatusScreenData& data)`

Draws the main timer status screen from precomputed data.

The `DisplayStatusScreenData` structure contains the fully formatted values for the header, state/profile row, timer tiles, output/cycle rows, and action-row flags.
This keeps application-specific timer logic outside the reusable display layer.
Use `headerRightText` for the caller-computed header text and `actionRowMessage` for the precomputed 24h action-row label.

#### `drawListScreen(...)`

Draws a scrollable list with a highlighted selection row.

#### `drawListLine(int lineIndex, const String& text, bool selected)`

Redraws exactly one list row using the same visual style as `drawListScreen()`.
Useful for partial redraw paths where only one dynamic row changes.

#### `drawListScreenWithDisabledItems(...)`

Draws a list screen where some items can be visually disabled.

#### `refreshHeaderIfNeeded(const char* rightText, uint32_t minimumIntervalMs)`

Refreshes the header if the supplied right-side text changed or if the minimum refresh interval has elapsed.

The driver only compares and redraws the provided text. It does not compute the text itself.

#### `drawNumberEditor(...)`

Draws a simple numeric editor screen.

#### `drawEnumEditor(...)`

Draws a simple enum/value editor screen.

#### `drawTextInput(...)`

Draws a text entry screen with a current token.

#### `drawFieldInput(...)`

Draws a generic field editor with token navigation and optional button-mode selection.

#### `drawMessage(...)`

Draws a simple message screen.

#### `drawWifiPortalScreen(...)`

Draws the centered WiFi portal information screen.

#### `drawStartupConnectionScreen(...)`

Draws the startup connection screen.

#### `drawTestColorPattern()`

Draws the palette test pattern using the configured color profiles.

#### `drawTestColorPalette(int selectedIndex)`

Draws the palette bars and cursor marker for the selected palette entry.

#### `drawTestColorFade(...)`

Draws the color fade test for one selected color profile.

#### `drawButton(int x, int y, int width, int height, const char* label, bool selected)`

Draws a reusable button with the same selected/inactive convention used throughout the project.

## Color Rules

This hardware uses an inverted ST7789 panel.
That means the display class and any caller must follow the same color rules:

- use `PANEL_COLOR()` for fills and colored shapes
- use `ST77XX_BLACK` for readable text on this inverted panel
- do not assume `ST77XX_WHITE` is visible as white

The project also uses one fixed button convention:

- selected button: dark fill, selected border, readable text, inner border drawn with `ST77XX_BLACK`
- inactive button: light fill, inactive border, readable text

Reuse `drawButton()` whenever you need a button that should match the rest of the UI.

## Example

```cpp
#include "DisplayDriverClass.h"

void setup()
{
  DisplayTheme theme;
  theme.selectedFillColor = PANEL_COLOR(0x001F);
  theme.selectedBorderColor = PANEL_COLOR(0x7DFF);
  theme.selectedTextColor = ST77XX_BLACK;
  theme.inactiveFillColor = PANEL_COLOR(0xD69A);
  theme.inactiveBorderColor = PANEL_COLOR(0xBDF7);
  theme.inactiveTextColor = ST77XX_BLACK;
  theme.accentColor = PANEL_COLOR(0xD69A);

  display.init(TFT_WIDTH, TFT_HEIGHT, 3);
  display.setTheme(theme);
  display.setTileGrid(4, 28, 72, 48, 4);
  display.drawHeader("Demo");
  display.addTile("state", 0, 0, 2, "READY", DisplayTextAlign::Center);
  display.drawButton(10, 200, 92, 30, "Start", true);
}
```

## Compatibility Layer

The following legacy free functions remain available in this repository for existing callers:

- `displayInit()`
- `displaySetRotation()`
- `displayGetRotation()`
- `displaySetThemeColorIndex()`
- `displayGetThemeColorIndex()`
- `displayDrawStatusScreen()`
- `displayForceStatusScreenRebuild()`
- `displayDrawListScreen()`
- `displayDrawListScreenWithDisabledItems()`
- `displayRefreshHeaderIfNeeded(const char* rightText, uint32_t minimumIntervalMs)`
- `displayDrawNumberEditor()`
- `displayDrawEnumEditor()`
- `displayDrawTextInput()`
- `displayDraw24hTimerEditor()`
- `displayDrawFieldInput()`
- `displayDrawMessage()`
- `displayDrawWifiPortalScreen()`
- `displayDrawStartupConnectionScreen()`
- `displayDrawTestColorPattern()`
- `displayDrawTestColorPalette()`
- `displayDrawTestColorFade()`
- `displaySetBacklight()`

These wrappers exist so the current firmware can keep working while the reusable class grows.

## Reuse Guidance

When copying this class into another project:

1. Keep the same panel type and inversion behavior, or adapt the color helpers.
2. Call `init()` with the real hardware dimensions and rotation.
3. Provide your own `DisplayTheme` values.
4. Set a tile grid that matches your layout.
5. Use `drawButton()` for any button that should match the project style.
6. Keep application state outside the class and pass rendered values into it.

If you want the class to become even more portable, the next step is usually to move project-specific color-profile logic fully out of the `.cpp` file and into the caller.