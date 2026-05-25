/*** Last Changed: 2026-05-25 - 18:06 ***/
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

//-- Find next available default pattern name.
bool settingsStoreFindNextPatternName(String& outName);

//-- Save pattern payload to LittleFS.
bool settingsStoreSavePattern(const String& patternName, const PatternData& patternData);

//-- Load pattern payload from LittleFS.
bool settingsStoreLoadPattern(const String& patternName, PatternData& patternData);

//-- Delete one pattern file.
bool settingsStoreDeletePattern(const String& patternName);

#endif