/*** Last Changed: 2026-06-01 - 15:04 ***/
#include "uiCardStorageMenu.h"

#include <Arduino.h>

//-- Card Storage menu configuration.
static const int cardStorageMenuEntryCount = 5;

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
void uiCardStorageMenuDraw(DisplayDriver& display, int selection, int& firstVisibleIndex)
{
  String items[cardStorageMenuEntryCount];

  items[0] = "Load Pattern";
  items[1] = "Save Pattern";
  items[2] = "Rename Pattern";
  items[3] = "Copy Pattern";
  items[4] = "Exit";

  updateCardStorageFirstVisibleIndex(selection, cardStorageMenuEntryCount, firstVisibleIndex);

  display.drawListScreen("Card Storage", items, cardStorageMenuEntryCount, selection,
                         firstVisibleIndex);

} //   uiCardStorageMenuDraw()
