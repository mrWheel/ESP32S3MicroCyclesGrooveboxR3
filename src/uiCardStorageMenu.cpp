/*** Last Changed: 2026-06-18 - 12:08 ***/
#include "uiCardStorageMenu.h"

#include <Arduino.h>

//-- Card Storage menu configuration.
static const int cardStorageMenuEntryCount = 7;

//-- Keep list selection visible within a small scroll window.
static void updateCardStorageFirstVisibleIndex(int selectedIndex, int itemCount,
                                               int& firstVisibleIndex)
{
  static const int visibleRows = 6;
  static const int upperScrollTrigger = 1;
  static const int lowerScrollTrigger = visibleRows - 2;
  int maxFirstVisible = itemCount - visibleRows;

  if (maxFirstVisible < 0)
  {
    maxFirstVisible = 0;
  }

  if (selectedIndex < firstVisibleIndex + upperScrollTrigger && firstVisibleIndex > 0)
  {
    firstVisibleIndex--;
  }
  else if (selectedIndex >= (firstVisibleIndex + lowerScrollTrigger) &&
           firstVisibleIndex < maxFirstVisible)
  {
    firstVisibleIndex++;
  }

  if (firstVisibleIndex < 0)
  {
    firstVisibleIndex = 0;
  }
  else if (firstVisibleIndex > maxFirstVisible)
  {
    firstVisibleIndex = maxFirstVisible;
  }

} //   updateCardStorageFirstVisibleIndex()

//-- Draw Card Storage submenu and update its visible scroll window.
void uiCardStorageMenuDraw(DisplayDriver& display, int selection, int& firstVisibleIndex,
                           bool patternGroupDirty)
{
  String items[cardStorageMenuEntryCount];

  items[0] = patternGroupDirty ? "Save Group *" : "Save Group";
  items[1] = "Load Group";
  items[2] = "New Group";
  items[3] = "Rename Group";
  items[4] = "Copy Group";
  items[5] = "Delete Group";
  items[6] = "Exit";

  updateCardStorageFirstVisibleIndex(selection, cardStorageMenuEntryCount, firstVisibleIndex);

  display.drawListScreen("Card Storage", items, cardStorageMenuEntryCount, selection,
                         firstVisibleIndex);

} //   uiCardStorageMenuDraw()
