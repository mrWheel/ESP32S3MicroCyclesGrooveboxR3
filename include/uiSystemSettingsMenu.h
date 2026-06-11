/*** Last Changed: 2026-06-11 - 14:31 ***/
#ifndef UI_SYSTEM_SETTINGS_MENU_H
#define UI_SYSTEM_SETTINGS_MENU_H

#include <Arduino.h>

//-- Draw System Settings main menu and update first visible index.
void uiSystemSettingsMenuDraw(int menuSelection, int& firstVisibleIndex, const String& ssidValue,
                              const String& ipValue, const String& macValue, const char* themeName,
                              int displayRotation, bool encoderReversed, bool patternGroupDirty);

#endif
