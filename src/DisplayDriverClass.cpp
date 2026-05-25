/*** Last Changed: 2026-05-25 - 18:06 ***/
#include "DisplayDriverClass.h"
#include "appConfig.h"
#include "colorSettings.h"
#include "timerEngine.h"
#include "warpMachine.h"
#include "settingsStore.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <WiFi.h>
#include <array>
#include <esp_log.h>
#include <string>
#include <string.h>
#include <time.h>

//--- Panel color correction
//--- Based on hardware test pattern results, this panel maps colors inverted.
#define PANEL_COLOR(colorValue) static_cast<uint16_t>((colorValue) ^ 0xFFFFU)

//--- TFT instance
static Adafruit_ST7789 tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

//--- Public display instance
DisplayDriver display;

//--- Active UI color index (0-based index into colorProfiles[])
static int activeUiColorIndex = 2;

//--- Active generic display theme
static DisplayTheme activeTheme;

//--- Active TFT rotation (valid values: 1 or 3)
static int activeDisplayRotation = DEFAULT_DISPLAY_ROTATION;

//--- Generic tile registry for the class API
static constexpr int maxTileCount = 24;
static std::array<DisplayTile, maxTileCount> tileRegistry;
static int tileCount = 0;
static int tileGridOriginX = 4;
static int tileGridOriginY = 28;
static int tileGridCellWidth = 72;
static int tileGridCellHeight = 48;
static int tileGridGap = 4;

//--- Cached screen geometry
static uint16_t displayWidth = TFT_WIDTH;
static uint16_t displayHeight = TFT_HEIGHT;

//--- Status screen tile count (4 data tiles + 1 action row)
static const int statusLineCount = 6;

//--- Minimum phase duration to show countdown in status output
static const uint32_t minimumCountdownDisplayMs = 2000UL;

//--- Status screen cache
static bool statusScreenPrepared = false;
static std::array<std::string, statusLineCount> cachedStatusLines;
static uint32_t lastHeaderRefreshMs = 0;
static std::string activeHeaderTitle = "";
static std::string lastHeaderRightText = "";

//--- Draw a status tile
static void drawStatusTile(int x, int y, int w, int h, const char* label, const std::string& value);

//--- Draw status action row (buttons or external trigger label)
static void drawStatusActionButtons(int statusActionIndex, bool externalTriggerMode, bool hideActionsFor24h, const std::string& hideActionsMessage);

//--- Invalidate status screen cache
static void invalidateStatusScreenCache();

//--- Draw screen header
static void drawHeader(const char* title, const char* rightText = nullptr);

//--- Draw centered single line text
static void drawCenteredLine(const std::string& lineText, int y, uint16_t textColor, uint16_t backgroundColor);

//--- Draw a generic status tile
static void drawStatusTile(int x, int y, int w, int h, const char* label, const std::string& value);

//--- Draw status action row (buttons or external trigger label)
static void drawStatusActionButtons(int statusActionIndex, bool externalTriggerMode, bool hideActionsFor24h, const std::string& hideActionsMessage);

//--- Blend two RGB565 colors by 8-bit factor
static uint16_t blendRgb565(uint16_t darkColor, uint16_t lightColor, uint8_t blendFactor);

//--- Resolve visual text color to panel text value
static uint16_t mapVisualTextColorToPanelColor(VisualTextColor visualTextColor);

//--- Build a generic theme from a color profile
static DisplayTheme buildThemeFromColorProfile(const ColorProfile& profile);

//--- Apply a color profile theme by index
static void applyThemeFromColorProfileIndex(int index);

//--- Draw a generic tile from the registry
static void drawGenericTile(const DisplayTile& tile);

//--- Find tile index by name
static int findTileIndexByName(const char* name);

//--- Get selected fill color for UI elements
static uint16_t getUiSelectedFillColor();

//--- Get inactive fill color for UI elements
static uint16_t getUiInactiveFillColor();

//--- Get selected text color for UI elements
static uint16_t getUiSelectedTextColor();

//--- Get inactive text color for UI elements
static uint16_t getUiInactiveTextColor();

//--- Get selected border color for UI elements
static uint16_t getUiSelectedBorderColor();

//--- Get inactive border color for UI elements
static uint16_t getUiInactiveBorderColor();

//--- Get accent color for UI elements
static uint16_t getUiAccentColor();

//--- Initialize display
void displayInit()
{
  display.init(TFT_WIDTH, TFT_HEIGHT, static_cast<int>(settingsStoreLoadDisplayRotation()));
  applyThemeFromColorProfileIndex(activeUiColorIndex);
  display.setTheme(activeTheme);

} //   displayInit()

//--- Set active TFT rotation (valid values: 1 or 3)
void displaySetRotation(int rotation)
{
  display.setRotation(rotation);

} //   displaySetRotation()

//--- Get active TFT rotation (1 or 3)
int displayGetRotation()
{
  return activeDisplayRotation;

} //   displayGetRotation()

//--- Set active UI theme color index
void displaySetThemeColorIndex(int index)
{
  if (index >= 0 && index < colorProfileCount)
  {
    activeUiColorIndex = index;
    applyThemeFromColorProfileIndex(index);
    display.setTheme(activeTheme);
  }

} //   displaySetThemeColorIndex()

//--- Get active UI theme color index
int displayGetThemeColorIndex()
{
  return activeUiColorIndex;

} //   displayGetThemeColorIndex()

//--- Draw status screen from precomputed view model
void DisplayDriver::drawStatusScreen(const DisplayStatusScreenData& data)
{
  const int tileGap = 3;
  const int tileH = 36;
  const int tileStartY = 27;
  const int col1X = 3;
  const int col1W = 155;
  const int col2X = 161;
  const int col2W = 156;
  const int fullW = 314;
  std::array<std::string, statusLineCount> nextStatusLines;
  std::string profileValue = data.profileValue.empty() ? std::string("-") : data.profileValue;
  int minimumProfileX;
  int profileX;

  if (!statusScreenPrepared)
  {
    tft.fillScreen(ST77XX_BLACK);
    drawHeader(data.title.c_str(), data.headerRightText.empty() ? nullptr : data.headerRightText.c_str());

    for (int lineIndex = 0; lineIndex < statusLineCount; lineIndex++)
    {
      cachedStatusLines[lineIndex] = "";
    }

    statusScreenPrepared = true;
  }

  nextStatusLines[0] = data.stateValue + "|" + profileValue;
  nextStatusLines[1] = data.leftTimeTileLabel + "|" + data.leftTimeTileValue;
  nextStatusLines[2] = data.rightTimeTileLabel + "|" + data.rightTimeTileValue;
  nextStatusLines[3] = data.centerTileLabel + "|" + data.centerTileValue;
  nextStatusLines[4] = data.bottomTileLabel + "|" + data.bottomTileValue;
  nextStatusLines[5] = "A:" + std::to_string(data.statusActionIndex) + "|E:" + std::to_string(data.externalTriggerMode ? 1 : 0) + "|Y:" + std::to_string(data.is24hTimer ? 1 : 0) + "|W:" + std::to_string(data.warpSpeedEnabled ? 1 : 0) + "|H:" + std::to_string(data.hideActionsFor24h ? 1 : 0);

  for (int lineIndex = 0; lineIndex < statusLineCount; lineIndex++)
  {
    if (cachedStatusLines[lineIndex] == nextStatusLines[lineIndex])
    {
      continue;
    }

    switch (lineIndex)
    {
    case 0:
    {
      int16_t textX1;
      int16_t textY1;
      uint16_t textWidth;
      uint16_t textHeight;
      uint16_t stateTextWidth;
      uint16_t stateTextHeight;

      drawStatusTile(col1X, tileStartY, fullW, tileH, "STATE", data.stateValue);

      tft.setTextSize(2);
      tft.getTextBounds(data.stateValue.c_str(), 0, 0, &textX1, &textY1, &stateTextWidth, &stateTextHeight);
      tft.getTextBounds(profileValue.c_str(), 0, 0, &textX1, &textY1, &textWidth, &textHeight);
      profileX = col1X + fullW - static_cast<int>(textWidth) - 6;
      minimumProfileX = col1X + 4 + static_cast<int>(stateTextWidth) + 12;

      if (profileX < minimumProfileX)
      {
        profileX = minimumProfileX;
      }

      tft.setTextColor(getUiSelectedTextColor(), getUiSelectedFillColor());
      tft.setCursor(profileX, tileStartY + 14);
      tft.print(profileValue.c_str());
      break;
    }

    case 1:
      if (data.is24hTimer)
      {
        drawStatusTile(col1X, tileStartY + 2 * (tileH + tileGap), col1W, tileH, data.leftTimeTileLabel.c_str(), data.leftTimeTileValue);
      }
      else
      {
        drawStatusTile(col1X, tileStartY + tileH + tileGap, col1W, tileH, data.leftTimeTileLabel.c_str(), data.leftTimeTileValue);
      }
      break;

    case 2:
      if (data.is24hTimer)
      {
        drawStatusTile(col2X, tileStartY + 2 * (tileH + tileGap), col2W, tileH, data.rightTimeTileLabel.c_str(), data.rightTimeTileValue);
      }
      else
      {
        drawStatusTile(col2X, tileStartY + tileH + tileGap, col2W, tileH, data.rightTimeTileLabel.c_str(), data.rightTimeTileValue);
      }
      break;

    case 3:
      if (data.is24hTimer)
      {
        drawStatusTile(col1X, tileStartY + tileH + tileGap, fullW, tileH, data.centerTileLabel.c_str(), data.centerTileValue);
      }
      else
      {
        drawStatusTile(col1X, tileStartY + 2 * (tileH + tileGap), fullW, tileH, data.centerTileLabel.c_str(), data.centerTileValue);
      }
      break;

    case 4:
      if (data.is24hTimer)
      {
        drawStatusTile(col1X, tileStartY + 3 * (tileH + tileGap), fullW, tileH, data.bottomTileLabel.c_str(), data.bottomTileValue);
      }
      else
      {
        drawStatusTile(col1X, tileStartY + 3 * (tileH + tileGap), fullW, tileH, data.bottomTileLabel.c_str(), data.bottomTileValue);

        if (data.warpSpeedEnabled)
        {
          int16_t textX1;
          int16_t textY1;
          uint16_t textWidth;
          uint16_t textHeight;
          int warpModeX;
          int tileY = tileStartY + 3 * (tileH + tileGap);
          const char* warpLabel = "Warp Mode";

          tft.setTextSize(2);
          tft.getTextBounds(warpLabel, 0, 0, &textX1, &textY1, &textWidth, &textHeight);

          warpModeX = col1X + fullW - 4 - static_cast<int>(textWidth);
          if (warpModeX < col1X + 4)
          {
            warpModeX = col1X + 4;
          }

          tft.setTextColor(getUiSelectedTextColor(), getUiSelectedFillColor());
          tft.setCursor(warpModeX, tileY + 14);
          tft.print(warpLabel);
        }
      }
      break;

    case 5:
      drawStatusActionButtons(data.statusActionIndex, data.externalTriggerMode, data.hideActionsFor24h, data.actionRowMessage);
      break;
    }

    cachedStatusLines[lineIndex] = nextStatusLines[lineIndex];
  }

} //   DisplayDriver::drawStatusScreen()

