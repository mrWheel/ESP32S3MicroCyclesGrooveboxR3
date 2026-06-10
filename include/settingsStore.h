/*** Last Changed: 2026-06-10 - 16:50 ***/
/*** Last Changed: 2026-05-27 - 17:20 ***/
#ifndef SETTINGS_STORE_H
#define SETTINGS_STORE_H

#include <Arduino.h>
#include <stdint.h>

//-- List available pattern group names on SD card (subdirectories of /patterns)
bool settingsStoreListPatternGroupsOnCard(String groupNames[], size_t maxCount, size_t& outCount);

//-- NVS persistent storage keys
#define NVS_NAMESPACE "groovebox"

//-- Get/set active pattern group name (from NVS)
String settingsStoreGetActivePatternGroup();
bool settingsStoreSetActivePatternGroup(const String& groupName);

//-- Get/set active sample set name (from NVS)
String settingsStoreGetActiveSampleSet();
bool settingsStoreSetActiveSampleSet(const String& setName);

//-- Get/set display rotation (from NVS)
uint8_t settingsStoreGetDisplayRotation();
bool settingsStoreSetDisplayRotation(uint8_t rotation);

//-- Get/set encoder order (from NVS)
bool settingsStoreGetEncoderOrder();
bool settingsStoreSetEncoderOrder(bool reversed);

//-- Get/set theme color index (from NVS)
int settingsStoreGetThemeColorIndex();
bool settingsStoreSetThemeColorIndex(int colorIndex);

//-- Get/set WiFi credentials (from NVS)
bool settingsStoreGetWifiCredentials(String& ssid, String& password);
bool settingsStoreSetWifiCredentials(const String& ssid, const String& password);

#include "sequencer.h"

static const size_t patternStoreMaxEntries = 64;

//-- Runtime settings persisted in LittleFS settings file.
struct RuntimeSettings
{
  uint8_t displayRotation;
  int themeColorIndex;
  bool encoderDirectionReversed;
  String activePatternName;
  uint8_t masterGainPercent;

}; //   RuntimeSettings

//-- Load persisted display rotation.
uint8_t settingsStoreLoadDisplayRotation();

//-- Load persisted runtime settings.
void settingsStoreLoadRuntimeSettings(RuntimeSettings& settings);

//-- Save runtime settings.
bool settingsStoreSaveRuntimeSettings(const RuntimeSettings& settings);

//-- Get persisted master gain percentage.
uint8_t settingsStoreGetMasterGainPercent();

//-- Persist master gain percentage.
bool settingsStoreSetMasterGainPercent(uint8_t gainPercent);

//-- Return SD usage values in bytes.
bool settingsStoreGetSdUsage(size_t& outTotalBytes, size_t& outUsedBytes, size_t& outFreeBytes);

//-- Save pattern payload to SD card (with group).
bool settingsStoreSavePatternToCard(const String& groupName, const String& patternName,
                                    const PatternData& patternData);

//-- List available pattern names in a group on SD card.
bool settingsStoreListPatternsInGroupOnCard(const String& groupName, String patternNames[],
                                            size_t maxCount, size_t& outCount);

//-- Load pattern payload from SD card (with group).
bool settingsStoreLoadPatternFromCard(const String& groupName, const String& patternName,
                                      PatternData& patternData);

//-- Rename one Card pattern group directory.
bool settingsStoreRenamePatternGroupOnCard(const String& oldGroupName, const String& newGroupName);

//-- Copy one Card pattern group directory.
bool settingsStoreCopyPatternGroupOnCard(const String& sourceGroupName,
                                         const String& targetGroupName);

//-- Load chain settings from one existing pattern JSON file.
bool settingsStoreLoadPatternChainSettings(const String& patternName, bool& outEnabled,
                                           uint8_t& outLength, String& outTarget);

//-- Delete one SD card pattern file (with group).
bool settingsStoreDeletePatternFromCard(const String& groupName, const String& patternName);

#endif