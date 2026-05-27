/*** Last Changed: 2026-05-27 - 17:49 ***/
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

//-- Polyphonic voice structure (Phase 4)
struct Voice
{
  bool active;
  SampleId sampleId;
  const int16_t* sampleData;
  uint32_t frameCount;
  uint32_t position;
  uint8_t level;
  uint16_t gain;           //-- Per-voice gain (fixed-point, 0..65535)
  int8_t pan;              //-- -64 (left) .. +64 (right)
  uint8_t chokeGroup;      //-- 0 = none, >0 = choke group
  bool releaseActive;      //-- true = in release fade
  uint16_t releaseCounter; //-- release fade progress
};

//-- Initialize I2S, DMA and mixer state.
bool audioEngineInit();

//-- Return true when I2S output is available.
bool audioEngineIsOutputReady();

//-- Trigger sample playback with full voice params.
void audioEngineTriggerSample(SampleId sampleId, uint8_t level, uint16_t gain = 65535, int8_t pan = 0, uint8_t chokeGroup = 0);

//-- Render one audio block and write to I2S.
void audioEngineRenderBlock();

//-- Enable or disable sine test tone when no voices are active.
void audioEngineSetTestToneEnabled(bool enabled);

//-- Copy audio statistics for status UI.
void audioEngineGetStats(AudioEngineStats& outStats);

#endif