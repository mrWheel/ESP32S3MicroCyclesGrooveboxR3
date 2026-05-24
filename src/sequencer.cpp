/*** Last Changed: 2026-05-24 - 11:12 ***/
#include "sequencer.h"

#include <Arduino.h>

//-- Shared lock between AudioTask and UI/System tasks.
static portMUX_TYPE sequencerMux = portMUX_INITIALIZER_UNLOCKED;

//-- Full sequencer state.
struct SequencerState
{
  Pattern patterns[sequencerPatternCount];
  uint16_t bpm;
  uint8_t swingPercent;
  uint8_t currentStep;
  uint8_t cursorStep;
  uint8_t selectedTrack;
  uint8_t activePatternIndex;
  bool playing;
  bool shiftMode;
  uint64_t nextStepDueUs;
};

//-- Runtime state storage.
static SequencerState state;

//-- Apply fixed startup pattern for immediate playback testing.
static void loadDefaultPattern(Pattern& pattern)
{
  for (uint8_t trackIndex = 0; trackIndex < sequencerTrackCount; trackIndex++)
  {
    pattern.tracks[trackIndex].mute = false;

    for (uint8_t stepIndex = 0; stepIndex < sequencerStepCount; stepIndex++)
    {
      pattern.tracks[trackIndex].steps[stepIndex].trigger = false;
      pattern.tracks[trackIndex].steps[stepIndex].velocity = 255;
    }
  }

  //-- Kick: 1, 5, 9, 13
  pattern.tracks[0].steps[0].trigger = true;
  pattern.tracks[0].steps[4].trigger = true;
  pattern.tracks[0].steps[8].trigger = true;
  pattern.tracks[0].steps[12].trigger = true;

  //-- Snare: 5, 13
  pattern.tracks[1].steps[4].trigger = true;
  pattern.tracks[1].steps[12].trigger = true;

  //-- Closed hat: every even step.
  for (uint8_t stepIndex = 0; stepIndex < sequencerStepCount; stepIndex += 2)
  {
    pattern.tracks[2].steps[stepIndex].trigger = true;
    pattern.tracks[2].steps[stepIndex].velocity = 180;
  }

  //-- Open hat: step 12.
  pattern.tracks[3].steps[11].trigger = true;
  pattern.tracks[3].steps[11].velocity = 220;

} //   loadDefaultPattern()

//-- Convert BPM and swing into next step interval in microseconds.
static uint32_t getStepIntervalUs(uint16_t bpm, uint8_t swingPercent, uint8_t stepIndex)
{
  uint32_t baseIntervalUs = 60000000UL / (static_cast<uint32_t>(bpm) * 4UL);
  int32_t swingOffsetUs = (static_cast<int32_t>(baseIntervalUs) * static_cast<int32_t>(swingPercent)) / 100;

  if ((stepIndex & 1U) == 0)
  {
    int32_t earlyIntervalUs = static_cast<int32_t>(baseIntervalUs) - swingOffsetUs;

    if (earlyIntervalUs < 1000)
    {
      earlyIntervalUs = 1000;
    }

    return static_cast<uint32_t>(earlyIntervalUs);
  }

  return static_cast<uint32_t>(static_cast<int32_t>(baseIntervalUs) + swingOffsetUs);

} //   getStepIntervalUs()

//-- Initialize deterministic sequencer state.
void sequencerInit()
{
  portENTER_CRITICAL(&sequencerMux);

  state.bpm = 120;
  state.swingPercent = 8;
  state.currentStep = 0;
  state.cursorStep = 0;
  state.selectedTrack = 0;
  state.activePatternIndex = 0;
  state.playing = false;
  state.shiftMode = false;
  state.nextStepDueUs = 0;

  for (uint8_t patternIndex = 0; patternIndex < sequencerPatternCount; patternIndex++)
  {
    loadDefaultPattern(state.patterns[patternIndex]);
  }

  portEXIT_CRITICAL(&sequencerMux);

} //   sequencerInit()

