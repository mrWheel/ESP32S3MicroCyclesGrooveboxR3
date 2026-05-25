/*** Last Changed: 2026-05-25 - 18:06 ***/
#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <Arduino.h>
#include <cstdint>
#include <string>
#include "timerTypes.h"
#include "colorSettings.h"

//--- Text alignment for tile content
enum class DisplayTextAlign
{
  Left,
  Center,
  Right
};

//--- Generic tile definition
struct DisplayTile
{
  std::string name;
  int row = 0;
  int column = 0;
  int size = 1;
  std::string text;
  DisplayTextAlign align = DisplayTextAlign::Left;
  uint16_t fillColor = 0;
  uint16_t borderColor = 0;
  uint16_t textColor = 0;
};

//--- Generic display theme for the current hardware
struct DisplayTheme
{
  uint16_t selectedFillColor = 0;
  uint16_t selectedBorderColor = 0;
  uint16_t selectedTextColor = 0;
  uint16_t inactiveFillColor = 0;
  uint16_t inactiveBorderColor = 0;
  uint16_t inactiveTextColor = 0;
  uint16_t accentColor = 0;
};

//--- Precomputed status screen data for the reusable display layer
struct DisplayStatusScreenData
{
  std::string title;
  std::string headerRightText;
  std::string actionRowMessage;
  std::string stateValue;
  std::string profileValue;
  std::string leftTimeTileLabel;
  std::string leftTimeTileValue;
  std::string rightTimeTileLabel;
  std::string rightTimeTileValue;
  std::string centerTileLabel;
  std::string centerTileValue;
  std::string bottomTileLabel;
  std::string bottomTileValue;
  bool is24hTimer = false;
  bool externalTriggerMode = false;
  bool warpSpeedEnabled = false;
  bool hideActionsFor24h = false;
  int statusActionIndex = 0;
};

//--- Universal display driver wrapper
class DisplayDriver
{
public:
  void init(uint16_t width, uint16_t height, int rotation);
  void setRotation(int rotation);
  int getRotation();
  void setTheme(const DisplayTheme& theme);
  const DisplayTheme& getTheme() const;
  void setBacklight(bool enabled);
  void setTileGrid(int originX, int originY, int cellWidth, int cellHeight, int gap);
  void clearScreen(uint16_t color);
  void drawHeader(const char* title, const char* rightText = nullptr);
  void drawCenteredLine(const std::string& lineText, int y, uint16_t textColor, uint16_t backgroundColor);
  void drawStatusScreen(const DisplayStatusScreenData& data);

  int addTile(const char* name, int row, int column, int size, const std::string& text, DisplayTextAlign align = DisplayTextAlign::Left);
  bool updateTile(int tileIndex, const std::string& text);
  bool updateTile(const char* name, const std::string& text);
  void clearTiles();
  void drawListScreen(const char* title, const String items[], size_t itemCount, int selectedIndex, int firstVisibleIndex, const char* rightText = nullptr);
  void drawListLine(int lineIndex, const String& text, bool selected);
  void drawListCharacterHighlight(int lineIndex, int charIndex, char character);
  void drawListScreenWithDisabledItems(const char* title, const String items[], size_t itemCount, int selectedIndex, int firstVisibleIndex, const bool disabledItems[]);
  void drawTempoOverlay(uint16_t bpm, uint8_t swingPercent, bool bpmSelected);
  void drawSelectionOverlay(const char* title, const String options[], size_t optionCount, int selectedIndex);
  void refreshHeaderIfNeeded(const char* rightText, uint32_t minimumIntervalMs);
  void drawNumberEditor(const char* label, uint32_t value, const char* unitLabel);
  void drawEnumEditor(const char* label, const char* valueLabel);
  void drawTextInput(const char* title, const std::string& textValue, const std::string& currentToken);
  void drawFieldInput(const char* title, const char* fieldName, const std::string& fieldValue, int cursorIndex, const std::string& prevToken, const std::string& currentToken, const std::string& nextToken,
                      const char* tokenOptions[], int tokenOptionCount, int selectedOptionIndex);
  void drawMessage(const char* title, const char* message);
  void drawWifiPortalScreen(const std::string& line1, const std::string& line2, const std::string& line3, const std::string& line4);
  void drawStartupConnectionScreen(const std::string& line1, const std::string& line2);
  void drawTestColorPattern();
  void drawTestColorPalette(int selectedIndex);
  void drawTestColorFade(const char* colorName, uint16_t darkColorVisual, uint16_t lightColorVisual, VisualTextColor darkLabelColor, VisualTextColor lightLabelColor);
  void drawButton(int x, int y, int width, int height, const char* label, bool selected);

private:
  void drawTileByIndex(int tileIndex);
};

extern DisplayDriver display;

//--- Initialize display
void displayInit();
void displaySetRotation(int rotation);
int displayGetRotation();
void displaySetThemeColorIndex(int index);
int displayGetThemeColorIndex();

//--- Force next Timer Screen draw to rebuild from scratch
void displayForceStatusScreenRebuild();

//--- Draw text list
void displayDrawListScreen(const char* title, const String items[], size_t itemCount, int selectedIndex, int firstVisibleIndex);

//--- Draw text list with disabled items support
void displayDrawListScreenWithDisabledItems(const char* title, const String items[], size_t itemCount, int selectedIndex, int firstVisibleIndex, const bool disabledItems[]);

//--- Refresh current header line when time/WiFi text changes
void displayRefreshHeaderIfNeeded(const char* rightText, uint32_t minimumIntervalMs);

//--- Draw numeric editor
void displayDrawNumberEditor(const char* label, uint32_t value, const char* unitLabel);

//--- Draw enum editor
void displayDrawEnumEditor(const char* label, const char* valueLabel);

//--- Draw text input editor
void displayDrawTextInput(const char* title, const std::string& textValue, const std::string& currentToken);

//--- Draw 24h timer editor
void displayDraw24hTimerEditor(uint8_t hourIndex, bool hourIsCursor, const char* typeLabel, bool typeIsCursor, bool showQuarters, const char* quarterStateLabels[4], int quarterCursorSlot, bool quarterSlotHasCursor, const char* quarterTypeCursorLabel, bool quarterTypeIsCursor);

//--- Draw generic field input screen
void displayDrawFieldInput(const char* title, const char* fieldName, const std::string& fieldValue, int cursorIndex, const std::string& prevToken, const std::string& currentToken, const std::string& nextToken,
                           const char* tokenOptions[], int tokenOptionCount, int selectedOptionIndex);

//--- Draw message screen
void displayDrawMessage(const char* title, const char* message);

//--- Draw centered WiFi portal info screen
void displayDrawWifiPortalScreen(const std::string& line1, const std::string& line2, const std::string& line3, const std::string& line4);

//--- Draw centered startup connection screen
void displayDrawStartupConnectionScreen(const std::string& line1, const std::string& line2);

//--- Draw color palette test pattern
void displayDrawTestColorPattern();

//--- Draw test palette with dark bars and cursor
void displayDrawTestColorPalette(int selectedIndex);

//--- Draw color fade test for selected color
void displayDrawTestColorFade(const char* colorName, uint16_t darkColorVisual, uint16_t lightColorVisual, VisualTextColor darkLabelColor, VisualTextColor lightLabelColor);

//--- Set display backlight state
void displaySetBacklight(bool enabled);

#endif
