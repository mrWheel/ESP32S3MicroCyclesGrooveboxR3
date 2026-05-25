/*** Last Changed: 2026-05-25 - 18:06 ***/
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

//-- Initialize SD card and load samples.
bool sampleManagerInit();

//-- True when SD card is mounted and usable.
bool sampleManagerIsSdCardReady();

//-- Get sample slot by identifier.
const SampleSlot& sampleManagerGetSample(SampleId sampleId);

//-- Get sample slot by track index.
const SampleSlot& sampleManagerGetSampleForTrack(uint8_t trackIndex);

#endif