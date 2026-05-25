/*** Last Changed: 2026-05-25 - 13:45 ***/
#ifndef SEQUENCER_H
#define SEQUENCER_H

#include <stdint.h>

static const uint8_t sequencerTrackCount = 6;
static const uint8_t sequencerStepCount = 16;
static const uint8_t sequencerPatternCount = 4;

//-- Trigger metadata for one step.
struct Step
{
  bool trigger;
  uint8_t velocity;
  uint8_t probability;
};

//-- One sequencer track.
struct Track
{
  Step steps[sequencerStepCount];
  bool mute;
};

//-- One full pattern memory slot.
struct Pattern
{
  Track tracks[sequencerTrackCount];
};

//-- Lightweight read-only snapshot for UI.
struct SequencerView
{
  Pattern pattern;
  uint16_t bpm;
  uint8_t swingPercent;
  uint8_t currentStep;
  uint8_t selectedTrack;
  uint8_t cursorStep;
  uint8_t activePatternIndex;
  bool playing;
  bool editMode;
};

//-- Persistable pattern payload.
struct PatternData
{
  Pattern pattern;
  uint16_t bpm;
  uint8_t swingPercent;
};

//-- Initialize deterministic sequencer state.
void sequencerInit();

//-- Advance timing from AudioTask clock and return step, trigger mask, and per-track levels.
bool sequencerConsumeDueStep(uint64_t nowUs, uint8_t& outStepIndex, uint8_t& outTrackMask, uint8_t outTrackLevels[sequencerTrackCount]);

//-- Handle transport and edit controls.
void sequencerTogglePlay();
void sequencerToggleEditMode();
void sequencerMoveCursor(int delta);
void sequencerMoveTrack(int delta);
void sequencerToggleCurrentStep();
void sequencerAdjustBpm(int delta);
void sequencerAdjustSwing(int delta);
void sequencerToggleMuteForSelectedTrack();

//-- Pattern memory actions.
void sequencerStorePattern(uint8_t slotIndex);
void sequencerLoadPattern(uint8_t slotIndex);

//-- Import/export helpers for pattern storage.
void sequencerExportPattern(PatternData& outData);
void sequencerImportPattern(const PatternData& patternData);
void sequencerClearActivePattern();

//-- Copy current state into a view snapshot.
void sequencerGetView(SequencerView& outView);

#endif