//--- Force next Timer Screen draw to rebuild from scratch
void displayForceStatusScreenRebuild()
{
  invalidateStatusScreenCache();

} //   displayForceStatusScreenRebuild()

//--- Draw text list
void displayDrawListScreen(const char* title, const String items[], size_t itemCount, int selectedIndex, int firstVisibleIndex)
{
  invalidateStatusScreenCache();
  tft.fillScreen(ST77XX_BLACK);
  drawHeader(title);

  tft.setTextSize(2);

  const int visibleLines = 9;

  for (int line = 0; line < visibleLines; line++)
  {
    int itemIndex = firstVisibleIndex + line;

    if (itemIndex >= static_cast<int>(itemCount))
    {
      break;
    }

    int y = 30 + (line * 22);

    if (itemIndex == selectedIndex)
    {
      uint16_t selectedFillColor = getUiSelectedFillColor();

      tft.fillRect(0, y, tft.width(), 22, selectedFillColor);
      tft.setTextColor(getUiSelectedTextColor(), selectedFillColor);
      tft.setCursor(6, y);
      tft.print(">");
      tft.print(items[itemIndex]);
      tft.print("<");
    }
    else
    {
      tft.fillRect(0, y, tft.width(), 22, ST77XX_BLACK);
      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.setCursor(6, y);
      tft.print(" ");
      tft.print(items[itemIndex]);
    }
  }

} //   displayDrawListScreen()

//--- Draw text list with disabled items support
void displayDrawListScreenWithDisabledItems(const char* title, const String items[], size_t itemCount, int selectedIndex, int firstVisibleIndex, const bool disabledItems[])
{
  invalidateStatusScreenCache();
  tft.fillScreen(ST77XX_BLACK);
  drawHeader(title);

  tft.setTextSize(2);

  const int visibleLines = 9;

  for (int line = 0; line < visibleLines; line++)
  {
    int itemIndex = firstVisibleIndex + line;

    if (itemIndex >= static_cast<int>(itemCount))
    {
      break;
    }

    int y = 30 + (line * 22);
    bool isDisabled = disabledItems && disabledItems[itemIndex];

    if (itemIndex == selectedIndex)
    {
      uint16_t selectedFillColor = getUiSelectedFillColor();

      tft.fillRect(0, y, tft.width(), 22, selectedFillColor);
      tft.setTextColor(getUiSelectedTextColor(), selectedFillColor);
      tft.setCursor(6, y);
      tft.print(">");
      tft.print(items[itemIndex]);
      tft.print("<");
    }
    else
    {
      tft.fillRect(0, y, tft.width(), 22, ST77XX_BLACK);

      if (isDisabled)
      {
        //--- Grayed out text for disabled items
        tft.setTextColor(0x4208, ST77XX_BLACK); // Darker gray
      }
      else
      {
        tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      }

      tft.setCursor(6, y);
      tft.print(" ");
      tft.print(items[itemIndex]);
    }
  }

} //   displayDrawListScreenWithDisabledItems()

//--- Refresh current header line when the supplied header text changes
void displayRefreshHeaderIfNeeded(const char* rightText, uint32_t minimumIntervalMs)
{
  uint32_t nowMs = millis();
  std::string rightTextValue = rightText != nullptr ? rightText : "";
  bool headerTitleAvailable = !activeHeaderTitle.empty();
  bool rightTextChanged = (rightTextValue != lastHeaderRightText);

  if (!headerTitleAvailable)
  {
    return;
  }

  if (rightTextChanged)
  {
    drawHeader(activeHeaderTitle.c_str(), rightTextValue.c_str());

    return;
  }

  if (nowMs - lastHeaderRefreshMs >= minimumIntervalMs)
  {
    drawHeader(activeHeaderTitle.c_str(), rightTextValue.c_str());
  }

} //   displayRefreshHeaderIfNeeded()

//--- Draw numeric editor
void displayDrawNumberEditor(const char* label, uint32_t value, const char* unitLabel)
{
  invalidateStatusScreenCache();
  tft.fillScreen(ST77XX_BLACK);
  drawHeader("Edit Value");

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);

  tft.setCursor(6, 60);
  tft.print(label);

  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(4);
  tft.setCursor(6, 105);
  tft.printf("%lu", static_cast<unsigned long>(value));

  tft.setTextColor(getUiAccentColor());
  tft.setTextSize(2);
  tft.setCursor(6, 170);
  tft.print(unitLabel);

  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(6, 205);
  tft.print("Press=OK  K0=Back");

} //   displayDrawNumberEditor()

//--- Draw enum editor
void displayDrawEnumEditor(const char* label, const char* valueLabel)
{
  invalidateStatusScreenCache();
  tft.fillScreen(ST77XX_BLACK);
  drawHeader("Edit Option");

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);

  tft.setCursor(6, 60);
  tft.print(label);

  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(4);
  tft.setCursor(6, 110);
  tft.print(valueLabel);

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(6, 205);
  tft.print("Press=OK  K0=Back");

} //   displayDrawEnumEditor()

//--- Draw text input editor
void displayDrawTextInput(const char* title, const std::string& textValue, const std::string& currentToken)
{
  invalidateStatusScreenCache();
  tft.fillScreen(ST77XX_BLACK);
  drawHeader(title);

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);

  tft.setCursor(6, 45);
  tft.print("Name:");

  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(6, 75);
  tft.print(textValue.c_str());

  tft.setTextColor(getUiAccentColor());
  tft.setTextSize(3);
  tft.setCursor(6, 140);
  tft.print("[");
  tft.print(currentToken.c_str());
  tft.print("]");

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(6, 205);
  tft.print("Hold=Save  K0=Back");

} //   displayDrawTextInput()

//--- Draw 24h timer editor
//--- hourIsCursor:         true = <HH>, false = [HH]
//--- typeLabel:            nullptr = hide type section; else show type
//--- typeIsCursor:         true = <X>, false = [X]
//--- showQuarters:         true = show 4 quarter tiles below
//--- quarterCursorSlot:    -1 = no active slot cursor, 0-3 = cursor on this slot
//--- quarterSlotHasCursor: true = slot label is cursor; false = slot is locked, type label may be cursor
//--- quarterTypeCursorLabel: nullptr = no separate type cursor; else override active slot's state label
//--- quarterTypeIsCursor:  true = type label shown as cursor <X>, false as locked [X]
void displayDraw24hTimerEditor(uint8_t hourIndex, bool hourIsCursor, const char* typeLabel, bool typeIsCursor, bool showQuarters, const char* quarterStateLabels[4], int quarterCursorSlot, bool quarterSlotHasCursor, const char* quarterTypeCursorLabel, bool quarterTypeIsCursor)
{
  static const char* const slotNames[4] = {"00-15", "16-30", "31-45", "46-59"};
  const int tileX = 3;
  const int tileW = 314;
  const int topTileY = 32;
  const int topTileH = 78;
  const int quarterAreaY = 120;
  const int buttonWidth = 68;
  const int buttonHeight = 52;
  const int buttonSpacing = 6;
  const int totalButtonWidth = (4 * buttonWidth) + (3 * buttonSpacing);
  const int buttonAreaX = (tft.width() - totalButtonWidth) / 2;
  char labelBuf[24];
  int16_t textX1;
  int16_t textY1;
  uint16_t textW;
  uint16_t textH;
  int centerX;

  invalidateStatusScreenCache();
  tft.fillScreen(ST77XX_BLACK);
  drawHeader("24h Timer");

  //-- Top tile
  tft.fillRoundRect(tileX, topTileY, tileW, topTileH, 5, getUiSelectedFillColor());
  tft.drawRoundRect(tileX, topTileY, tileW, topTileH, 5, getUiSelectedBorderColor());

  //-- Hour label
  tft.setTextColor(getUiSelectedTextColor(), getUiSelectedFillColor());
  tft.setTextSize(2);
  tft.setCursor(tileX + 8, topTileY + 6);
  tft.print("Hour");

  //-- Hour value with cursor or lock brackets
  snprintf(labelBuf, sizeof(labelBuf), hourIsCursor ? ">%02u<" : "[%02u]", static_cast<unsigned>(hourIndex));
  tft.setTextSize(3);
  tft.setCursor(tileX + 12, topTileY + 32);
  tft.print(labelBuf);

  //-- Type section (only when typeLabel is provided)
  if (typeLabel != nullptr)
  {
    tft.setTextColor(getUiSelectedTextColor(), getUiSelectedFillColor());
    tft.setTextSize(2);
    tft.setCursor(tileX + 180, topTileY + 6);
    tft.print("Type");

    snprintf(labelBuf, sizeof(labelBuf), typeIsCursor ? ">%s<" : "[%s]", typeLabel);
    tft.setTextSize(3);
    tft.setCursor(tileX + 180, topTileY + 32);
    tft.print(labelBuf);
  }

  //-- Quarter tiles (visible when showQuarters is true)
  if (showQuarters)
  {
    for (int q = 0; q < 4; q++)
    {
      int buttonX = buttonAreaX + (q * (buttonWidth + buttonSpacing));
      bool isActive = (q == quarterCursorSlot);
      uint16_t buttonFill = isActive ? getUiSelectedFillColor() : getUiInactiveFillColor();
      uint16_t buttonBorder = isActive ? getUiSelectedBorderColor() : getUiInactiveBorderColor();
      uint16_t buttonText = isActive ? getUiSelectedTextColor() : getUiInactiveTextColor();

      tft.fillRoundRect(buttonX, quarterAreaY, buttonWidth, buttonHeight, 5, buttonFill);
      tft.drawRoundRect(buttonX, quarterAreaY, buttonWidth, buttonHeight, 5, buttonBorder);

      //-- Slot name row (textSize 1: 6x8 px per char)
      bool slotHasCursor = isActive && quarterSlotHasCursor;
      snprintf(labelBuf, sizeof(labelBuf), slotHasCursor ? ">%s<" : "[%s]", slotNames[q]);
      tft.setTextSize(1);
      tft.getTextBounds(labelBuf, 0, 0, &textX1, &textY1, &textW, &textH);
      centerX = buttonX + ((buttonWidth - static_cast<int>(textW)) / 2);
      tft.setTextColor(buttonText, buttonFill);
      tft.setCursor(centerX, quarterAreaY + 6);
      tft.print(labelBuf);

      //-- State label row (textSize 2: 12x16 px per char)
      const char* stateLabel;
      bool stateHasCursor = false;

      if (isActive && !quarterSlotHasCursor && quarterTypeCursorLabel != nullptr)
      {
        stateLabel = quarterTypeCursorLabel;
        stateHasCursor = quarterTypeIsCursor;
      }
      else
      {
        stateLabel = quarterStateLabels[q];
      }

      snprintf(labelBuf, sizeof(labelBuf), stateHasCursor ? ">%s<" : "[%s]", stateLabel);
      tft.setTextSize(2);
      tft.getTextBounds(labelBuf, 0, 0, &textX1, &textY1, &textW, &textH);
      centerX = buttonX + ((buttonWidth - static_cast<int>(textW)) / 2);

      if (centerX < buttonX + 2)
      {
        centerX = buttonX + 2;
      }

      tft.setTextColor(buttonText, buttonFill);
      tft.setCursor(centerX, quarterAreaY + 24);
      tft.print(labelBuf);
    }

    //-- Hint rows for quarter modes
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);
    tft.setCursor(6, 184);

    if (quarterCursorSlot < 0)
    {
      tft.print("Turn=Type  Short=Edit quarters");
    }
    else if (quarterSlotHasCursor)
    {
      tft.print("Turn=Quarter  Short=Edit type");
    }
    else
    {
      tft.print("Turn=Type  Short=Set type");
    }

    tft.setCursor(6, 196);
    tft.print("Hold=Save+Back  K0=Back");
  }
  else
  {
    //-- Hint rows for hour/type modes
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);
    tft.setCursor(6, 170);

    if (hourIsCursor)
    {
      tft.print("Turn=Select hour  Short=Lock");
    }
    else
    {
      tft.print("Turn=Select type  Short=Set");
    }

    tft.setCursor(6, 182);
    tft.print("Hold=Save+Back  K0=Back");
  }

} //   displayDraw24hTimerEditor()