//-- Advance timing from AudioTask clock and return track trigger bitmask.
bool sequencerConsumeDueStep(uint64_t nowUs, uint8_t& outStepIndex, uint8_t& outTrackMask)
{
  bool stepDue = false;
  outTrackMask = 0;
  outStepIndex = 0;

  portENTER_CRITICAL(&sequencerMux);

  if (state.playing)
  {
    if (state.nextStepDueUs == 0)
    {
      state.nextStepDueUs = nowUs;
    }

    if (nowUs >= state.nextStepDueUs)
    {
      Pattern& activePattern = state.patterns[state.activePatternIndex];

      outStepIndex = state.currentStep;
      stepDue = true;

      for (uint8_t trackIndex = 0; trackIndex < sequencerTrackCount; trackIndex++)
      {
        const Track& track = activePattern.tracks[trackIndex];

        if (track.mute)
        {
          continue;
        }

        if (track.steps[state.currentStep].trigger)
        {
          outTrackMask |= static_cast<uint8_t>(1U << trackIndex);
        }
      }

      uint32_t intervalUs = getStepIntervalUs(state.bpm, state.swingPercent, state.currentStep);
      state.nextStepDueUs += intervalUs;
      state.currentStep = static_cast<uint8_t>((state.currentStep + 1U) % sequencerStepCount);
    }
  }

  portEXIT_CRITICAL(&sequencerMux);

  return stepDue;

} //   sequencerConsumeDueStep()

//-- Toggle sequencer transport state.
void sequencerTogglePlay()
{
  portENTER_CRITICAL(&sequencerMux);

  state.playing = !state.playing;

  if (state.playing)
  {
    state.nextStepDueUs = 0;
  }

  portEXIT_CRITICAL(&sequencerMux);

} //   sequencerTogglePlay()

//-- Toggle shift modifier state.
void sequencerToggleShiftMode()
{
  portENTER_CRITICAL(&sequencerMux);
  state.shiftMode = !state.shiftMode;
  portEXIT_CRITICAL(&sequencerMux);

} //   sequencerToggleShiftMode()

//-- Move step cursor.
void sequencerMoveCursor(int delta)
{
  portENTER_CRITICAL(&sequencerMux);

  int32_t nextStep = static_cast<int32_t>(state.cursorStep) + delta;

  while (nextStep < 0)
  {
    nextStep += sequencerStepCount;
  }

  state.cursorStep = static_cast<uint8_t>(nextStep % sequencerStepCount);

  portEXIT_CRITICAL(&sequencerMux);

} //   sequencerMoveCursor()

//-- Move active track selection.
void sequencerMoveTrack(int delta)
{
  portENTER_CRITICAL(&sequencerMux);

  int32_t nextTrack = static_cast<int32_t>(state.selectedTrack) + delta;

  while (nextTrack < 0)
  {
    nextTrack += sequencerTrackCount;
  }

  state.selectedTrack = static_cast<uint8_t>(nextTrack % sequencerTrackCount);

  portEXIT_CRITICAL(&sequencerMux);

} //   sequencerMoveTrack()

//-- Toggle trigger state at selected track and cursor step.
void sequencerToggleCurrentStep()
{
  portENTER_CRITICAL(&sequencerMux);

  Pattern& activePattern = state.patterns[state.activePatternIndex];
  Step& selectedStep = activePattern.tracks[state.selectedTrack].steps[state.cursorStep];

  selectedStep.trigger = !selectedStep.trigger;

  portEXIT_CRITICAL(&sequencerMux);

} //   sequencerToggleCurrentStep()

//-- Adjust BPM in safe realtime range.
void sequencerAdjustBpm(int delta)
{
  portENTER_CRITICAL(&sequencerMux);

  int32_t nextBpm = static_cast<int32_t>(state.bpm) + delta;

  if (nextBpm < 40)
  {
    nextBpm = 40;
  }
  else if (nextBpm > 260)
  {
    nextBpm = 260;
  }

  state.bpm = static_cast<uint16_t>(nextBpm);

  portEXIT_CRITICAL(&sequencerMux);

} //   sequencerAdjustBpm()

