/*** Last Changed: 2026-05-23 - 16:00 ***/
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

//-- Initialize I2S, DMA and mixer state.
bool audioEngineInit();

//-- Return true when I2S output is available.
bool audioEngineIsOutputReady();

//-- Trigger sample playback on a fixed voice slot.
void audioEngineTriggerSample(SampleId sampleId, uint8_t level);

//-- Render one audio block and write to I2S.
void audioEngineRenderBlock();

//-- Enable or disable sine test tone when no voices are active.
void audioEngineSetTestToneEnabled(bool enabled);

//-- Copy audio statistics for status UI.
void audioEngineGetStats(AudioEngineStats& outStats);

#endif