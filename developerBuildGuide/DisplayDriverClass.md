# `src/DisplayDriverClass.cpp` — ST7789 TFT Display and UI Rendering Engine

**Purpose:** ST7789 TFT display abstraction, text rendering, screen layout engine, color theme management, and boot diagnostics.

---

## Responsibilities

```
1. Initialize ST7789 TFT hardware via dedicated SPI bus
2. Manage display backlight (PWM brightness)
3. Render boot log with color-coded severity levels
4. Draw main sequencer screen with track rows
5. Draw parameter/edit popups and selection overlays
6. Draw system settings menu and card storage menus
7. Manage color theme profiles (light, dark, custom)
8. Provide text rendering with wrapping and alignment
9. Handle partial screen updates (redraw only changed regions)
10. Support theme color switching at runtime
```

---

## Core Data Structures

**DisplayTheme struct:**

```cpp
struct DisplayTheme {
  uint16_t background;
  uint16_t text;
  uint16_t accent;
  uint16_t trackHigh;
  uint16_t trackMid;
  uint16_t trackLow;
  uint16_t selection;
  uint16_t warning;
  uint16_t error;
  // ... RGB565 color palette
};
```

Stores 16-bit RGB565 colors for the current theme.

**DisplayTile struct:**

Layout unit for menu/screen regions. Each tile defines position, size, and cached content for partial redraws.

**BootLogEntry:**

```cpp
struct BootLogEntry {
  String message;
  uint8_t severity;  // INFO, WARNING, ERROR
  uint32_t timestamp;
};
```

Stores boot-time log messages for display and diagnostics.

---

## RGB565 Color Format

Colors are 16-bit packed values:

```
Bit 15–11: Red (5 bits,    0–31)
Bit 10–5:  Green (6 bits,  0–63)
Bit 4–0:   Blue (5 bits,   0–31)
```

Example:
- Pure red:   `0xF800` (11111 000000 00000)
- Pure green: `0x07E0` (00000 111111 00000)
- Pure blue:  `0x001F` (00000 000000 11111)
- White:      `0xFFFF` (11111 111111 11111)
- Black:      `0x0000` (00000 000000 00000)

---

## Key Internal Functions

**buildThemeFromColorProfile(uint8_t profileIndex):**

Loads a color theme by profile index (0=Light, 1=Dark, 2=Custom). Rebuilds `DisplayTheme` struct from `colorSettings.h` definitions.

**blendRgb565(uint16_t fg, uint16_t bg, uint8_t alpha):**

Blends two RGB565 colors with alpha weight (0–255). Used for semi-transparent overlays without pre-rendering buffers.

**drawBootLog():**

Renders accumulated boot messages with auto-scrolling. Each line uses severity-based color (green=info, orange=warning, red=error). Entries persist for ~10 seconds then scroll off.

**drawStatusScreen():**

Full main sequencer screen layout:
- **Header** — version, BPM, swing, group name, WiFi/SD status
- **Six track rows** — sample name, 16-step trigger pattern, velocity/decay/probability indicators
- **Footer** — current parameter page, pattern name, chain info

**drawListScreen(const ListItem[] items, uint8_t selectedIndex, ...):**

Generic menu list renderer. Handles scrolling, selection highlight, cursor wrapping, and optional disabled rows.

**drawSelectionOverlay(const char* title, const char* options[], ...):**

Modal confirmation dialog for destructive actions (delete group, overwrite pattern, etc.). Blocks input until user selects an option.

**drawTempoOverlay(uint16_t bpm, uint8_t swing, ...):**

Tempo and swing edit popup overlay. Partial redraw over the main screen.

**drawFieldInput(const char* label, const char* initialValue, ...):**

Character-by-character text input UI for pattern group names. Character palette rotates via encoder.

**PANEL_COLOR() macro:**

Color correction for panel-specific display issues. Converts logical RGB565 to visual RGB565 if needed.

---

## Public Functions

### `displayInit()`

**Purpose:** Initialize TFT hardware and boot log.

**Actions:**

- Configure SPI pins (TFT_SCLK, TFT_MOSI, TFT_CS, TFT_DC, TFT_RST)
- Create dedicated `SPIClass(TFT_SPI_HOST)`
- Initialize Adafruit_ST7789 driver
- Set rotation and backlight PWM
- Clear screen to background color
- Reset boot log buffer

**Call during:** `setup()`