//-- Adjust swing amount.
void sequencerAdjustSwing(int delta)
{
  portENTER_CRITICAL(&sequencerMux);

  int32_t nextSwing = static_cast<int32_t>(state.swingPercent) + delta;

  if (nextSwing < 0)
  {
    nextSwing = 0;
  }
  else if (nextSwing > 45)
  {
    nextSwing = 45;
  }

  state.swingPercent = static_cast<uint8_t>(nextSwing);

  portEXIT_CRITICAL(&sequencerMux);

} //   sequencerAdjustSwing()

//-- Toggle mute state for selected track.
void sequencerToggleMuteForSelectedTrack()
{
  portENTER_CRITICAL(&sequencerMux);
  Pattern& activePattern = state.patterns[state.activePatternIndex];
  activePattern.tracks[state.selectedTrack].mute = !activePattern.tracks[state.selectedTrack].mute;
  portEXIT_CRITICAL(&sequencerMux);

} //   sequencerToggleMuteForSelectedTrack()

//-- Copy current pattern into memory slot.
void sequencerStorePattern(uint8_t slotIndex)
{
  if (slotIndex >= sequencerPatternCount)
  {
    return;
  }

  portENTER_CRITICAL(&sequencerMux);
  state.patterns[slotIndex] = state.patterns[state.activePatternIndex];
  state.activePatternIndex = slotIndex;
  portEXIT_CRITICAL(&sequencerMux);

} //   sequencerStorePattern()

//-- Load pattern memory slot.
void sequencerLoadPattern(uint8_t slotIndex)
{
  if (slotIndex >= sequencerPatternCount)
  {
    return;
  }

  portENTER_CRITICAL(&sequencerMux);
  state.activePatternIndex = slotIndex;
  portEXIT_CRITICAL(&sequencerMux);

} //   sequencerLoadPattern()

//-- Export active sequence data for storage.
void sequencerExportSequence(SequenceData& outData)
{
  portENTER_CRITICAL(&sequencerMux);

  outData.pattern = state.patterns[state.activePatternIndex];
  outData.bpm = state.bpm;
  outData.swingPercent = state.swingPercent;

  portEXIT_CRITICAL(&sequencerMux);

} //   sequencerExportSequence()

//-- Import sequence data into active pattern slot.
void sequencerImportSequence(const SequenceData& sequenceData)
{
  portENTER_CRITICAL(&sequencerMux);

  state.patterns[state.activePatternIndex] = sequenceData.pattern;
  state.bpm = sequenceData.bpm;
  state.swingPercent = sequenceData.swingPercent;
  state.currentStep = 0;
  state.cursorStep = 0;
  state.nextStepDueUs = 0;

  portEXIT_CRITICAL(&sequencerMux);

} //   sequencerImportSequence()

//-- Reset active pattern to an empty sequence.
void sequencerClearActivePattern()
{
  portENTER_CRITICAL(&sequencerMux);

  Pattern& activePattern = state.patterns[state.activePatternIndex];

  for (uint8_t trackIndex = 0; trackIndex < sequencerTrackCount; trackIndex++)
  {
    activePattern.tracks[trackIndex].mute = false;

    for (uint8_t stepIndex = 0; stepIndex < sequencerStepCount; stepIndex++)
    {
      activePattern.tracks[trackIndex].steps[stepIndex].trigger = false;
      activePattern.tracks[trackIndex].steps[stepIndex].velocity = 255;
    }
  }

  state.currentStep = 0;
  state.cursorStep = 0;
  state.nextStepDueUs = 0;

  portEXIT_CRITICAL(&sequencerMux);

} //   sequencerClearActivePattern()

//-- Build a thread-safe UI snapshot.
void sequencerGetView(SequencerView& outView)
{
  portENTER_CRITICAL(&sequencerMux);

  outView.pattern = state.patterns[state.activePatternIndex];
  outView.bpm = state.bpm;
  outView.swingPercent = state.swingPercent;
  outView.currentStep = state.currentStep;
  outView.selectedTrack = state.selectedTrack;
  outView.cursorStep = state.cursorStep;
  outView.activePatternIndex = state.activePatternIndex;
  outView.playing = state.playing;
  outView.shiftMode = state.shiftMode;

  portEXIT_CRITICAL(&sequencerMux);

} //   sequencerGetView()