//--- Draw generic field input screen
void displayDrawFieldInput(const char* title, const char* fieldName, const std::string& fieldValue, int cursorIndex, const std::string& prevToken, const std::string& currentToken, const std::string& nextToken,
                           const char* tokenOptions[], int tokenOptionCount, int selectedOptionIndex)
{
  //--- Per colorSettings.md: PANEL_COLOR() for fills/borders, ST77XX_BLACK for readable text.
  uint16_t tokenCurrentColor = ST77XX_BLACK;
  uint16_t tokenNeighborColor = getUiInactiveFillColor();
  uint16_t tileFillColor = getUiSelectedFillColor();
  uint16_t tileBorderColor = getUiSelectedBorderColor();
  uint16_t tileValueColor = getUiSelectedTextColor();
  uint16_t tileLabelColor = tileValueColor;

  const int tileX = 3;
  const int tileW = 314;
  bool useButtonOptions = (cursorIndex == 0) && (tokenOptions != nullptr) && (tokenOptionCount >= 2) && (tokenOptionCount <= 6);
  bool useTwoRowButtons = useButtonOptions && (tokenOptionCount > 4);

  //--- Tile 1: field name + value, cursor arrows (higher and roomier)
  const int tile1Y = 30;
  const int tile1H = 84;

  //--- Tile 2: token selector fixed near bottom (only when NOT using button options)
  const int tile2H = !useButtonOptions ? 44 : 0;
  const int tile2Y = !useButtonOptions ? 156 : 0;

  std::string valueText = "[" + fieldValue + "]";
  std::string leftTokenText = prevToken + " < ";
  std::string rightTokenText = " > " + nextToken;
  int16_t textX1;
  int16_t textY1;
  uint16_t textW;
  uint16_t textH;
  uint16_t leftTokenW;
  uint16_t currentTokenW;
  uint16_t rightTokenW;
  int valueX;
  int tokenX;
  int cursorBaseX;
  int currentTokenX;
  int currentTokenY;

  invalidateStatusScreenCache();
  tft.fillScreen(ST77XX_BLACK);
  drawHeader(title);

  //--- Tile 1: value input
  tft.fillRoundRect(tileX, tile1Y, tileW, tile1H, 5, tileFillColor);
  tft.drawRoundRect(tileX, tile1Y, tileW, tile1H, 5, tileBorderColor);

  tft.setTextSize(2);
  tft.setTextColor(tileLabelColor, tileFillColor);
  tft.setCursor(tileX + 6, tile1Y + 5);
  tft.print(fieldName);

  tft.setTextSize(2);
  tft.getTextBounds(valueText.c_str(), 0, 0, &textX1, &textY1, &textW, &textH);
  valueX = tileX + ((tileW - static_cast<int>(textW)) / 2);

  if (valueX < tileX + 4)
  {
    valueX = tileX + 4;
  }

  cursorBaseX = valueX + 12;

  if (cursorIndex >= 0)
  {
    int cursorX = cursorBaseX + (cursorIndex * 12);
    tft.setTextColor(ST77XX_BLACK, tileFillColor);
    tft.setCursor(cursorX, tile1Y + 24);
    tft.print("v");
  }

  tft.setTextColor(tileValueColor, tileFillColor);
  tft.setCursor(valueX, tile1Y + 41);
  tft.print(valueText.c_str());

  if (cursorIndex >= 0)
  {
    int cursorX = cursorBaseX + (cursorIndex * 12);
    tft.setTextColor(ST77XX_BLACK, tileFillColor);
    tft.setCursor(cursorX, tile1Y + 62);
    tft.print("^");
  }

  //--- Tile 2: token selector (only when NOT using button options)
  if (!useButtonOptions)
  {
    tft.fillRoundRect(tileX, tile2Y, tileW, tile2H, 5, tileFillColor);
    tft.drawRoundRect(tileX, tile2Y, tileW, tile2H, 5, tileBorderColor);

    tft.setTextSize(2);
    tft.setTextColor(tileLabelColor, tileFillColor);
    tft.setCursor(tileX + 6, tile2Y + 5);
    tft.print("SELECT");
  }

  if (useButtonOptions)
  {
    const int buttonHeight = 30;
    const int buttonWidth = 92;
    const int buttonSpacing = 10;
    int itemsPerRow = useTwoRowButtons ? 3 : tokenOptionCount;
    int rowCount = useTwoRowButtons ? 2 : 1;

    //--- Calculate total button area dimensions
    int totalButtonWidth = (itemsPerRow * buttonWidth) + ((itemsPerRow - 1) * buttonSpacing);
    int totalButtonHeight = (rowCount * buttonHeight) + ((rowCount - 1) * buttonSpacing);

    //--- Center buttons horizontally on screen
    int buttonAreaX = (tft.width() - totalButtonWidth) / 2;

    //--- Position button area at bottom with padding
    const int bottomPadding = 12;
    int buttonAreaY = tft.height() - totalButtonHeight - bottomPadding;

    //--- Clear entire area for buttons (with margin)
    tft.fillRect(0, buttonAreaY - 10, tft.width(), totalButtonHeight + 20, ST77XX_BLACK);

    tft.setTextSize(2);

    for (int row = 0; row < rowCount; row++)
    {
      for (int col = 0; col < itemsPerRow; col++)
      {
        int optionIndex = row * itemsPerRow + col;

        if (optionIndex >= tokenOptionCount)
        {
          break;
        }

        int buttonX = buttonAreaX + (col * (buttonWidth + buttonSpacing));
        int buttonY = buttonAreaY + (row * (buttonHeight + buttonSpacing));
        bool isSelected = (optionIndex == selectedOptionIndex);
        const char* optionLabel = tokenOptions[optionIndex];
        display.drawButton(buttonX, buttonY, buttonWidth, buttonHeight, optionLabel, isSelected);
      }
    }
  }
  else
  {
    tft.setTextSize(2);
    tft.getTextBounds(leftTokenText.c_str(), 0, 0, &textX1, &textY1, &leftTokenW, &textH);
    tft.getTextBounds(currentToken.c_str(), 0, 0, &textX1, &textY1, &currentTokenW, &textH);
    tft.getTextBounds(rightTokenText.c_str(), 0, 0, &textX1, &textY1, &rightTokenW, &textH);

    textW = leftTokenW + currentTokenW + rightTokenW;
    tokenX = tileX + ((tileW - static_cast<int>(textW)) / 2);

    if (tokenX < tileX + 4)
    {
      tokenX = tileX + 4;
    }

    currentTokenY = tile2Y + 24;
    tft.setCursor(tokenX, currentTokenY);
    tft.setTextColor(tokenNeighborColor, tileFillColor);
    tft.print(leftTokenText.c_str());
    currentTokenX = tokenX + static_cast<int>(leftTokenW);

    tft.setTextColor(tokenCurrentColor, tileFillColor);
    tft.setCursor(currentTokenX, currentTokenY);
    tft.print(currentToken.c_str());
    //--- Pseudo-bold effect for current token on bitmap font.
    tft.setCursor(currentTokenX + 1, currentTokenY);
    tft.print(currentToken.c_str());

    tft.setTextColor(tokenNeighborColor, tileFillColor);
    tft.setCursor(currentTokenX + static_cast<int>(currentTokenW), currentTokenY);
    tft.print(rightTokenText.c_str());
  }

  //--- Position help text
  tft.setTextColor(ST77XX_BLACK);
  tft.setTextSize(2);
  if (useButtonOptions)
  {
    //--- Position help text above buttons
    int rowCount = useTwoRowButtons ? 2 : 1;
    int totalButtonHeight = (rowCount * 30) + ((rowCount - 1) * 10);
    int buttonAreaY = tft.height() - totalButtonHeight - 12;
    int helpTextY = buttonAreaY - 14;
    tft.setCursor(6, helpTextY);
    tft.print("Turn=Select  Short=Save");
  }
  else
  {
    tft.setCursor(6, tile2Y + tile2H + 4);
    tft.print("Short=Next  Turn=Change");
  }

} //   displayDrawFieldInput()

