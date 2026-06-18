/*** Last Changed: 2026-06-18 - 12:08 ***/
#ifndef UI_CARD_STORAGE_MENU_H
#define UI_CARD_STORAGE_MENU_H

#include "DisplayDriverClass.h"

//-- Draw Card Storage submenu and update its visible scroll window.
void uiCardStorageMenuDraw(DisplayDriver& display, int selection, int& firstVisibleIndex,
                           bool patternGroupDirty);
#endif
