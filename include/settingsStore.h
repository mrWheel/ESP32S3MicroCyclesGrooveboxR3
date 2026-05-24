/*** Last Changed: 2026-05-24 - 11:12 ***/
#ifndef SETTINGS_STORE_H
#define SETTINGS_STORE_H

#include <Arduino.h>
#include <stdint.h>

#include "sequencer.h"

static const size_t sequenceStoreMaxEntries = 24;

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

//-- List available sequence names without extension.
bool settingsStoreListSequences(String sequenceNames[], size_t maxCount, size_t& outCount);

//-- Find next available default sequence name.
bool settingsStoreFindNextSequenceName(String& outName);

//-- Save sequence payload to LittleFS.
bool settingsStoreSaveSequence(const String& sequenceName, const SequenceData& sequenceData);

//-- Load sequence payload from LittleFS.
bool settingsStoreLoadSequence(const String& sequenceName, SequenceData& sequenceData);

//-- Delete one sequence file.
bool settingsStoreDeleteSequence(const String& sequenceName);

#endif