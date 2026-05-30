/*** Last Changed: 2026-05-30 - 17:15 ***/
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

static const size_t patternStoreMaxEntries = 24;

struct RuntimeSettings
{
  uint8_t displayRotation;
  int themeColorIndex;
  bool encoderDirectionReversed;
  String activePatternName;
};

enum class PatternStorageTarget : uint8_t
{
  Local = 0,
  Card = 1
};

//-- Load persisted display rotation.
uint8_t settingsStoreLoadDisplayRotation();

//-- Load persisted runtime settings.
void settingsStoreLoadRuntimeSettings(RuntimeSettings& settings);

//-- Save runtime settings.
bool settingsStoreSaveRuntimeSettings(const RuntimeSettings& settings);

//-- Initialize pattern storage directory.
bool settingsStoreInitPatternStorage();

//-- List available pattern names without extension.
bool settingsStoreListPatterns(String patternNames[], size_t maxCount, size_t& outCount);

//-- Find highest existing Local pNN pattern and return zero-based sequencer index.
bool settingsStoreFindHighestLocalPatternIndex(uint8_t& outPatternIndex);

//-- List available pattern names for one series letter (A..Z), sorted numerically.
bool settingsStoreListPatternsForSeries(char patternLetter, String patternNames[], size_t maxCount,
                                        size_t& outCount);

//-- Find next available default pattern name.
bool settingsStoreFindNextPatternName(String& outName);

//-- Find next available pattern name for one letter group (A01..Z99).
bool settingsStoreFindNextPatternNameForLetter(char patternLetter, String& outName);

//-- Find next available SD pattern name for one letter group (A01..Z99).
bool settingsStoreFindNextPatternNameForLetterOnCard(char patternLetter, String& outName);

//-- Count remaining free pattern slots for one letter group (0..99).
bool settingsStoreCountAvailablePatternSlotsForLetter(char patternLetter, int& outFreeCount);

//-- Count remaining free SD pattern slots for one letter group (0..99).
bool settingsStoreCountAvailablePatternSlotsForLetterOnCard(char patternLetter, int& outFreeCount);

//-- Return LittleFS usage values in bytes.
bool settingsStoreGetLittleFsUsage(size_t& outTotalBytes, size_t& outUsedBytes,
                                   size_t& outFreeBytes);

//-- Return SD usage values in bytes.
bool settingsStoreGetSdUsage(size_t& outTotalBytes, size_t& outUsedBytes, size_t& outFreeBytes);

//-- Save pattern payload to LittleFS.
bool settingsStoreSavePattern(const String& patternName, const PatternData& patternData);

//-- Save pattern payload to SD card (with group).
bool settingsStoreSavePatternToCard(const String& groupName, const String& patternName,
                                    const PatternData& patternData);

//-- List available pattern names in a group on SD card.
bool settingsStoreListPatternsInGroupOnCard(const String& groupName, String patternNames[],
                                            size_t maxCount, size_t& outCount);

//-- Load pattern payload from SD card (with group).
bool settingsStoreLoadPatternFromCard(const String& groupName, const String& patternName,
                                      PatternData& patternData);

//-- Copy one complete Card pattern group into Local working storage.
bool settingsStoreLoadPatternGroupFromCardToLocal(const String& groupName);

//-- Load pattern payload from LittleFS.
bool settingsStoreLoadPattern(const String& patternName, PatternData& patternData);

//-- Load chain settings from one existing pattern JSON file.
bool settingsStoreLoadPatternChainSettings(const String& patternName, bool& outEnabled,
                                           uint8_t& outLength, String& outTarget);

//-- Load chain settings from one existing SD card pattern JSON file.
bool settingsStoreLoadPatternChainSettingsFromCard(const String& patternName, bool& outEnabled,
                                                   uint8_t& outLength, String& outTarget);

//-- Update only chain settings in one existing pattern JSON file.
bool settingsStoreSavePatternChainSettings(const String& patternName, bool chainEnabled,
                                           uint8_t chainLength, const String& chainTarget);

//-- Delete one pattern file.
bool settingsStoreDeletePattern(const String& patternName);

//-- Delete one SD card pattern file (with group).
bool settingsStoreDeletePatternFromCard(const String& groupName, const String& patternName);

#endif