### `displaySetRotation(uint8_t rot)`

**Purpose:** Set TFT orientation (0–3).

- `0` = portrait, origin top-left
- `1` = landscape, rotated 90° CW
- `2` = portrait, rotated 180°
- `3` = landscape, rotated 270° CW

Rotation is persisted in NVS for next boot.

### `displaySetThemeColorIndex(uint8_t index)`

**Purpose:** Switch color theme at runtime (0=Light, 1=Dark, 2=Custom).

Rebuilds `DisplayTheme` and redraws the active screen region. Theme choice is persisted in NVS.

### `displayAddBootLogEntry(const String& message, uint8_t severity)`

**Purpose:** Append a boot log message.

**Parameters:**

- `message` — log text (typically max 40 chars for TFT width)
- `severity` — `BOOT_LOG_INFO`, `BOOT_LOG_WARNING`, `BOOT_LOG_ERROR`

Used during `setup()` to communicate boot status (SD ready, samples loaded, pin conflicts, etc.).

### `displaySetBacklight(bool enabled)`

**Purpose:** Turn TFT backlight on/off (GPIO2 PWM).

Can be used for power saving or user-triggered blank screen.

### `displayForceStatusScreenRebuild()`

**Purpose:** Invalidate cached status screen so next update does full redraw.

Useful when switching to a different screen mode that needs complete refresh.

### `displayDrawListScreen(const ListItem[] items, uint8_t count, uint8_t selectedIndex, const char* title)`

**Purpose:** Draw a full menu screen.

**Parameters:**

- `items` — list of menu items with labels and optional right-text
- `count` — number of items
- `selectedIndex` — currently highlighted item
- `title` — screen header text

Handles wrapping selection, partial row redraws on highlight change.

### `displayDrawSelectionOverlay(const char* title, const String options[], uint8_t count, uint8_t selectedIndex)`

**Purpose:** Modal selection dialog for "Are you sure?" actions.

Renders title and options, highlights selected option, waits for user confirmation via encod er input.

---

## Boot Log Rendering

The boot log displays on startup with color-coded lines:

```
[INFO]    Firmware v1.3.7 booting...
[INFO]    Display initialized
[WARNING] SD card not found
[INFO]    Using fallback samples
[INFO]    Audio engine ready
[INFO]    Load complete. Press to start.
```

Each entry auto-scrolls off the top after ~10 seconds or when the log fills. Color indicates severity:
- **Green** — INFO (normal operation)
- **Orange** — WARNING (non-critical issue, device can recover)
- **Red** — ERROR (critical failure, may halt)

Developers can read errors at a glance without serial console access.

---

## Partial Screen Updates

To avoid flicker and improve responsiveness, only changed screen regions redraw:

- **Header update** — version, BPM, status change
- **Track update** — one row if triggers/velocity/decay changed
- **Footer update** — chain status, pattern name
- **Parameter page** — full redraw when page changes (acceptable, non-critical path)

Full redraw occurs only on screen switch (Groovebox → Settings, etc.) or explicit `displayForceStatusScreenRebuild()`.

---

## Dependencies

- `Adafruit_ST7789` — TFT driver library
- `Adafruit_GFX` — graphics primitives
- `appConfig.h` — TFT GPIO pins
- `colorSettings.h` — color palette definitions
- `settingsStore.h` — NVS theme/rotation persistence
- SPI (dedicated bus via SPIClass)

---

## Important Implementation Notes

1. **Dedicated SPI bus:** Do not use global `SPI` object. Create a dedicated `SPIClass(TFT_SPI_HOST)` to avoid contention with SD card.

2. **No TFT calls from audioTask():** Display drawing is blocking and must never run on core 0 during audio render. Violations cause audio dropouts.

3. **Backlight PWM:** Uses GPIO2 with PWM timer. Brightness persists in NVS for next boot.

4. **Boot log** appears immediately during `setup()` before SD card is ready, helping developers see early errors (pin conflicts, memory issues, etc.).

5. **Color blending** supports smooth transitions and overlay effects without pre-rendering buffers, saving PSRAM.

6. **Text rendering** wraps at ~40 characters (typical for 320 px width with 8 px font).

7. **Panel color inversion:** Some TFT panels require inverted bit colors for correct visual output. The `PANEL_COLOR()` macro handles this dynamically.

---

[⬆ UP](developerBuildGuide.md#20-source-file-reference) | [📖 README](../README.md#)
