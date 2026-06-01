/*** Last Changed: 2026-06-01 - 14:45 ***/
#include "uiCardStorageActions.h"

#include "settingsStore.h"

#include <Arduino.h>

//-- Copy one pattern group on Card and make the copy active in NVS.
bool uiCardStorageCopyPatternGroup(const String& oldGroupName, const String& newGroupName,
                                   String& outStatusMessage)
{
  if (oldGroupName.isEmpty())
  {
    outStatusMessage = "No active\ngroup";
    return false;
  }

  if (newGroupName.isEmpty())
  {
    outStatusMessage = "Name empty";
    return false;
  }

  if (!settingsStoreCopyPatternGroupOnCard(oldGroupName, newGroupName))
  {
    outStatusMessage = "Copy failed\n" + newGroupName;
    return false;
  }

  if (!settingsStoreSetActivePatternGroup(newGroupName))
  {
    outStatusMessage = "NVS save failed\n" + newGroupName;
    return false;
  }

  outStatusMessage = "Copied group\n" + newGroupName;

  return true;

} //   uiCardStorageCopyPatternGroup()

//-- Rename one pattern group on Card and make the renamed group active in NVS.
bool uiCardStorageRenamePatternGroup(const String& oldGroupName, const String& newGroupName,
                                     String& outStatusMessage)
{
  if (oldGroupName.isEmpty())
  {
    outStatusMessage = "No active\ngroup";
    return false;
  }

  if (newGroupName.isEmpty())
  {
    outStatusMessage = "Name empty";
    return false;
  }

  if (!settingsStoreRenamePatternGroupOnCard(oldGroupName, newGroupName))
  {
    outStatusMessage = "Rename failed\n" + newGroupName;
    return false;
  }

  if (!settingsStoreSetActivePatternGroup(newGroupName))
  {
    outStatusMessage = "NVS save failed\n" + newGroupName;
    return false;
  }

  outStatusMessage = "Renamed group\n" + newGroupName;

  return true;

} //   uiCardStorageRenamePatternGroup()

//-- Remove Card pattern files above the currently loaded RAM pattern count.
void uiCardStorageDeleteStalePatterns(const String& groupName, uint8_t loadedPatternCount)
{
  String existingPatternNames[patternStoreMaxEntries];
  size_t existingPatternCount = 0;

  if (groupName.isEmpty())
  {
    return;
  }

  if (!settingsStoreListPatternsInGroupOnCard(groupName, existingPatternNames,
                                              patternStoreMaxEntries, existingPatternCount))
  {
    return;
  }

  for (size_t patternIndex = 0; patternIndex < existingPatternCount; patternIndex++)
  {
    String existingName = existingPatternNames[patternIndex];
    int existingNumber = existingName.substring(1).toInt();

    if (existingNumber > loadedPatternCount)
    {
      settingsStoreDeletePatternFromCard(groupName, existingName);
    }
  }

} //   uiCardStorageDeleteStalePatterns()