//--- Draw message screen
void displayDrawMessage(const char* title, const char* message)
{
  invalidateStatusScreenCache();
  tft.fillScreen(ST77XX_BLACK);
  drawHeader(title);

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(6, 60);
  tft.print(message);

} //   displayDrawMessage()

//--- Draw centered WiFi portal info screen
void displayDrawWifiPortalScreen(const std::string& line1, const std::string& line2, const std::string& line3, const std::string& line4)
{
  invalidateStatusScreenCache();
  tft.fillScreen(ST77XX_BLACK);
  drawHeader("WiFi Manager Started");

  tft.setTextSize(2);

  drawCenteredLine(line1, 62, ST77XX_WHITE, ST77XX_BLACK);
  drawCenteredLine(line2, 92, ST77XX_GREEN, ST77XX_BLACK);
  drawCenteredLine(line3, 122, ST77XX_GREEN, ST77XX_BLACK);
  drawCenteredLine(line4, 186, getUiAccentColor(), ST77XX_BLACK);

} //   displayDrawWifiPortalScreen()

//--- Draw centered startup connection screen
void displayDrawStartupConnectionScreen(const std::string& line1, const std::string& line2)
{
  invalidateStatusScreenCache();
  tft.fillScreen(ST77XX_BLACK);
  drawHeader("Universal Timer");

  tft.setTextSize(2);
  drawCenteredLine(line1, 86, ST77XX_WHITE, ST77XX_BLACK);
  drawCenteredLine(line2, 118, ST77XX_GREEN, ST77XX_BLACK);

} //   displayDrawStartupConnectionScreen()

//--- Draw color palette test pattern
void displayDrawTestColorPattern()
{
  displayDrawTestColorPalette(0);

} //   displayDrawTestColorPattern()

//--- Draw test palette with dark bars and cursor
void displayDrawTestColorPalette(int selectedIndex)
{
  int barCount = 0;
  const int barY0 = 24;
  int barH;

  for (int profileIndex = 0; profileIndex < colorProfileCount; profileIndex++)
  {
    if (colorProfiles[profileIndex].usedInPalette)
    {
      barCount++;
    }
  }

  if (barCount <= 0)
  {
    return;
  }

  barH = (tft.height() - barY0) / barCount;

  if (selectedIndex < 0)
  {
    selectedIndex = 0;
  }
  else if (selectedIndex >= barCount)
  {
    selectedIndex = barCount - 1;
  }

  invalidateStatusScreenCache();
  tft.fillScreen(ST77XX_BLACK);
  drawHeader("Dark Palette");
  tft.setTextSize(2);

  int drawIndex = 0;

  for (int profileIndex = 0; profileIndex < colorProfileCount; profileIndex++)
  {
    if (!colorProfiles[profileIndex].usedInPalette)
    {
      continue;
    }

    int y = barY0 + (drawIndex * barH);
    int drawH = barH;
    uint16_t panelColor = PANEL_COLOR(colorProfiles[profileIndex].getDarkColor());
    uint16_t labelColor = mapVisualTextColorToPanelColor(colorProfiles[profileIndex].darkLabelColor);

    if (drawIndex == (barCount - 1))
    {
      drawH = tft.height() - y;
    }

    tft.fillRect(0, y, tft.width(), drawH, panelColor);
    tft.setTextColor(labelColor, panelColor);
    tft.setCursor(26, y + ((drawH - 16) / 2));
    tft.print(colorProfiles[profileIndex].colorName);

    if (drawIndex == selectedIndex)
    {
      tft.setTextColor(labelColor, panelColor);
      tft.setCursor(6, y + ((drawH - 16) / 2));
      tft.print(">");
    }

    drawIndex++;
  }

} //   displayDrawTestColorPalette()

//--- Draw color fade test for selected color
void displayDrawTestColorFade(const char* colorName, uint16_t darkColorVisual, uint16_t lightColorVisual, VisualTextColor darkLabelColor, VisualTextColor lightLabelColor)
{
  const int rowCount = 8;
  const int rowY0 = 24;
  const int rowH = (tft.height() - rowY0) / rowCount;
  const int textHeight = 16;
  uint16_t darkLabelPanelColor = mapVisualTextColorToPanelColor(darkLabelColor);
  uint16_t lightLabelPanelColor = mapVisualTextColorToPanelColor(lightLabelColor);
  char rowText[8];
  char hexText[24];

  invalidateStatusScreenCache();
  tft.fillScreen(ST77XX_BLACK);
  drawHeader(colorName);
  tft.setTextSize(2);

  for (int rowIndex = 0; rowIndex < rowCount; rowIndex++)
  {
    int y = rowY0 + (rowIndex * rowH);
    int drawH = rowH;
    uint8_t blendFactor = static_cast<uint8_t>((rowIndex * 255) / (rowCount - 1));
    uint16_t shadeVisual = blendRgb565(darkColorVisual, lightColorVisual, blendFactor);
    uint16_t shadePanel = PANEL_COLOR(shadeVisual);

    if (rowIndex == (rowCount - 1))
    {
      drawH = tft.height() - y;
    }

    tft.fillRect(0, y, tft.width(), drawH, shadePanel);
    snprintf(rowText, sizeof(rowText), "%d", rowIndex);
    snprintf(hexText, sizeof(hexText), "0x%04X", static_cast<unsigned int>(shadeVisual));

    tft.setTextColor(darkLabelPanelColor, shadePanel);
    tft.setCursor(8, y + ((drawH - textHeight) / 2));
    tft.print(rowText);

    tft.setTextColor(lightLabelPanelColor, shadePanel);
    tft.setCursor(44, y + ((drawH - textHeight) / 2));
    tft.print(hexText);
  }

} //   displayDrawTestColorFade()

//--- Set display backlight state
void displaySetBacklight(bool enabled)
{
  display.setBacklight(enabled);

} //   displaySetBacklight()

//--- Draw screen header
static void drawHeader(const char* title, const char* rightText)
{
  uint16_t headerBackgroundColor = getUiSelectedFillColor();
  uint16_t headerTextColor = getUiSelectedTextColor();
  std::string rightTextValue = rightText != nullptr ? rightText : "";
  int16_t textX1;
  int16_t textY1;
  uint16_t titleWidth;
  uint16_t titleHeight;
  uint16_t rightWidth;
  uint16_t rightHeight;
  int titleX = 6;
  int rightX;
  int minimumGap = 10;

  tft.fillRect(0, 0, tft.width(), 24, headerBackgroundColor);
  tft.setTextSize(2);
  tft.setTextColor(headerTextColor, headerBackgroundColor);
  tft.getTextBounds(title, 0, 0, &textX1, &textY1, &titleWidth, &titleHeight);
  tft.getTextBounds(rightTextValue.c_str(), 0, 0, &textX1, &textY1, &rightWidth, &rightHeight);
  (void)titleHeight;
  (void)rightHeight;
  rightX = tft.width() - static_cast<int>(rightWidth) - 6;

  if (rightX < (titleX + static_cast<int>(titleWidth) + minimumGap))
  {
    rightX = titleX + static_cast<int>(titleWidth) + minimumGap;
  }

  tft.setCursor(titleX, 4);
  tft.print(title);
  tft.setCursor(titleX + 1, 4);
  tft.print(title);

  tft.setCursor(rightX, 4);
  tft.print(rightTextValue.c_str());

  activeHeaderTitle = title;
  lastHeaderRightText = rightTextValue;
  lastHeaderRefreshMs = millis();

} //   drawHeader()

//--- Draw centered single line text
static void drawCenteredLine(const std::string& lineText, int y, uint16_t textColor, uint16_t backgroundColor)
{
  int16_t x1;
  int16_t y1;
  uint16_t width;
  uint16_t height;
  int cursorX;

  tft.getTextBounds(lineText.c_str(), 0, 0, &x1, &y1, &width, &height);
  cursorX = (tft.width() - static_cast<int>(width)) / 2;

  if (cursorX < 0)
  {
    cursorX = 0;
  }

  tft.setTextColor(textColor, backgroundColor);
  tft.setCursor(cursorX, y);
  tft.print(lineText.c_str());

} //   drawCenteredLine()

//--- Draw a status tile
static void drawStatusTile(int x, int y, int w, int h, const char* label, const std::string& value)
{
  uint16_t tileFillColor = getUiSelectedFillColor();
  uint16_t tileBorderColor = getUiSelectedBorderColor();
  uint16_t tileValueColor = getUiSelectedTextColor();
  uint16_t tileLabelColor = tileValueColor;

  tft.fillRoundRect(x, y, w, h, 5, tileFillColor);
  tft.drawRoundRect(x, y, w, h, 5, tileBorderColor);

  tft.setTextSize(1);
  tft.setTextColor(tileLabelColor, tileFillColor);
  tft.setCursor(x + 4, y + 3);
  tft.print(label);

  tft.setTextSize(2);
  tft.setTextColor(tileValueColor, tileFillColor);
  tft.setCursor(x + 4, y + 14);
  tft.print(value.c_str());

} //   drawStatusTile()

//--- Draw status action row (buttons or external trigger label)
static void drawStatusActionButtons(int statusActionIndex, bool externalTriggerMode, bool hideActionsFor24h, const std::string& hideActionsMessage)
{
  static const char* labels[] =
      {
          "Start",
          "Stop",
          "Reset"};

  const int buttonY = 194;
  const int buttonHeight = 30;
  const int buttonWidth = 92;
  const int buttonSpacing = 10;
  const int firstButtonX = 11;

  tft.fillRect(0, 182, tft.width(), 58, ST77XX_BLACK);

  if (hideActionsFor24h)
  {
    int16_t textX1;
    int16_t textY1;
    uint16_t textWidth;
    uint16_t textHeight;
    int textX;
    int textY;

    tft.setTextSize(2);
    tft.getTextBounds(hideActionsMessage.c_str(), 0, 0, &textX1, &textY1, &textWidth, &textHeight);
    textX = (tft.width() - static_cast<int>(textWidth)) / 2;
    textY = buttonY + ((buttonHeight - static_cast<int>(textHeight)) / 2);

    if (textX < 0)
    {
      textX = 0;
    }

    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(textX, textY);
    tft.print(hideActionsMessage.c_str());

    return;
  }

  if (externalTriggerMode)
  {
    const char* message = "External Trigger";
    int16_t textX1;
    int16_t textY1;
    uint16_t textWidth;
    uint16_t textHeight;
    int textX;
    int textY;

    tft.setTextSize(2);
    tft.getTextBounds(message, 0, 0, &textX1, &textY1, &textWidth, &textHeight);
    textX = (tft.width() - static_cast<int>(textWidth)) / 2;
    textY = buttonY + ((buttonHeight - static_cast<int>(textHeight)) / 2);

    if (textX < 0)
    {
      textX = 0;
    }

    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(textX, textY);
    tft.print(message);

    return;
  }

  tft.setTextSize(2);

  for (int buttonIndex = 0; buttonIndex < 3; buttonIndex++)
  {
    int buttonX = firstButtonX + (buttonIndex * (buttonWidth + buttonSpacing));
    bool isSelected = (buttonIndex == statusActionIndex);
    display.drawButton(buttonX, buttonY, buttonWidth, buttonHeight, labels[buttonIndex], isSelected);
  }

} //   drawStatusActionButtons()

