/*** Last Changed: 2026-05-27 - 17:20 ***/
#ifndef SETTINGS_STORE_H
#define SETTINGS_STORE_H

#include <Arduino.h>
#include <stdint.h>

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

//-- List available pattern names for one series letter (A..Z), sorted numerically.
bool settingsStoreListPatternsForSeries(char patternLetter, String patternNames[], size_t maxCount, size_t& outCount);

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
bool settingsStoreGetLittleFsUsage(size_t& outTotalBytes, size_t& outUsedBytes, size_t& outFreeBytes);

//-- Return SD usage values in bytes.
bool settingsStoreGetSdUsage(size_t& outTotalBytes, size_t& outUsedBytes, size_t& outFreeBytes);

//-- Save pattern payload to LittleFS.
bool settingsStoreSavePattern(const String& patternName, const PatternData& patternData);

//-- Save pattern payload to SD card (same JSON schema).
bool settingsStoreSavePatternToCard(const String& patternName, const PatternData& patternData);

//-- List available pattern names from SD card /patterns.
bool settingsStoreListPatternsOnCard(String patternNames[], size_t maxCount, size_t& outCount);

//-- Load pattern payload from SD card.
bool settingsStoreLoadPatternFromCard(const String& patternName, PatternData& patternData);

//-- Load pattern payload from LittleFS.
bool settingsStoreLoadPattern(const String& patternName, PatternData& patternData);

//-- Load chain settings from one existing pattern JSON file.
bool settingsStoreLoadPatternChainSettings(const String& patternName, bool& outEnabled, uint8_t& outLength, String& outTarget);

//-- Load chain settings from one existing SD card pattern JSON file.
bool settingsStoreLoadPatternChainSettingsFromCard(const String& patternName, bool& outEnabled, uint8_t& outLength, String& outTarget);

//-- Update only chain settings in one existing pattern JSON file.
bool settingsStoreSavePatternChainSettings(const String& patternName, bool chainEnabled, uint8_t chainLength, const String& chainTarget);

//-- Delete one pattern file.
bool settingsStoreDeletePattern(const String& patternName);

//-- Delete one SD card pattern file.
bool settingsStoreDeletePatternFromCard(const String& patternName);

#endif