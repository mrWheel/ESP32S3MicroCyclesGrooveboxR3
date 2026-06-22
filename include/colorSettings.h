/*** Last Changed: 2026-06-22 - 16:55 ***/
#ifndef COLOR_SETTINGS_H
#define COLOR_SETTINGS_H

#include <Arduino.h>

//--- Visual text color selection
//--- Visual white maps to ST77XX_BLACK on this inverted panel.
//--- Visual black maps to ST77XX_WHITE on this inverted panel.
enum VisualTextColor
{
  VISUAL_TEXT_WHITE = 0,
  VISUAL_TEXT_BLACK = 1
};

//--- Shade tables: 8 RGB565 levels per color.
//--- These are the generated values from TEST_COLOR_PATERN fade output.
//--- Level mapping is user-friendly: level 0 = shades[0], level 7 = shades[7].
static const uint16_t redShades[] = {0xF800, 0xF882, 0xF904, 0xF9A6,
                                     0xFA28, 0xFACA, 0xFB4C, 0xFBEF};
static const uint16_t greenShades[] = {0x07E0, 0x17E2, 0x27E4, 0x37E6,
                                       0x4FE9, 0x5FEB, 0x6FED, 0x87F0};
static const uint16_t blueShades[] = {0x21BF, 0x2A3F, 0x3ADF, 0x437F,
                                      0x541F, 0x5CBF, 0x6D5F, 0x7DFF};
static const uint16_t indigoShades[] = {0x4810, 0x5052, 0x5894, 0x60F6,
                                        0x6938, 0x719A, 0x79DC, 0x8A3F};
static const uint16_t violetShades[] = {0x780F, 0x8031, 0x8873, 0x98B5,
                                        0xA0D8, 0xB11A, 0xB95C, 0xC99F};
static const uint16_t yellowShades[] = {0xFFE0, 0xFFE2, 0xFFE4, 0xFFE6,
                                        0xFFE9, 0xFFEB, 0xFFED, 0xFFF0};
static const uint16_t orangeShades[] = {0xFD20, 0xFD40, 0xFD80, 0xFDC0,
                                        0xFE00, 0xFE40, 0xFE80, 0xFEC0};

//--- Color profile settings for test palette and UI theming rules
struct ColorProfile
{
  const char* colorName;
  const uint16_t* shades; // Pointer to an 8-entry shade table
  uint8_t darkLevel;      // 0..7 level number for dark UI variant
  VisualTextColor darkLabelColor;
  uint8_t lightLevel; // 0..7 level number for light UI variant
  VisualTextColor lightLabelColor;
  bool usedInPalette;

  uint16_t getDarkColor() const
  {
    uint8_t index = (darkLevel > 7U) ? 7U : darkLevel;

    return shades[index];
  }

  uint16_t getLightColor() const
  {
    uint8_t index = (lightLevel > 7U) ? 7U : lightLevel;

    return shades[index];
  }
};

//--- Shared color profiles
//--- Format: {colorName, shadesTable, darkLevel, darkLabelColor, lightLevel, lightLabelColor,
// usedInPalette}
static const ColorProfile colorProfiles[] = {
    {"Red", redShades, 0, VISUAL_TEXT_WHITE, 5, VISUAL_TEXT_WHITE, true},
    {"Green", greenShades, 0, VISUAL_TEXT_BLACK, 4, VISUAL_TEXT_BLACK, true},
    {"Blue", blueShades, 2, VISUAL_TEXT_WHITE, 5, VISUAL_TEXT_WHITE, true},
    {"Indigo", indigoShades, 0, VISUAL_TEXT_WHITE, 5, VISUAL_TEXT_WHITE, true},
    {"Violet", violetShades, 0, VISUAL_TEXT_WHITE, 6, VISUAL_TEXT_WHITE, true},
    {"Yellow", yellowShades, 0, VISUAL_TEXT_BLACK, 4, VISUAL_TEXT_BLACK, true},
    {"Orange", orangeShades, 2, VISUAL_TEXT_WHITE, 4, VISUAL_TEXT_WHITE, false}};

static const int colorProfileCount =
    static_cast<int>(sizeof(colorProfiles) / sizeof(colorProfiles[0]));

#endif