//--- Invalidate status screen cache
static void invalidateStatusScreenCache()
{
  statusScreenPrepared = false;
  lastHeaderRefreshMs = 0;
  activeHeaderTitle = "";
  lastHeaderRightText = "";

} //   invalidateStatusScreenCache()

//--- Blend two RGB565 colors by 8-bit factor
static uint16_t blendRgb565(uint16_t darkColor, uint16_t lightColor, uint8_t blendFactor)
{
  uint8_t darkR = static_cast<uint8_t>((darkColor >> 11) & 0x1FU);
  uint8_t darkG = static_cast<uint8_t>((darkColor >> 5) & 0x3FU);
  uint8_t darkB = static_cast<uint8_t>(darkColor & 0x1FU);
  uint8_t lightR = static_cast<uint8_t>((lightColor >> 11) & 0x1FU);
  uint8_t lightG = static_cast<uint8_t>((lightColor >> 5) & 0x3FU);
  uint8_t lightB = static_cast<uint8_t>(lightColor & 0x1FU);
  uint16_t mixedR = static_cast<uint16_t>((static_cast<uint16_t>(darkR) * static_cast<uint16_t>(255U - blendFactor) + static_cast<uint16_t>(lightR) * static_cast<uint16_t>(blendFactor)) / 255U);
  uint16_t mixedG = static_cast<uint16_t>((static_cast<uint16_t>(darkG) * static_cast<uint16_t>(255U - blendFactor) + static_cast<uint16_t>(lightG) * static_cast<uint16_t>(blendFactor)) / 255U);
  uint16_t mixedB = static_cast<uint16_t>((static_cast<uint16_t>(darkB) * static_cast<uint16_t>(255U - blendFactor) + static_cast<uint16_t>(lightB) * static_cast<uint16_t>(blendFactor)) / 255U);

  return static_cast<uint16_t>((mixedR << 11) | (mixedG << 5) | mixedB);

} //   blendRgb565()

//--- Get selected fill color for UI elements
static uint16_t getUiSelectedFillColor()
{
  return activeTheme.selectedFillColor;

} //   getUiSelectedFillColor()

//--- Get inactive fill color for UI elements
static uint16_t getUiInactiveFillColor()
{
  return activeTheme.inactiveFillColor;

} //   getUiInactiveFillColor()

//--- Get selected text color for UI elements
static uint16_t getUiSelectedTextColor()
{
  return activeTheme.selectedTextColor;

} //   getUiSelectedTextColor()

//--- Get inactive text color for UI elements
static uint16_t getUiInactiveTextColor()
{
  return activeTheme.inactiveTextColor;

} //   getUiInactiveTextColor()

//--- Get selected border color for UI elements
static uint16_t getUiSelectedBorderColor()
{
  return activeTheme.selectedBorderColor;

} //   getUiSelectedBorderColor()

//--- Get inactive border color for UI elements
static uint16_t getUiInactiveBorderColor()
{
  return activeTheme.inactiveBorderColor;

} //   getUiInactiveBorderColor()

//--- Get accent color for UI elements
static uint16_t getUiAccentColor()
{
  return activeTheme.accentColor;

} //   getUiAccentColor()

//--- Resolve visual text color to panel text value
static uint16_t mapVisualTextColorToPanelColor(VisualTextColor visualTextColor)
{
  if (visualTextColor == VISUAL_TEXT_WHITE)
  {
    return ST77XX_BLACK;
  }

  return ST77XX_WHITE;

} //   mapVisualTextColorToPanelColor()

//--- Build a generic theme from a color profile
static DisplayTheme buildThemeFromColorProfile(const ColorProfile& profile)
{
  DisplayTheme theme;

  theme.selectedFillColor = PANEL_COLOR(profile.getDarkColor());
  theme.selectedBorderColor = PANEL_COLOR(profile.getLightColor());
  theme.selectedTextColor = mapVisualTextColorToPanelColor(profile.darkLabelColor);
  theme.inactiveFillColor = PANEL_COLOR(profile.getLightColor());
  theme.inactiveBorderColor = PANEL_COLOR(blendRgb565(profile.getLightColor(), 0xFFFF, 92));
  theme.inactiveTextColor = mapVisualTextColorToPanelColor(profile.lightLabelColor);
  theme.accentColor = theme.inactiveFillColor;

  return theme;

} //   buildThemeFromColorProfile()

//--- Apply a color profile theme by index
static void applyThemeFromColorProfileIndex(int index)
{
  if (index < 0 || index >= colorProfileCount)
  {
    return;
  }

  activeTheme = buildThemeFromColorProfile(colorProfiles[index]);

} //   applyThemeFromColorProfileIndex()

//--- Initialize display hardware
void DisplayDriver::init(uint16_t width, uint16_t height, int rotation)
{
  displayWidth = width;
  displayHeight = height;

  pinMode(PIN_TFT_BL, OUTPUT);
  digitalWrite(PIN_TFT_BL, HIGH);

  SPI.begin(PIN_TFT_SCL, -1, PIN_TFT_SDA, PIN_TFT_CS);

  activeDisplayRotation = rotation;

  if (activeDisplayRotation != 1 && activeDisplayRotation != 3)
  {
    activeDisplayRotation = DEFAULT_DISPLAY_ROTATION;
  }

  tft.init(displayHeight, displayWidth);
  tft.setRotation(activeDisplayRotation);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);
  tft.setFont(nullptr);
  invalidateStatusScreenCache();

  ESP_LOGI("displayDriver", "Display initialized");

} //   DisplayDriver::init()

//--- Set active TFT rotation (valid values: 1 or 3)
void DisplayDriver::setRotation(int rotation)
{
  int newRotation = rotation;

  if (newRotation != 1 && newRotation != 3)
  {
    newRotation = DEFAULT_DISPLAY_ROTATION;
  }

  if (newRotation == activeDisplayRotation)
  {
    return;
  }

  activeDisplayRotation = newRotation;
  tft.setRotation(activeDisplayRotation);
  tft.fillScreen(ST77XX_BLACK);
  invalidateStatusScreenCache();

} //   DisplayDriver::setRotation()

//--- Get active TFT rotation (1 or 3)
int DisplayDriver::getRotation()
{
  return activeDisplayRotation;

} //   DisplayDriver::getRotation()

//--- Set active display theme
void DisplayDriver::setTheme(const DisplayTheme& theme)
{
  activeTheme = theme;

} //   DisplayDriver::setTheme()

//--- Get active display theme
const DisplayTheme& DisplayDriver::getTheme() const
{
  return activeTheme;

} //   DisplayDriver::getTheme()

//--- Set display backlight state
void DisplayDriver::setBacklight(bool enabled)
{
  digitalWrite(PIN_TFT_BL, enabled ? HIGH : LOW);

} //   DisplayDriver::setBacklight()

//--- Configure tile grid geometry for addTile/updateTile
void DisplayDriver::setTileGrid(int originX, int originY, int cellWidth, int cellHeight, int gap)
{
  tileGridOriginX = originX;
  tileGridOriginY = originY;
  tileGridCellWidth = cellWidth;
  tileGridCellHeight = cellHeight;
  tileGridGap = gap;

} //   DisplayDriver::setTileGrid()

//--- Clear the screen
void DisplayDriver::clearScreen(uint16_t color)
{
  tft.fillScreen(color);

} //   DisplayDriver::clearScreen()

//--- Draw a screen header
void DisplayDriver::drawHeader(const char* title, const char* rightText)
{
  ::drawHeader(title, rightText);

} //   DisplayDriver::drawHeader()

//--- Draw centered single line text
void DisplayDriver::drawCenteredLine(const std::string& lineText, int y, uint16_t textColor, uint16_t backgroundColor)
{
  ::drawCenteredLine(lineText, y, textColor, backgroundColor);

} //   DisplayDriver::drawCenteredLine()

//--- Draw a generic button
void DisplayDriver::drawButton(int x, int y, int width, int height, const char* label, bool selected)
{
  int16_t textX1;
  int16_t textY1;
  uint16_t textWidth;
  uint16_t textHeight;
  int textX;
  int textY;
  uint16_t fillColor = selected ? getUiSelectedFillColor() : getUiInactiveFillColor();
  uint16_t borderColor = selected ? getUiSelectedBorderColor() : getUiInactiveBorderColor();
  uint16_t textColor = selected ? getUiSelectedTextColor() : getUiInactiveTextColor();

  tft.fillRoundRect(x, y, width, height, 8, fillColor);
  tft.drawRoundRect(x, y, width, height, 8, borderColor);

  if (selected)
  {
    tft.drawRoundRect(x + 1, y + 1, width - 2, height - 2, 8, ST77XX_BLACK);
  }

  tft.setTextSize(2);
  tft.getTextBounds(label, 0, 0, &textX1, &textY1, &textWidth, &textHeight);
  textX = x + ((width - static_cast<int>(textWidth)) / 2);
  textY = y + ((height - static_cast<int>(textHeight)) / 2);

  tft.setTextColor(textColor, fillColor);
  tft.setCursor(textX, textY);
  tft.print(label);

} //   DisplayDriver::drawButton()

