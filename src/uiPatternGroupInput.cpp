/*** Last Changed: 2026-06-01 - 14:45 ***/
#include "uiPatternGroupInput.h"
#include "DisplayDriverClass.h"
#include <string.h>

//-- Pattern group name input configuration.
static const char* patternGroupNameInputTokens = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
static const int patternGroupNameInputLength = 8;

//-- Pattern group input runtime state.
struct UiPatternGroupInputState
{
  bool open;
  bool copyMode;
  int cursor;
  int tokenIndex;
  char value[patternGroupNameInputLength + 1];
  bool drawn;
};

//-- Module-local input state.
static UiPatternGroupInputState inputState;

//-- Build visible pattern group input text with cursor marker.
static String buildPatternGroupNameInputText()
{
  String outputText = "";

  for (int charIndex = 0; charIndex < patternGroupNameInputLength; charIndex++)
  {
    char currentChar = inputState.value[charIndex];

    if (currentChar == '\0' || currentChar == ' ')
    {
      currentChar = '-';
    }

    if (charIndex == inputState.cursor)
    {
      outputText += "[";
      outputText += currentChar;
      outputText += "]";
    }
    else
    {
      outputText += currentChar;
    }
  }

  return outputText;

} //   buildPatternGroupNameInputText()

//-- Initialize pattern group input state.
void uiPatternGroupInputInit()
{
  inputState.open = false;
  inputState.copyMode = false;
  inputState.cursor = 0;
  inputState.tokenIndex = 0;
  inputState.drawn = false;

  for (int charIndex = 0; charIndex <= patternGroupNameInputLength; charIndex++)
  {
    inputState.value[charIndex] = '\0';
  }

} //   uiPatternGroupInputInit()

//-- Open pattern group input for rename or copy mode.
void uiPatternGroupInputOpen(bool copyMode)
{
  for (int charIndex = 0; charIndex < patternGroupNameInputLength; charIndex++)
  {
    inputState.value[charIndex] = ' ';
  }

  inputState.value[patternGroupNameInputLength] = '\0';
  inputState.open = true;
  inputState.copyMode = copyMode;
  inputState.cursor = 0;
  inputState.tokenIndex = 0;
  inputState.drawn = false;

} //   uiPatternGroupInputOpen()

//-- Close pattern group input and reset redraw state.
void uiPatternGroupInputClose()
{
  inputState.open = false;
  inputState.drawn = false;

} //   uiPatternGroupInputClose()

//-- Return true when pattern group input is active.
bool uiPatternGroupInputIsOpen()
{
  return inputState.open;

} //   uiPatternGroupInputIsOpen()

//-- Return true when pattern group input is in copy mode.
bool uiPatternGroupInputIsCopyMode()
{
  return inputState.copyMode;

} //   uiPatternGroupInputIsCopyMode()

//-- Return trimmed input name.
String uiPatternGroupInputGetTrimmedName()
{
  String groupName = "";

  for (int charIndex = 0; charIndex < patternGroupNameInputLength; charIndex++)
  {
    char currentChar = inputState.value[charIndex];

    if (currentChar != '\0' && currentChar != ' ')
    {
      groupName += currentChar;
    }
  }

  groupName.trim();

  return groupName;

} //   uiPatternGroupInputGetTrimmedName()

//-- Draw or partially update pattern group input popup.
void uiPatternGroupInputDraw(const String& sourceGroupName)
{
  String lines[6];
  String inputText = buildPatternGroupNameInputText();
  String tokenText = "Turn=" + String(patternGroupNameInputTokens[inputState.tokenIndex]);
  const char* title = inputState.copyMode ? "Copy Pattern" : "Rename Pattern";

  if (!inputState.drawn)
  {
    lines[0] = inputState.copyMode ? "Copy:" : "Rename:";
    lines[1] = sourceGroupName;
    lines[2] = "To:";
    lines[3] = inputText;
    lines[4] = tokenText;
    lines[5] = inputState.copyMode ? "Hold=Copy K0=Back" : "Hold=Rename K0=Back";

    display.drawSelectionOverlay(title, lines, 6, -1);

    inputState.drawn = true;
  }
  else
  {
    display.updateSelectionOverlayRow(3, inputText);
    display.updateSelectionOverlayRow(4, tokenText);
  }

} //   uiPatternGroupInputDraw()

//-- Rotate current editable character.
void uiPatternGroupInputRotate(int delta)
{
  int tokenCount = static_cast<int>(strlen(patternGroupNameInputTokens));

  if (tokenCount <= 0)
  {
    return;
  }

  inputState.tokenIndex += delta;

  while (inputState.tokenIndex < 0)
  {
    inputState.tokenIndex += tokenCount;
  }

  while (inputState.tokenIndex >= tokenCount)
  {
    inputState.tokenIndex -= tokenCount;
  }

  inputState.value[inputState.cursor] = patternGroupNameInputTokens[inputState.tokenIndex];

} //   uiPatternGroupInputRotate()

//-- Accept current character and advance cursor.
void uiPatternGroupInputAcceptCharacter()
{
  inputState.value[inputState.cursor] = patternGroupNameInputTokens[inputState.tokenIndex];

  if (inputState.cursor < patternGroupNameInputLength - 1)
  {
    inputState.cursor++;
  }

} //   uiPatternGroupInputAcceptCharacter()

//-- Backspace current character or cancel when cursor is at first position.
void uiPatternGroupInputBackspaceOrCancel()
{
  if (inputState.cursor <= 0)
  {
    uiPatternGroupInputClose();
    return;
  }

  inputState.value[inputState.cursor] = ' ';
  inputState.cursor--;
  inputState.value[inputState.cursor] = ' ';

} //   uiPatternGroupInputBackspaceOrCancel()
