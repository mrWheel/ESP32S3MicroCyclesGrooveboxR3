/*** Last Changed: 2026-06-27 - 16:18 ***/
#include "loadJob.h"

#include "audioEngine.h"
#include "loadStatus.h"
#include "sampleManager.h"
#include "settingsStore.h"
#include "sequencer.h"
#include "uiManager.h"

#include <string.h>

static portMUX_TYPE loadJobMux = portMUX_INITIALIZER_UNLOCKED;
static LoadJobState loadJobState = {};
static TaskHandle_t loadJobTaskHandle = nullptr;

//-- This function copies text into a fixed buffer.
static void copyText(char* destination, size_t destinationSize, const char* source)
{
  if (!destination || destinationSize == 0)
  {
    return;
  }

  strncpy(destination, source ? source : "", destinationSize - 1);
  destination[destinationSize - 1] = '\0';

} //   copyText()

//-- This function stores the final load job result.
static void finishLoadJob(bool failed, const char* errorText)
{
  portENTER_CRITICAL(&loadJobMux);

  loadJobState.active = false;
  loadJobState.finished = true;
  loadJobState.failed = failed;
  copyText(loadJobState.error, sizeof(loadJobState.error), errorText);

  portEXIT_CRITICAL(&loadJobMux);

  if (failed)
  {
    loadStatusFail(errorText);
  }
  else
  {
    loadStatusFinish();
  }

} //   finishLoadJob()

//-- This function runs the active load job in a worker task.
static void runLoadJobTask(void* parameter)
{
  (void)parameter;

  LoadJobState job = {};

  portENTER_CRITICAL(&loadJobMux);
  job = loadJobState;
  portEXIT_CRITICAL(&loadJobMux);

  bool ok = false;

  sequencerStopImmediately();
  audioEngineStopAllVoices();

  if (job.type == LoadJobType::loadGroup)
  {
    ok = uiManagerLoadPatternGroup(String(job.name));

    if (ok)
    {
      uiManagerReturnToGrooveboxScreen();
    }

    finishLoadJob(!ok, ok ? "" : "Failed to load group");
  }
  else if (job.type == LoadJobType::saveGroup)
  {
    loadStatusStart("Saving Group", 1);
    loadStatusUpdate(1, "Writing SD");

    ok = uiManagerSavePatternGroup();

    if (ok)
    {
      uiManagerReturnToGrooveboxScreen();
    }

    finishLoadJob(!ok, ok ? "" : "Failed to save group");
  }
  else if (job.type == LoadJobType::copyGroup)
  {
    ok = settingsStoreCopyPatternGroupOnCard(String(job.name), String(job.targetName));

    finishLoadJob(!ok, ok ? "" : "Failed to copy group");
  }
  else if (job.type == LoadJobType::loadSampleSet)
  {
    ok = sampleManagerLoadSampleSet(job.name);

    if (ok)
    {
      if (!settingsStoreSetActiveSampleSet(String(job.name)))
      {
        ok = false;
      }
    }

    if (ok)
    {
      uiManagerRequestRedraw();
    }

    finishLoadJob(!ok, ok ? "" : "Failed to load sample set");
  }
  else
  {
    finishLoadJob(true, "Invalid load job");
  }

  portENTER_CRITICAL(&loadJobMux);
  loadJobTaskHandle = nullptr;
  portEXIT_CRITICAL(&loadJobMux);

  vTaskDelete(nullptr);

} //   runLoadJobTask()

//-- This function starts a load job worker task.
static bool startLoadJob(LoadJobType type, const char* title, const char* name,
                         const char* targetName = "")
{
  portENTER_CRITICAL(&loadJobMux);

  if (loadJobState.active)
  {
    portEXIT_CRITICAL(&loadJobMux);
    return false;
  }

  loadJobState.active = true;
  loadJobState.finished = false;
  loadJobState.failed = false;
  loadJobState.type = type;
  copyText(loadJobState.title, sizeof(loadJobState.title), title);
  copyText(loadJobState.name, sizeof(loadJobState.name), name);
  copyText(loadJobState.targetName, sizeof(loadJobState.targetName), targetName);
  loadJobState.error[0] = '\0';

  portEXIT_CRITICAL(&loadJobMux);

  loadStatusClear();
  loadStatusStart(title, 0);

  BaseType_t created =
      xTaskCreatePinnedToCore(runLoadJobTask, "LoadJob", 12288, nullptr, 1, &loadJobTaskHandle, 1);

  if (created != pdPASS)
  {
    finishLoadJob(true, "Failed to create load job");
    return false;
  }

  return true;

} //   startLoadJob()

//-- This function starts a pattern group load job.
bool loadJobStartLoadGroup(const String& groupName)
{
  String title = String("Loading Group ") + groupName;

  return startLoadJob(LoadJobType::loadGroup, title.c_str(), groupName.c_str());

} //   loadJobStartLoadGroup()

//-- This function starts a pattern group save job.
bool loadJobStartSaveGroup()
{
  return startLoadJob(LoadJobType::saveGroup, "Saving Group", "");

} //   loadJobStartSaveGroup()

//-- This function starts a pattern group copy job.
bool loadJobStartCopyGroup(const String& sourceGroupName, const String& targetGroupName)
{
  String title = String("Copy Group ") + sourceGroupName + " -> " + targetGroupName;

  return startLoadJob(LoadJobType::copyGroup, title.c_str(), sourceGroupName.c_str(),
                      targetGroupName.c_str());

} //   loadJobStartCopyGroup()

//-- This function starts a sample set load job.
bool loadJobStartLoadSampleSet(const String& sampleSetName)
{
  String title = String("Loading Sample Set ") + sampleSetName;

  return startLoadJob(LoadJobType::loadSampleSet, title.c_str(), sampleSetName.c_str());

} //   loadJobStartLoadSampleSet()

//-- This function returns the current load job state.
void loadJobGetState(LoadJobState& state)
{
  portENTER_CRITICAL(&loadJobMux);
  state = loadJobState;
  portEXIT_CRITICAL(&loadJobMux);

} //   loadJobGetState()

//-- This function clears a finished load job.
void loadJobClearFinished()
{
  portENTER_CRITICAL(&loadJobMux);

  if (!loadJobState.active)
  {
    loadJobState.finished = false;
    loadJobState.failed = false;
    loadJobState.type = LoadJobType::none;
    loadJobState.title[0] = '\0';
    loadJobState.name[0] = '\0';
    loadJobState.error[0] = '\0';
  }

  portEXIT_CRITICAL(&loadJobMux);

} //   loadJobClearFinished()

//-- This function returns whether a load job is active.
bool loadJobIsActive()
{
  bool active = false;

  portENTER_CRITICAL(&loadJobMux);
  active = loadJobState.active;
  portEXIT_CRITICAL(&loadJobMux);

  return active;

} //   loadJobIsActive()
