/*** Last Changed: 2026-06-10 - 17:49 ***/
#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <stdint.h>
#include "sampleManager.h"

//-- Runtime diagnostics from audio engine.
struct AudioEngineStats
{
  uint32_t dmaWriteFailures;
  uint32_t activeVoiceCount;
  bool testToneEnabled;
};

//-- Audio playback voice.
struct Voice
{
  bool active;
  SampleId sampleId;
  const int16_t* sampleData;
  uint32_t frameCount;
  uint32_t playbackFrameLimit;
  uint32_t position;
  uint32_t phase;
  uint32_t phaseIncrement;
  uint8_t level;
  uint16_t gain;
  int8_t pan;
  uint8_t decayPercent;
  int8_t pitch;
  uint8_t chokeGroup;
  bool releaseActive;
  uint16_t releaseCounter;
  uint16_t attackCounter;

}; //   Voice

//-- Initialize I2S, DMA and mixer state.
bool audioEngineInit();

//-- Return true when I2S output is available.
bool audioEngineIsOutputReady();

//-- Trigger sample playback with full voice params.
void audioEngineTriggerSample(SampleId sampleId, uint8_t level, uint16_t gain, int8_t pan,
                              uint8_t chokeGroup, uint8_t decayPercent, int8_t pitch);

//-- Backward compatibility: trigger without decay and pitch.
void audioEngineTriggerSample(SampleId sampleId, uint8_t level, uint16_t gain, int8_t pan,
                              uint8_t chokeGroup);

//-- Backward compatibility: old trigger function.
void audioEngineTriggerSample(SampleId sampleId, uint8_t level);

//-- Set runtime master gain percentage.
void audioEngineSetMasterGainPercent(uint8_t gainPercent);

//-- Get runtime master gain percentage.
uint8_t audioEngineGetMasterGainPercent();

//-- Render one audio block and write to I2S.
void audioEngineRenderBlock();

//-- Enable or disable sine test tone when no voices are active.
void audioEngineSetTestToneEnabled(bool enabled);

//-- Copy audio statistics for status UI.
void audioEngineGetStats(AudioEngineStats& outStats);

#endif