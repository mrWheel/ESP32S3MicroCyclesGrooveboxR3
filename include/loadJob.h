/*** Last Changed: 2026-06-27 - 16:18 ***/
#pragma once

#include <Arduino.h>

enum class LoadJobType : uint8_t
{
  none = 0,
  loadGroup,
  saveGroup,
  copyGroup,
  loadSampleSet
};

struct LoadJobState
{
  bool active;
  bool finished;
  bool failed;
  LoadJobType type;
  char title[48];
  char name[32];
  char targetName[32];
  char error[80];
};

//-- This function starts a pattern group load job.
bool loadJobStartLoadGroup(const String& groupName);

//-- This function starts a pattern group save job.
bool loadJobStartSaveGroup();

//-- This function starts a pattern group copy job.
bool loadJobStartCopyGroup(const String& sourceGroupName, const String& targetGroupName);

//-- This function starts a sample set load job.
bool loadJobStartLoadSampleSet(const String& sampleSetName);

//-- This function returns the current load job state.
void loadJobGetState(LoadJobState& state);

//-- This function clears a finished load job.
void loadJobClearFinished();

//-- This function returns whether a load job is active.
bool loadJobIsActive();
