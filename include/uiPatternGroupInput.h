/*** Last Changed: 2026-06-01 - 14:45 ***/
#ifndef UI_PATTERN_GROUP_INPUT_H
#define UI_PATTERN_GROUP_INPUT_H

#include <Arduino.h>

//-- Initialize pattern group input state.
void uiPatternGroupInputInit();

//-- Open pattern group input for rename or copy mode.
void uiPatternGroupInputOpen(bool copyMode);

//-- Close pattern group input and reset redraw state.
void uiPatternGroupInputClose();

//-- Return true when pattern group input is active.
bool uiPatternGroupInputIsOpen();

//-- Return true when pattern group input is in copy mode.
bool uiPatternGroupInputIsCopyMode();

//-- Return trimmed input name.
String uiPatternGroupInputGetTrimmedName();

//-- Draw or partially update pattern group input popup.
void uiPatternGroupInputDraw(const String& sourceGroupName);

//-- Rotate current editable character.
void uiPatternGroupInputRotate(int delta);

//-- Accept current character and advance cursor.
void uiPatternGroupInputAcceptCharacter();

//-- Backspace current character or cancel when cursor is at first position.
void uiPatternGroupInputBackspaceOrCancel();

#endif