//--- Draw inline tempo overlay card on top of the sequencer screen
void DisplayDriver::drawTempoOverlay(uint16_t bpm, uint8_t swingPercent, bool bpmSelected)
{
  const int shadowX = 28;
  const int shadowY = 64;
  const int shadowW = 264;
  const int shadowH = 118;
  const int panelX = 36;
  const int panelY = 72;
  const int panelW = 248;
  const int panelH = 102;
  const int rowX = panelX + 10;
  const int rowW = panelW - 20;
  const int rowH = 32;
  const int bpmRowY = panelY + 16;
  const int swingRowY = panelY + 54;
  const uint16_t shadowColor = PANEL_COLOR(0x2104);
  const uint16_t panelFillColor = getUiInactiveFillColor();
  const uint16_t panelBorderColor = getUiInactiveBorderColor();
  char bpmText[20];
  char swingText[20];
  int16_t x1;
  int16_t y1;
  uint16_t textW;
  uint16_t textH;
  int textX;

  //-- Subtle dim zone behind the popup while keeping most of the screen visible.
  tft.fillRoundRect(shadowX, shadowY, shadowW, shadowH, 10, shadowColor);

  //-- Bright popup panel.
  tft.fillRoundRect(panelX, panelY, panelW, panelH, 10, panelFillColor);
  tft.drawRoundRect(panelX, panelY, panelW, panelH, 10, panelBorderColor);

  tft.fillRoundRect(rowX, bpmRowY, rowW, rowH, 6, bpmSelected ? getUiSelectedFillColor() : panelFillColor);
  tft.drawRoundRect(rowX, bpmRowY, rowW, rowH, 6, bpmSelected ? getUiSelectedBorderColor() : panelBorderColor);

  tft.fillRoundRect(rowX, swingRowY, rowW, rowH, 6, bpmSelected ? panelFillColor : getUiSelectedFillColor());
  tft.drawRoundRect(rowX, swingRowY, rowW, rowH, 6, bpmSelected ? panelBorderColor : getUiSelectedBorderColor());

  snprintf(bpmText, sizeof(bpmText), "BPM %c%03u%c", bpmSelected ? '>' : ' ', static_cast<unsigned>(bpm), bpmSelected ? '<' : ' ');
  snprintf(swingText, sizeof(swingText), "SW  %c%02u%%%c", bpmSelected ? ' ' : '>', static_cast<unsigned>(swingPercent), bpmSelected ? ' ' : '<');

  tft.setTextSize(2);

  tft.setTextColor(bpmSelected ? getUiSelectedTextColor() : getUiInactiveTextColor(), bpmSelected ? getUiSelectedFillColor() : panelFillColor);
  tft.getTextBounds(bpmText, 0, 0, &x1, &y1, &textW, &textH);
  textX = rowX + ((rowW - static_cast<int>(textW)) / 2);
  tft.setCursor(textX, bpmRowY + 8);
  tft.print(bpmText);

  tft.setTextColor(bpmSelected ? getUiInactiveTextColor() : getUiSelectedTextColor(), bpmSelected ? panelFillColor : getUiSelectedFillColor());
  tft.getTextBounds(swingText, 0, 0, &x1, &y1, &textW, &textH);
  textX = rowX + ((rowW - static_cast<int>(textW)) / 2);
  tft.setCursor(textX, swingRowY + 8);
  tft.print(swingText);

} //   DisplayDriver::drawTempoOverlay()

//--- Draw generic selection popup card over the active screen.
void DisplayDriver::drawSelectionOverlay(const char* title, const String options[], size_t optionCount, int selectedIndex)
{
  const int shadowX = 20;
  const int shadowY = 36;
  const int shadowW = 284;
  const int maxPanelH = 188;
  const int panelX = 28;
  const int panelY = 44;
  const int panelW = 268;
  const int rowX = panelX + 10;
  const int rowW = panelW - 20;
  const int rowH = 22;
  const int titleY = panelY + 8;
  const int listTopY = panelY + 34;
  const int maxVisibleRows = 6;
  const uint16_t shadowColor = PANEL_COLOR(0x2104);
  const uint16_t panelFillColor = getUiInactiveFillColor();
  const uint16_t panelBorderColor = getUiInactiveBorderColor();
  const size_t maxOptionChars = 20;
  int visibleRows;
  int panelH;
  int firstVisibleIndex = 0;

  if (optionCount == 0)
  {
    return;
  }

  if (selectedIndex < 0)
  {
    selectedIndex = 0;
  }
  else if (selectedIndex >= static_cast<int>(optionCount))
  {
    selectedIndex = static_cast<int>(optionCount) - 1;
  }

  visibleRows = static_cast<int>(optionCount);

  if (visibleRows > maxVisibleRows)
  {
    visibleRows = maxVisibleRows;
    firstVisibleIndex = selectedIndex - (maxVisibleRows / 2);

    if (firstVisibleIndex < 0)
    {
      firstVisibleIndex = 0;
    }

    if (firstVisibleIndex > (static_cast<int>(optionCount) - maxVisibleRows))
    {
      firstVisibleIndex = static_cast<int>(optionCount) - maxVisibleRows;
    }
  }

  panelH = 42 + (visibleRows * rowH);

  if (panelH > maxPanelH)
  {
    panelH = maxPanelH;
  }

  tft.fillRoundRect(shadowX, shadowY, shadowW, panelH + 8, 10, shadowColor);
  tft.fillRoundRect(panelX, panelY, panelW, panelH, 10, panelFillColor);
  tft.drawRoundRect(panelX, panelY, panelW, panelH, 10, panelBorderColor);

  tft.setTextSize(2);
  tft.setTextColor(getUiInactiveTextColor(), panelFillColor);
  tft.setCursor(panelX + 10, titleY);
  tft.print(title != nullptr ? title : "Select");

  for (int rowIndex = 0; rowIndex < visibleRows; rowIndex++)
  {
    int optionIndex = firstVisibleIndex + rowIndex;
    int rowY = listTopY + (rowIndex * rowH);
    bool rowSelected = (optionIndex == selectedIndex);
    String optionText = options[optionIndex];

    if (optionText.length() > maxOptionChars)
    {
      optionText = optionText.substring(0, maxOptionChars);
    }

    tft.fillRoundRect(rowX, rowY, rowW, rowH - 2, 5, rowSelected ? getUiSelectedFillColor() : panelFillColor);
    tft.drawRoundRect(rowX, rowY, rowW, rowH - 2, 5, rowSelected ? getUiSelectedBorderColor() : panelBorderColor);

    tft.setTextColor(rowSelected ? getUiSelectedTextColor() : getUiInactiveTextColor(), rowSelected ? getUiSelectedFillColor() : panelFillColor);
    tft.setCursor(rowX + 8, rowY + 4);

    tft.print(optionText);
  }

} //   DisplayDriver::drawSelectionOverlay()

//--- Add a generic tile to the registry
int DisplayDriver::addTile(const char* name, int row, int column, int size, const std::string& text, DisplayTextAlign align)
{
  int tileIndex;

  if (name == nullptr || tileCount >= maxTileCount)
  {
    return -1;
  }

  tileIndex = findTileIndexByName(name);
  if (tileIndex < 0)
  {
    tileIndex = tileCount;
    tileCount++;
  }

  tileRegistry[tileIndex].name = name;
  tileRegistry[tileIndex].row = row;
  tileRegistry[tileIndex].column = column;
  tileRegistry[tileIndex].size = (size <= 0) ? 1 : size;
  tileRegistry[tileIndex].text = text;
  tileRegistry[tileIndex].align = align;
  tileRegistry[tileIndex].fillColor = getUiSelectedFillColor();
  tileRegistry[tileIndex].borderColor = getUiSelectedBorderColor();
  tileRegistry[tileIndex].textColor = getUiSelectedTextColor();

  drawGenericTile(tileRegistry[tileIndex]);

  return tileIndex;

} //   DisplayDriver::addTile()

//--- Update a tile by index
bool DisplayDriver::updateTile(int tileIndex, const std::string& text)
{
  if (tileIndex < 0 || tileIndex >= tileCount)
  {
    return false;
  }

  tileRegistry[tileIndex].text = text;
  drawGenericTile(tileRegistry[tileIndex]);

  return true;

} //   DisplayDriver::updateTile()

//--- Update a tile by name
bool DisplayDriver::updateTile(const char* name, const std::string& text)
{
  int tileIndex = findTileIndexByName(name);

  if (tileIndex < 0)
  {
    return false;
  }

  return updateTile(tileIndex, text);

} //   DisplayDriver::updateTile()

//--- Clear tile registry
void DisplayDriver::clearTiles()
{
  tileCount = 0;

} //   DisplayDriver::clearTiles()

//--- Draw a list screen
void DisplayDriver::drawListScreen(const char* title, const String items[], size_t itemCount, int selectedIndex, int firstVisibleIndex, const char* rightText)
{
  invalidateStatusScreenCache();
  tft.fillScreen(ST77XX_BLACK);
  drawHeader(title, rightText);

  tft.setTextSize(2);

  const int visibleLines = 9;

  for (int line = 0; line < visibleLines; line++)
  {
    int itemIndex = firstVisibleIndex + line;

    if (itemIndex >= static_cast<int>(itemCount))
    {
      break;
    }

    int y = 30 + (line * 22);

    if (itemIndex == selectedIndex)
    {
      tft.fillRect(0, y, tft.width(), 22, ST77XX_BLACK);
      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.setCursor(6, y);
      tft.print(">");
      tft.print(items[itemIndex]);
      tft.print("<");
    }
    else
    {
      tft.fillRect(0, y, tft.width(), 22, ST77XX_BLACK);
      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.setCursor(6, y);
      tft.print(" ");
      tft.print(items[itemIndex]);
    }
  }

} //   DisplayDriver::drawListScreen()

//--- Draw one list line on the active list screen
void DisplayDriver::drawListLine(int lineIndex, const String& text, bool selected)
{
  if (lineIndex < 0)
  {
    return;
  }

  int y = 30 + (lineIndex * 22);

  if (y >= static_cast<int>(tft.height()))
  {
    return;
  }

  tft.setTextSize(2);

  if (selected)
  {
    tft.fillRect(0, y, tft.width(), 22, ST77XX_BLACK);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(6, y);
    tft.print(">");
    tft.print(text);
    tft.print("<");
  }
  else
  {
    tft.fillRect(0, y, tft.width(), 22, ST77XX_BLACK);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(6, y);
    tft.print(" ");
    tft.print(text);
  }

} //   DisplayDriver::drawListLine()

//--- Highlight one character cell inside a list row.
void DisplayDriver::drawListCharacterHighlight(int lineIndex, int charIndex, char character)
{
  const int rowY = 30 + (lineIndex * 22);
  const int charWidth = 12;
  const int charHeight = 22;
  const int rowPrefixChars = 1;
  const int charX = 6 + ((rowPrefixChars + charIndex) * charWidth);
  uint16_t fillColor = getUiSelectedFillColor();
  uint16_t textColor = getUiSelectedTextColor();

  if (lineIndex < 0 || charIndex < 0)
  {
    return;
  }

  if (rowY >= static_cast<int>(tft.height()) || charX >= static_cast<int>(tft.width()))
  {
    return;
  }

  tft.setTextSize(2);
  tft.fillRect(charX, rowY, charWidth, charHeight, fillColor);
  tft.setTextColor(textColor, fillColor);
  tft.setCursor(charX, rowY);
  tft.print(character);

} //   DisplayDriver::drawListCharacterHighlight()

