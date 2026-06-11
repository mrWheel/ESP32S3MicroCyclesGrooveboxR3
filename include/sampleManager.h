/*** Last Changed: 2026-06-11 - 11:30 ***/
#ifndef SAMPLE_MANAGER_H
#define SAMPLE_MANAGER_H

#include <Arduino.h>
#include <stdint.h>

//-- Fixed sample identifiers.
enum SampleId : uint8_t
{
  sampleKick = 0,
  sampleSnare,
  sampleClosedHat,
  sampleOpenHat,
  sampleTone,
  sampleMetal,
  sampleCount
};

//-- Decoded mono PCM sample slot.
struct SampleSlot
{
  const int16_t* data;
  uint32_t frameCount;
  bool valid;
  bool fromSd;
  bool storedInPsram;
  char name[16];
};

//-- Initialize SD card and load samples (from active sample set).
bool sampleManagerInit();

//-- True when SD card is mounted and usable.
bool sampleManagerIsSdCardReady();

bool sampleManagerListSampleSets(char sampleSetNames[][4], uint8_t maxSampleSets,
                                 uint8_t* sampleSetCount);
bool sampleManagerLoadSampleSet(const char* sampleSetName);

//-- Set the active sample set ("S1".."S9"). Returns true if successful.
bool sampleManagerSetActiveSampleSet(const char* setName);

//-- Get the active sample set name (e.g., "S1").
const char* sampleManagerGetActiveSampleSet();

//-- Get per-sample gain percent for a given SampleId (loaded from setGain.json)
uint16_t sampleManagerGetSampleGainPercent(SampleId sampleId);

//-- Get sample slot by identifier.
const SampleSlot& sampleManagerGetSample(SampleId sampleId);

//-- Get sample slot by track index.
const SampleSlot& sampleManagerGetSampleForTrack(uint8_t trackIndex);

#endif