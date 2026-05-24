/*** Last Changed: 2026-05-24 - 11:12 ***/
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
  char name[16];
};

//-- Initialize LittleFS and preload samples.
bool sampleManagerInit();

//-- Get sample slot by identifier.
const SampleSlot& sampleManagerGetSample(SampleId sampleId);

//-- Get sample slot by track index.
const SampleSlot& sampleManagerGetSampleForTrack(uint8_t trackIndex);

#endif