//--- Class API wrapper: list screen with disabled items
void DisplayDriver::drawListScreenWithDisabledItems(const char* title, const String items[], size_t itemCount, int selectedIndex, int firstVisibleIndex, const bool disabledItems[])
{
  invalidateStatusScreenCache();
  tft.fillScreen(ST77XX_BLACK);
  drawHeader(title);

  tft.setTextSize(2);

  const int visibleLines = 9;

  for (int line = 0; line < visibleLines; line++)
  {
    int itemIndex = firstVisibleIndex + line;

    if (itemIndex >= static_cast<int>(itemCount))
    {
      break;
    }

    int y = 30 + (line * 22);
    bool isDisabled = disabledItems && disabledItems[itemIndex];

    if (itemIndex == selectedIndex)
    {
      tft.fillRect(0, y, tft.width(), 22, ST77XX_BLACK);
      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.setCursor(6, y);
      tft.print(">");
      tft.print(items[itemIndex]);
      tft.print("<");
    }
    else
    {
      tft.fillRect(0, y, tft.width(), 22, ST77XX_BLACK);

      if (isDisabled)
      {
        //--- Grayed out text for disabled items
        tft.setTextColor(0x4208, ST77XX_BLACK);
      }
      else
      {
        tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      }

      tft.setCursor(6, y);
      tft.print(" ");
      tft.print(items[itemIndex]);
    }
  }

} //   DisplayDriver::drawListScreenWithDisabledItems()

//--- Refresh current header line when the supplied header text changes
void DisplayDriver::refreshHeaderIfNeeded(const char* rightText, uint32_t minimumIntervalMs)
{
  uint32_t nowMs = millis();
  std::string rightTextValue = rightText != nullptr ? rightText : "";
  bool headerTitleAvailable = !activeHeaderTitle.empty();
  bool rightTextChanged = (rightTextValue != lastHeaderRightText);

  if (!headerTitleAvailable)
  {
    return;
  }

  if (rightTextChanged)
  {
    drawHeader(activeHeaderTitle.c_str(), rightTextValue.c_str());

    return;
  }

  if (nowMs - lastHeaderRefreshMs >= minimumIntervalMs)
  {
    drawHeader(activeHeaderTitle.c_str(), rightTextValue.c_str());
  }

} //   DisplayDriver::refreshHeaderIfNeeded()

//--- Class API wrapper: number editor
void DisplayDriver::drawNumberEditor(const char* label, uint32_t value, const char* unitLabel)
{
  invalidateStatusScreenCache();
  tft.fillScreen(ST77XX_BLACK);
  drawHeader("Edit Value");

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);

  tft.setCursor(6, 60);
  tft.print(label);

  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(4);
  tft.setCursor(6, 105);
  tft.printf("%lu", static_cast<unsigned long>(value));

  tft.setTextColor(getUiAccentColor());
  tft.setTextSize(2);
  tft.setCursor(6, 170);
  tft.print(unitLabel);

  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(6, 205);
  tft.print("Press=OK  K0=Back");

} //   DisplayDriver::drawNumberEditor()

//--- Class API wrapper: enum editor
void DisplayDriver::drawEnumEditor(const char* label, const char* valueLabel)
{
  invalidateStatusScreenCache();
  tft.fillScreen(ST77XX_BLACK);
  drawHeader("Edit Option");

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);

  tft.setCursor(6, 60);
  tft.print(label);

  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(4);
  tft.setCursor(6, 110);
  tft.print(valueLabel);

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(6, 205);
  tft.print("Press=OK  K0=Back");

} //   DisplayDriver::drawEnumEditor()

//--- Class API wrapper: text input
void DisplayDriver::drawTextInput(const char* title, const std::string& textValue, const std::string& currentToken)
{
  invalidateStatusScreenCache();
  tft.fillScreen(ST77XX_BLACK);
  drawHeader(title);

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);

  tft.setCursor(6, 45);
  tft.print("Name:");

  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(6, 75);
  tft.print(textValue.c_str());

  tft.setTextColor(getUiAccentColor());
  tft.setTextSize(3);
  tft.setCursor(6, 140);
  tft.print("[");
  tft.print(currentToken.c_str());
  tft.print("]");

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(6, 205);
  tft.print("Hold=Save  K0=Back");

} //   DisplayDriver::drawTextInput()

//--- Draw generic field input screen
void DisplayDriver::drawFieldInput(const char* title, const char* fieldName, const std::string& fieldValue, int cursorIndex, const std::string& prevToken, const std::string& currentToken, const std::string& nextToken,
                                   const char* tokenOptions[], int tokenOptionCount, int selectedOptionIndex)
{
  //--- Per colorSettings.md: PANEL_COLOR() for fills/borders, ST77XX_BLACK for readable text.
  uint16_t tokenCurrentColor = ST77XX_BLACK;
  uint16_t tokenNeighborColor = getUiInactiveFillColor();
  uint16_t tileFillColor = getUiSelectedFillColor();
  uint16_t tileBorderColor = getUiSelectedBorderColor();
  uint16_t tileValueColor = getUiSelectedTextColor();
  uint16_t tileLabelColor = tileValueColor;

  const int tileX = 3;
  const int tileW = 314;
  bool useButtonOptions = (cursorIndex == 0) && (tokenOptions != nullptr) && (tokenOptionCount >= 2) && (tokenOptionCount <= 6);
  bool useTwoRowButtons = useButtonOptions && (tokenOptionCount > 4);

  //--- Tile 1: field name + value, cursor arrows (higher and roomier)
  const int tile1Y = 30;
  const int tile1H = 84;

  //--- Tile 2: token selector fixed near bottom (only when NOT using button options)
  const int tile2H = !useButtonOptions ? 44 : 0;
  const int tile2Y = !useButtonOptions ? 156 : 0;

  std::string valueText = "[" + fieldValue + "]";
  std::string leftTokenText = prevToken + " < ";
  std::string rightTokenText = " > " + nextToken;
  int16_t textX1;
  int16_t textY1;
  uint16_t textW;
  uint16_t textH;
  uint16_t leftTokenW;
  uint16_t currentTokenW;
  uint16_t rightTokenW;
  int valueX;
  int tokenX;
  int cursorBaseX;
  int currentTokenX;
  int currentTokenY;

  invalidateStatusScreenCache();
  tft.fillScreen(ST77XX_BLACK);
  drawHeader(title);

  //--- Tile 1: value input
  tft.fillRoundRect(tileX, tile1Y, tileW, tile1H, 5, tileFillColor);
  tft.drawRoundRect(tileX, tile1Y, tileW, tile1H, 5, tileBorderColor);

  tft.setTextSize(2);
  tft.setTextColor(tileLabelColor, tileFillColor);
  tft.setCursor(tileX + 6, tile1Y + 5);
  tft.print(fieldName);

  tft.setTextSize(2);
  tft.getTextBounds(valueText.c_str(), 0, 0, &textX1, &textY1, &textW, &textH);
  valueX = tileX + ((tileW - static_cast<int>(textW)) / 2);

  if (valueX < tileX + 4)
  {
    valueX = tileX + 4;
  }

  cursorBaseX = valueX + 12;

  if (cursorIndex >= 0)
  {
    int cursorX = cursorBaseX + (cursorIndex * 12);
    tft.setTextColor(ST77XX_BLACK, tileFillColor);
    tft.setCursor(cursorX, tile1Y + 24);
    tft.print("v");
  }

  tft.setTextColor(tileValueColor, tileFillColor);
  tft.setCursor(valueX, tile1Y + 41);
  tft.print(valueText.c_str());

  if (cursorIndex >= 0)
  {
    int cursorX = cursorBaseX + (cursorIndex * 12);
    tft.setTextColor(ST77XX_BLACK, tileFillColor);
    tft.setCursor(cursorX, tile1Y + 62);
    tft.print("^");
  }

  //--- Tile 2: token selector (only when NOT using button options)
  if (!useButtonOptions)
  {
    tft.fillRoundRect(tileX, tile2Y, tileW, tile2H, 5, tileFillColor);
    tft.drawRoundRect(tileX, tile2Y, tileW, tile2H, 5, tileBorderColor);

    tft.setTextSize(2);
    tft.setTextColor(tileLabelColor, tileFillColor);
    tft.setCursor(tileX + 6, tile2Y + 5);
    tft.print("SELECT");
  }

  if (useButtonOptions)
  {
    const int buttonHeight = 30;
    const int buttonWidth = 92;
    const int buttonSpacing = 10;
    int itemsPerRow = useTwoRowButtons ? 3 : tokenOptionCount;
    int rowCount = useTwoRowButtons ? 2 : 1;

    //--- Calculate total button area dimensions
    int totalButtonWidth = (itemsPerRow * buttonWidth) + ((itemsPerRow - 1) * buttonSpacing);
    int totalButtonHeight = (rowCount * buttonHeight) + ((rowCount - 1) * buttonSpacing);

    //--- Center buttons horizontally on screen
    int buttonAreaX = (tft.width() - totalButtonWidth) / 2;

    //--- Position button area at bottom with padding
    const int bottomPadding = 12;
    int buttonAreaY = tft.height() - totalButtonHeight - bottomPadding;

    //--- Clear entire area for buttons (with margin)
    tft.fillRect(0, buttonAreaY - 10, tft.width(), totalButtonHeight + 20, ST77XX_BLACK);

    tft.setTextSize(2);

    for (int row = 0; row < rowCount; row++)
    {
      for (int col = 0; col < itemsPerRow; col++)
      {
        int optionIndex = row * itemsPerRow + col;

        if (optionIndex >= tokenOptionCount)
        {
          break;
        }

        int buttonX = buttonAreaX + (col * (buttonWidth + buttonSpacing));
        int buttonY = buttonAreaY + (row * (buttonHeight + buttonSpacing));
        bool isSelected = (optionIndex == selectedOptionIndex);
        const char* optionLabel = tokenOptions[optionIndex];
        drawButton(buttonX, buttonY, buttonWidth, buttonHeight, optionLabel, isSelected);
      }
    }
  }
  else
  {
    tft.setTextSize(2);
    tft.getTextBounds(leftTokenText.c_str(), 0, 0, &textX1, &textY1, &leftTokenW, &textH);
    tft.getTextBounds(currentToken.c_str(), 0, 0, &textX1, &textY1, &currentTokenW, &textH);
    tft.getTextBounds(rightTokenText.c_str(), 0, 0, &textX1, &textY1, &rightTokenW, &textH);

    textW = leftTokenW + currentTokenW + rightTokenW;
    tokenX = tileX + ((tileW - static_cast<int>(textW)) / 2);

    if (tokenX < tileX + 4)
    {
      tokenX = tileX + 4;
    }

    currentTokenY = tile2Y + 24;
    tft.setCursor(tokenX, currentTokenY);
    tft.setTextColor(tokenNeighborColor, tileFillColor);
    tft.print(leftTokenText.c_str());
    currentTokenX = tokenX + static_cast<int>(leftTokenW);

    tft.setTextColor(tokenCurrentColor, tileFillColor);
    tft.setCursor(currentTokenX, currentTokenY);
    tft.print(currentToken.c_str());
    //--- Pseudo-bold effect for current token on bitmap font.
    tft.setCursor(currentTokenX + 1, currentTokenY);
    tft.print(currentToken.c_str());

    tft.setTextColor(tokenNeighborColor, tileFillColor);
    tft.setCursor(currentTokenX + static_cast<int>(currentTokenW), currentTokenY);
    tft.print(rightTokenText.c_str());
  }

  //--- Position help text
  tft.setTextColor(ST77XX_BLACK);
  tft.setTextSize(2);
  if (useButtonOptions)
  {
    //--- Position help text above buttons
    int rowCount = useTwoRowButtons ? 2 : 1;
    int totalButtonHeight = (rowCount * 30) + ((rowCount - 1) * 10);
    int buttonAreaY = tft.height() - totalButtonHeight - 12;
    int helpTextY = buttonAreaY - 14;
    tft.setCursor(6, helpTextY);
    tft.print("Turn=Select  Short=Save");
  }
  else
  {
    tft.setCursor(6, tile2Y + tile2H + 4);
    tft.print("Short=Next  Turn=Change");
  }

} //   DisplayDriver::drawFieldInput()

