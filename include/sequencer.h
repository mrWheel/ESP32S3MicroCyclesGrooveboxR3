/*** Last Changed: 2026-06-03 - 11:01 ***/
#ifndef SEQUENCER_H
#define SEQUENCER_H

#include <Arduino.h>
#include <stdint.h>

static const uint8_t sequencerTrackCount = 6;
static const uint8_t sequencerStepCount = 16;
static const uint8_t sequencerPatternCount = 48;

//-- Trigger metadata for one step.
struct Step
{
  bool trigger;
  uint8_t velocity;
  uint8_t probability;
  bool lockEnabled;
  int8_t lockPitch;
  uint8_t lockDecay;
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
  Pattern* pattern;
  uint16_t bpm;
  uint8_t swingPercent;
  uint8_t currentStep;
  uint8_t selectedTrack;
  uint8_t cursorStep;
  uint8_t activePatternIndex;
  uint8_t playingPatternIndex;
  uint8_t chainLength;
  bool playing;
  bool editMode;
  bool chainEnabled;
};

//-- Persistable pattern payload.
struct PatternData
{
  Pattern pattern;
  uint16_t bpm;
  uint8_t swingPercent;
  bool chainEnabled;
  uint8_t chainLength;
  String chainTarget;
};

//-- Initialize deterministic sequencer state.
void sequencerInit();

//-- Advance timing from AudioTask clock and return track trigger bitmask with per-track levels and
//pitches.
bool sequencerConsumeDueStep(uint64_t nowUs, uint8_t& outStepIndex, uint8_t& outTrackMask,
                             uint8_t outTrackLevels[sequencerTrackCount],
                             int8_t outTrackPitches[sequencerTrackCount]);

//-- Handle transport and edit controls.
void sequencerTogglePlay();
void sequencerStopImmediately();
void sequencerRequestStopAfterFinalPattern(uint8_t finalPatternIndex);
void sequencerSetLoadedPatternCount(uint8_t loadedPatternCount);
void sequencerClearPatternChainTargets();
void sequencerSetPatternChainTarget(uint8_t slotIndex, uint8_t targetSlotIndex, bool hasTarget);
void sequencerToggleEditMode();
void sequencerMoveCursor(int delta);
void sequencerMoveTrack(int delta);
void sequencerMoveTrackAndPattern(int delta, uint8_t loadedPatternCount);
void sequencerAdjustActivePatternIndex(int delta);
void sequencerSetActivePatternIndex(uint8_t slotIndex);
void sequencerToggleCurrentStep();
void sequencerAdjustCurrentStepVelocity(int delta);
void sequencerAdjustCurrentStepProbability(int delta);
void sequencerAdjustCurrentStepLockPitch(int delta);
void sequencerAdjustCurrentStepLockDecay(int delta);
void sequencerToggleCurrentStepLock();
void sequencerAdjustBpm(int delta);
void sequencerAdjustSwing(int delta);
void sequencerToggleMuteForSelectedTrack();
void sequencerAdjustChainLength(int delta);
void sequencerToggleChainEnabled();

//-- Pattern memory actions.
void sequencerStorePattern(uint8_t slotIndex);
void sequencerLoadPattern(uint8_t slotIndex);
void sequencerCreatePatternSlot(uint8_t slotIndex, uint8_t patternNumber);
void sequencerDeletePatternSlot(uint8_t slotIndex, uint8_t loadedPatternCount);

//-- Import/export helpers for pattern storage.
void sequencerExportPattern(PatternData& outData);
void sequencerExportPatternFromSlot(uint8_t slotIndex, PatternData& outData);
void sequencerImportPattern(const PatternData& patternData);
void sequencerImportPatternToSlot(uint8_t slotIndex, const PatternData& patternData);
void sequencerClearActivePattern();

//-- Copy current state into a view snapshot.
void sequencerGetView(SequencerView& outView);

#endif