/*** Last Changed: 2026-06-01 - 14:45 ***/
#ifndef UI_CARD_STORAGE_ACTIONS_H
#define UI_CARD_STORAGE_ACTIONS_H

#include <Arduino.h>

//-- Copy one pattern group on Card and make the copy active in NVS.
bool uiCardStorageCopyPatternGroup(const String& oldGroupName, const String& newGroupName,
                                   String& outStatusMessage);

//-- Rename one pattern group on Card and make the renamed group active in NVS.
bool uiCardStorageRenamePatternGroup(const String& oldGroupName, const String& newGroupName,
                                     String& outStatusMessage);

//-- Remove Card pattern files above the currently loaded RAM pattern count.
void uiCardStorageDeleteStalePatterns(const String& groupName, uint8_t loadedPatternCount);

#endif