//--- Draw message screen
void DisplayDriver::drawMessage(const char* title, const char* message)
{
  invalidateStatusScreenCache();
  tft.fillScreen(ST77XX_BLACK);
  drawHeader(title);

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(6, 60);
  tft.print(message);

} //   DisplayDriver::drawMessage()

//--- Draw centered WiFi portal info screen
void DisplayDriver::drawWifiPortalScreen(const std::string& line1, const std::string& line2, const std::string& line3, const std::string& line4)
{
  invalidateStatusScreenCache();
  tft.fillScreen(ST77XX_BLACK);
  drawHeader("WiFi Manager Started");

  tft.setTextSize(2);

  drawCenteredLine(line1, 62, ST77XX_WHITE, ST77XX_BLACK);
  drawCenteredLine(line2, 92, ST77XX_GREEN, ST77XX_BLACK);
  drawCenteredLine(line3, 122, ST77XX_GREEN, ST77XX_BLACK);
  drawCenteredLine(line4, 186, getUiAccentColor(), ST77XX_BLACK);

} //   DisplayDriver::drawWifiPortalScreen()

//--- Draw centered startup connection screen
void DisplayDriver::drawStartupConnectionScreen(const std::string& line1, const std::string& line2)
{
  invalidateStatusScreenCache();
  tft.fillScreen(ST77XX_BLACK);
  drawHeader("Universal Timer");

  tft.setTextSize(2);
  drawCenteredLine(line1, 86, ST77XX_WHITE, ST77XX_BLACK);
  drawCenteredLine(line2, 118, ST77XX_GREEN, ST77XX_BLACK);

} //   DisplayDriver::drawStartupConnectionScreen()

//--- Draw color palette test pattern
void DisplayDriver::drawTestColorPattern()
{
  drawTestColorPalette(0);

} //   DisplayDriver::drawTestColorPattern()

//--- Draw test palette with dark bars and cursor
void DisplayDriver::drawTestColorPalette(int selectedIndex)
{
  int barCount = 0;
  const int barY0 = 24;
  int barH;

  for (int profileIndex = 0; profileIndex < colorProfileCount; profileIndex++)
  {
    if (colorProfiles[profileIndex].usedInPalette)
    {
      barCount++;
    }
  }

  if (barCount <= 0)
  {
    return;
  }

  barH = (tft.height() - barY0) / barCount;

  if (selectedIndex < 0)
  {
    selectedIndex = 0;
  }
  else if (selectedIndex >= barCount)
  {
    selectedIndex = barCount - 1;
  }

  invalidateStatusScreenCache();
  tft.fillScreen(ST77XX_BLACK);
  drawHeader("Dark Palette");
  tft.setTextSize(2);

  int drawIndex = 0;

  for (int profileIndex = 0; profileIndex < colorProfileCount; profileIndex++)
  {
    if (!colorProfiles[profileIndex].usedInPalette)
    {
      continue;
    }

    int y = barY0 + (drawIndex * barH);
    int drawH = barH;
    uint16_t panelColor = PANEL_COLOR(colorProfiles[profileIndex].getDarkColor());
    uint16_t labelColor = mapVisualTextColorToPanelColor(colorProfiles[profileIndex].darkLabelColor);

    if (drawIndex == (barCount - 1))
    {
      drawH = tft.height() - y;
    }

    tft.fillRect(0, y, tft.width(), drawH, panelColor);
    tft.setTextColor(labelColor, panelColor);
    tft.setCursor(26, y + ((drawH - 16) / 2));
    tft.print(colorProfiles[profileIndex].colorName);

    if (drawIndex == selectedIndex)
    {
      tft.setTextColor(labelColor, panelColor);
      tft.setCursor(6, y + ((drawH - 16) / 2));
      tft.print(">");
    }

    drawIndex++;
  }

} //   DisplayDriver::drawTestColorPalette()

//--- Draw color fade test for selected color
void DisplayDriver::drawTestColorFade(const char* colorName, uint16_t darkColorVisual, uint16_t lightColorVisual, VisualTextColor darkLabelColor, VisualTextColor lightLabelColor)
{
  const int rowCount = 8;
  const int rowY0 = 24;
  const int rowH = (tft.height() - rowY0) / rowCount;
  const int textHeight = 16;
  uint16_t darkLabelPanelColor = mapVisualTextColorToPanelColor(darkLabelColor);
  uint16_t lightLabelPanelColor = mapVisualTextColorToPanelColor(lightLabelColor);
  char rowText[8];
  char hexText[24];

  invalidateStatusScreenCache();
  tft.fillScreen(ST77XX_BLACK);
  drawHeader(colorName);
  tft.setTextSize(2);

  for (int rowIndex = 0; rowIndex < rowCount; rowIndex++)
  {
    int y = rowY0 + (rowIndex * rowH);
    int drawH = rowH;
    uint8_t blendFactor = static_cast<uint8_t>((rowIndex * 255) / (rowCount - 1));
    uint16_t shadeVisual = blendRgb565(darkColorVisual, lightColorVisual, blendFactor);
    uint16_t shadePanel = PANEL_COLOR(shadeVisual);

    if (rowIndex == (rowCount - 1))
    {
      drawH = tft.height() - y;
    }

    tft.fillRect(0, y, tft.width(), drawH, shadePanel);
    snprintf(rowText, sizeof(rowText), "%d", rowIndex);
    snprintf(hexText, sizeof(hexText), "0x%04X", static_cast<unsigned int>(shadeVisual));

    tft.setTextColor(darkLabelPanelColor, shadePanel);
    tft.setCursor(8, y + ((drawH - textHeight) / 2));
    tft.print(rowText);

    tft.setTextColor(lightLabelPanelColor, shadePanel);
    tft.setCursor(44, y + ((drawH - textHeight) / 2));
    tft.print(hexText);
  }

} //   DisplayDriver::drawTestColorFade()

//--- Draw a registry tile
void DisplayDriver::drawTileByIndex(int tileIndex)
{
  if (tileIndex < 0 || tileIndex >= tileCount)
  {
    return;
  }

  drawGenericTile(tileRegistry[tileIndex]);

} //   DisplayDriver::drawTileByIndex()

//--- Find tile by name
static int findTileIndexByName(const char* name)
{
  if (name == nullptr)
  {
    return -1;
  }

  for (int tileIndex = 0; tileIndex < tileCount; tileIndex++)
  {
    if (tileRegistry[tileIndex].name == name)
    {
      return tileIndex;
    }
  }

  return -1;

} //   findTileIndexByName()

//--- Draw a generic tile using the current grid settings
static void drawGenericTile(const DisplayTile& tile)
{
  int x;
  int y;
  int w;
  int h;
  int16_t textX1;
  int16_t textY1;
  uint16_t textW;
  uint16_t textH;
  int textX;
  int textY;
  uint16_t fillColor = tile.fillColor == 0 ? getUiSelectedFillColor() : tile.fillColor;
  uint16_t borderColor = tile.borderColor == 0 ? getUiSelectedBorderColor() : tile.borderColor;
  uint16_t textColor = tile.textColor == 0 ? getUiSelectedTextColor() : tile.textColor;

  x = tileGridOriginX + (tile.column * (tileGridCellWidth + tileGridGap));
  y = tileGridOriginY + (tile.row * (tileGridCellHeight + tileGridGap));
  w = (tile.size * tileGridCellWidth) + ((tile.size - 1) * tileGridGap);
  h = tileGridCellHeight;

  tft.fillRoundRect(x, y, w, h, 5, fillColor);
  tft.drawRoundRect(x, y, w, h, 5, borderColor);

  tft.setTextSize(2);
  tft.setTextColor(textColor, fillColor);
  tft.getTextBounds(tile.text.c_str(), 0, 0, &textX1, &textY1, &textW, &textH);

  switch (tile.align)
  {
  case DisplayTextAlign::Center:
    textX = x + ((w - static_cast<int>(textW)) / 2);
    break;

  case DisplayTextAlign::Right:
    textX = x + w - static_cast<int>(textW) - 4;
    break;

  case DisplayTextAlign::Left:
  default:
    textX = x + 4;
    break;
  }

  if (textX < (x + 2))
  {
    textX = x + 2;
  }

  textY = y + ((h - static_cast<int>(textH)) / 2);
  tft.setCursor(textX, textY);
  tft.print(tile.text.c_str());

} //   drawGenericTile()
