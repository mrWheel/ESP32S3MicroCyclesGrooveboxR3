/*** Last Changed: 2026-06-26 - 16:38 ***/
#include "webApi.h"
#include "sequencer.h"
#include "settingsStore.h"
#include "sampleManager.h"
#include "audioEngine.h"
#include "uiManager.h"
#include "systemManager.h"
#include "webServerManager.h"
#include "progVersion.h"
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <WebServer.h>
#include <uri/UriRegex.h>

extern WebServer webServer;

//-- Helper: send JSON response with optional HTTP status code
static void sendJson(WebServer& server, JsonDocument& doc, int code = 200)
{
  String jsonStr;
  serializeJson(doc, jsonStr);
  webServer.send(code, "application/json", jsonStr);
} //   sendJson()

//-- Helper: send simple ok response
static void sendOk(WebServer& server)
{
  JsonDocument doc;
  doc["ok"] = true;
  sendJson(server, doc);
} //   sendOk()

//-- Helper: send error response
static void sendError(WebServer& server, const char* message, int code = 400)
{
  JsonDocument doc;
  doc["ok"] = false;
  doc["error"] = message;
  sendJson(server, doc, code);
} //   sendError()

//-- Helper: convert slot index to pattern name.
static String slotIndexToPatternName(uint8_t slotIndex)
{
  char buf[5];
  snprintf(buf, sizeof(buf), "p%02u", slotIndex + 1);
  return String(buf);
} //   slotIndexToPatternName()

//-- Helper: parse pattern name to slot index; returns -1 if invalid.
static int16_t patternNameToSlotIndex(const String& name)
{
  if (name.length() != 3 || name[0] != 'p')
  {
    return -1;
  }
  int num = atoi(name.c_str() + 1);
  if (num < 1 || num > sequencerPatternCount)
  {
    return -1;
  }
  return num - 1;
} //   patternNameToSlotIndex()

//-- Helper: build complete pattern JSON document for a given slot index
static void buildPatternJson(JsonDocument& doc, uint8_t slotIndex)
{
  PatternData patternData;
  sequencerExportPatternFromSlot(slotIndex, patternData);

  doc["ok"] = true;
  doc["name"] = slotIndexToPatternName(slotIndex);
  doc["bpm"] = patternData.bpm;
  doc["swing"] = patternData.swingPercent;
  doc["chainEnabled"] = uiManagerGetPatternChainEnabledForSlot(slotIndex);
  doc["chainLength"] = patternData.chainLength;
  doc["chainTarget"] = uiManagerGetPatternChainTargetForSlot(slotIndex);

  JsonArray tracksArray = doc["tracks"].to<JsonArray>();

  const char* trackNames[] = {"KICK", "SNARE", "CH", "OH", "TONE", "METAL"};
  for (uint8_t trackIdx = 0; trackIdx < 6; trackIdx++)
  {
    JsonObject trackObj = tracksArray.add<JsonObject>();
    trackObj["name"] = trackNames[trackIdx];
    trackObj["mute"] = patternData.pattern.tracks[trackIdx].mute;

    JsonArray stepsArray = trackObj["steps"].to<JsonArray>();
    for (uint8_t stepIdx = 0; stepIdx < 16; stepIdx++)
    {
      const Step& step = patternData.pattern.tracks[trackIdx].steps[stepIdx];

      JsonObject stepObj = stepsArray.add<JsonObject>();
      stepObj["trigger"] = step.trigger;
      stepObj["mute"] = step.mute;
      stepObj["velocity"] = step.velocity;
      stepObj["probability"] = step.probability;
      stepObj["lockEnabled"] = step.lockEnabled;
      stepObj["lockPitch"] = step.lockPitch;
      stepObj["lockDecay"] = step.lockDecay;
    }
  }
} //   buildPatternJson()

//-- Helper: parse pattern JSON from request body and convert to PatternData
static bool parsePatternFromJson(const JsonDocument& doc, PatternData& patternData)
{
  patternData.bpm = static_cast<uint16_t>(doc["bpm"] | 120);
  patternData.swingPercent = static_cast<uint8_t>(doc["swing"] | 8);
  patternData.chainEnabled = static_cast<bool>(doc["chainEnabled"] | false);
  patternData.chainLength = static_cast<uint8_t>(doc["chainLength"] | 1);
  patternData.chainTarget = doc["chainTarget"].as<String>();

  JsonArrayConst tracksArray = doc["tracks"].as<JsonArrayConst>();
  if (tracksArray.size() < 6)
  {
    return false;
  }

  for (uint8_t trackIdx = 0; trackIdx < 6; trackIdx++)
  {
    JsonObjectConst trackObj = tracksArray[trackIdx].as<JsonObjectConst>();
    patternData.pattern.tracks[trackIdx].mute = static_cast<bool>(trackObj["mute"] | false);

    JsonArrayConst stepsArray = trackObj["steps"].as<JsonArrayConst>();
    if (stepsArray.size() < 16)
    {
      return false;
    }

    for (uint8_t stepIdx = 0; stepIdx < 16; stepIdx++)
    {
      JsonObjectConst stepObj = stepsArray[stepIdx].as<JsonObjectConst>();
      patternData.pattern.tracks[trackIdx].steps[stepIdx].trigger =
          static_cast<bool>(stepObj["trigger"] | false);
      patternData.pattern.tracks[trackIdx].steps[stepIdx].mute =
          static_cast<bool>(stepObj["mute"] | false);
      patternData.pattern.tracks[trackIdx].steps[stepIdx].velocity =
          static_cast<uint8_t>(stepObj["velocity"] | 128);
      patternData.pattern.tracks[trackIdx].steps[stepIdx].probability =
          static_cast<uint8_t>(stepObj["probability"] | 100);
      patternData.pattern.tracks[trackIdx].steps[stepIdx].lockEnabled =
          static_cast<bool>(stepObj["lockEnabled"] | false);
      patternData.pattern.tracks[trackIdx].steps[stepIdx].lockPitch =
          static_cast<int8_t>(stepObj["lockPitch"] | 0);
      patternData.pattern.tracks[trackIdx].steps[stepIdx].lockDecay =
          static_cast<uint8_t>(stepObj["lockDecay"] | 100);
    }
  }

  return true;
} //   parsePatternFromJson()

//-- GET /api/status — comprehensive system status
static void handleStatusRequest()
{
  SequencerView view;
  sequencerGetView(view);

  JsonDocument doc;
  doc["ok"] = true;
  doc["version"] = PROG_VERSION;
  doc["ip"] = systemManagerGetIpAddress();
  doc["ssid"] = systemManagerGetSsid();
  doc["wifiConnected"] = !systemManagerGetSsid().isEmpty();
  doc["webServerRunning"] = webServerManagerIsRunning();

  doc["activeGroup"] = settingsStoreGetActivePatternGroup();
  doc["activePattern"] = slotIndexToPatternName(view.activePatternIndex);
  doc["activeSampleSet"] = sampleManagerGetActiveSampleSet();

  doc["playing"] = view.playing;
  doc["paused"] = view.paused;
  doc["editMode"] = view.editMode;
  doc["bpm"] = view.bpm;
  doc["swing"] = view.swingPercent;
  doc["currentStep"] = view.currentStep;
  doc["activePatternIndex"] = view.activePatternIndex;
  doc["playingPatternIndex"] = view.playingPatternIndex;
  doc["selectedTrack"] = view.selectedTrack;
  doc["selectedStep"] = view.cursorStep;
  doc["patternGroupDirty"] = uiManagerIsPatternGroupDirty();

  doc["sdCardInserted"] = sampleManagerIsSdCardInserted();
  doc["sdCardReady"] = sampleManagerIsSdCardReady();

  uint32_t psramFreeBefore = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  uint32_t psramTotal = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
  doc["psramAvailable"] = (psramTotal > 0);
  doc["freePsram"] = psramFreeBefore;
  doc["freeHeap"] = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);

  String jsonStr;
  serializeJson(doc, jsonStr);
  webServer.send(200, "application/json", jsonStr);
} //   handleStatusRequest()

//-- GET /api/transport — transport state snapshot
static void handleTransportRequest()
{
  SequencerView view;
  sequencerGetView(view);

  JsonDocument doc;
  doc["ok"] = true;
  doc["bpm"] = view.bpm;
  doc["swing"] = view.swingPercent;
  doc["playing"] = view.playing;
  doc["paused"] = view.paused;
  doc["editMode"] = view.editMode;
  doc["chainEnabled"] = view.chainEnabled;
  doc["chainLength"] = view.chainLength;
  doc["currentStep"] = view.currentStep;
  doc["activePatternIndex"] = view.activePatternIndex;
  doc["playingPatternIndex"] = view.playingPatternIndex;

  String jsonStr;
  serializeJson(doc, jsonStr);
  webServer.send(200, "application/json", jsonStr);
} //   handleTransportRequest()

//-- POST /api/transport/play — start transport
static void handleTransportPlayRequest()
{
  SequencerView view;

  sequencerGetView(view);

  if (view.paused)
  {
    sequencerResumePlayback();
  }
  else if (!view.playing)
  {
    sequencerStartFromActivePattern();
  }

  sendOk(webServer);

} //   handleTransportPlayRequest()

//-- POST /api/transport/stop — hard stop
static void handleTransportStopRequest()
{
  sequencerStopImmediately();
  sendOk(webServer);

} //   handleTransportStopRequest()

//-- POST /api/transport/pause — pause transport
static void handleTransportPauseRequest()
{
  sequencerPausePlayback();
  sendOk(webServer);

} //   handleTransportPauseRequest()

//-- POST /api/transport/continue — continue paused transport
static void handleTransportContinueRequest()
{
  sequencerResumePlayback();
  sendOk(webServer);

} //   handleTransportContinueRequest()

//-- POST /api/transport/toggle — toggle play/stop
static void handleTransportToggleRequest()
{
  sequencerTogglePlay();
  sendOk(webServer);

} //   handleTransportToggleRequest()

//-- POST /api/transport/bpm — set BPM
static void handleTransportBpmRequest()
{
  if (!webServer.hasArg("plain"))
  {
    sendError(webServer, "Missing body");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, webServer.arg("plain"));

  if (error || !doc["bpm"].is<uint16_t>())
  {
    sendError(webServer, "Invalid JSON or missing bpm");
    return;
  }

  uint16_t targetBpm = doc["bpm"];
  SequencerView view;

  sequencerGetView(view);

  int delta = static_cast<int>(targetBpm) - static_cast<int>(view.bpm);

  if (delta != 0)
  {
    sequencerAdjustBpm(delta);
    uiManagerRequestRedraw();
  }

  sendOk(webServer);

} //   handleTransportBpmRequest()

//-- POST /api/transport/swing — set swing
static void handleTransportSwingRequest()
{
  if (!webServer.hasArg("plain"))
  {
    sendError(webServer, "Missing body");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, webServer.arg("plain"));

  if (error || !doc["swing"].is<uint8_t>())
  {
    sendError(webServer, "Invalid JSON or missing swing");
    return;
  }

  uint8_t targetSwing = doc["swing"];
  SequencerView view;

  sequencerGetView(view);

  int delta = static_cast<int>(targetSwing) - static_cast<int>(view.swingPercent);

  if (delta != 0)
  {
    sequencerAdjustSwing(delta);
    uiManagerRequestRedraw();
  }

  sendOk(webServer);

} //   handleTransportSwingRequest()

//-- GET /api/groups — list all pattern groups
static void handleGroupsListRequest()
{

  JsonDocument doc;
  doc["ok"] = true;
  doc["activeGroup"] = settingsStoreGetActivePatternGroup();

  String groupNames[32];
  size_t groupCount = 0;
  if (!settingsStoreListPatternGroupsOnCard(groupNames, 32, groupCount))
  {
    sendError(webServer, "Failed to list groups");
    return;
  }

  JsonArray groupsArray = doc["groups"].to<JsonArray>();
  for (size_t i = 0; i < groupCount; i++)
  {
    groupsArray.add(groupNames[i]);
  }

  sendJson(webServer, doc);

} //   handleGroupsListRequest()

//-- GET /api/groups/active — get active group info
static void handleGroupsActiveRequest()
{

  JsonDocument doc;
  doc["ok"] = true;
  doc["name"] = settingsStoreGetActivePatternGroup();
  doc["patternCount"] = uiManagerGetLoadedPatternCount();

  sendJson(webServer, doc);

} //   handleGroupsActiveRequest()

//-- POST /api/groups/load — load a pattern group.
static void handleGroupsLoadRequest()
{
  if (!webServer.hasArg("plain"))
  {
    sendError(webServer, "Missing body");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, webServer.arg("plain"));

  if (error)
  {
    sendError(webServer, "Invalid JSON");
    return;
  }

  String groupName = doc["groupName"] | "";

  if (groupName.isEmpty())
  {
    groupName = doc["name"] | "";
  }

  if (groupName.isEmpty())
  {
    sendError(webServer, "Missing groupName");
    return;
  }

  if (!sampleManagerIsSdCardInserted())
  {
    sendError(webServer, "No SD card", 409);
    return;
  }

  if (!sampleManagerIsSdCardReady())
  {
    sendError(webServer, "SD card not ready", 503);
    return;
  }

  if (!uiManagerLoadPatternGroup(groupName))
  {
    sendError(webServer, "Failed to load group");
    return;
  }

  JsonDocument response;
  response["ok"] = true;
  response["groupName"] = groupName;
  response["patternCount"] = uiManagerGetLoadedPatternCount();

  uiManagerReturnToGrooveboxScreen();

  sendJson(webServer, response);

} //   handleGroupsLoadRequest()

//-- POST /api/groups/save — save active pattern group to SD
static void handleGroupsSaveRequest()
{

  if (!sampleManagerIsSdCardReady())
  {
    sendError(webServer, "SD card not ready", 503);
    return;
  }

  sequencerStopImmediately();

  if (!uiManagerSavePatternGroup())
  {
    sendError(webServer, "Failed to save group");
    return;
  }

  uiManagerReturnToGrooveboxScreen();

  sendOk(webServer);

} //   handleGroupsSaveRequest()

//-- POST /api/groups/new — create new pattern group
static void handleGroupsNewRequest()
{
  if (!webServer.hasArg("plain"))
  {
    sendError(webServer, "Missing body");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, webServer.arg("plain"));

  if (error || !doc["name"].is<const char*>())
  {
    sendError(webServer, "Invalid JSON or missing name");
    return;
  }

  String groupName = doc["name"].as<String>();
  String activeGroup = settingsStoreGetActivePatternGroup();

  if (!sampleManagerIsSdCardReady())
  {
    sendError(webServer, "SD card not ready", 503);
    return;
  }

  if (activeGroup.isEmpty())
  {
    sendError(webServer, "No active group to copy");
    return;
  }

  if (!settingsStoreCopyPatternGroupOnCard(activeGroup, groupName))
  {
    sendError(webServer, "Failed to create group");
    return;
  }

  if (!uiManagerLoadPatternGroup(groupName))
  {
    sendError(webServer, "Failed to load new group");
    return;
  }

  uiManagerReturnToGrooveboxScreen();

  sendOk(webServer);

} //   handleGroupsNewRequest()

//-- POST /api/groups/rename — rename pattern group
static void handleGroupsRenameRequest()
{

  if (!webServer.hasArg("plain"))
  {
    sendError(webServer, "Missing body");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, webServer.arg("plain"));
  if (error || !doc["from"].is<const char*>() || !doc["to"].is<const char*>())
  {
    sendError(webServer, "Missing 'from' or 'to'");
    return;
  }

  String fromName = doc["from"].as<String>();
  String toName = doc["to"].as<String>();

  if (!sampleManagerIsSdCardReady())
  {
    sendError(webServer, "SD card not ready", 503);
    return;
  }

  if (!settingsStoreRenamePatternGroupOnCard(fromName, toName))
  {
    sendError(webServer, "Failed to rename group");
    return;
  }

  String activeGroup = settingsStoreGetActivePatternGroup();
  if (activeGroup == fromName)
  {
    settingsStoreSetActivePatternGroup(toName);
  }

  uiManagerReturnToGrooveboxScreen();

  sendOk(webServer);

} //   handleGroupsRenameRequest()

//-- POST /api/groups/copy — copy pattern group
static void handleGroupsCopyRequest()
{

  if (!webServer.hasArg("plain"))
  {
    sendError(webServer, "Missing body");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, webServer.arg("plain"));
  if (error || !doc["from"].is<const char*>() || !doc["to"].is<const char*>())
  {
    sendError(webServer, "Missing 'from' or 'to'");
    return;
  }

  String fromName = doc["from"].as<String>();
  String toName = doc["to"].as<String>();

  if (!sampleManagerIsSdCardReady())
  {
    sendError(webServer, "SD card not ready", 503);
    return;
  }

  if (!settingsStoreCopyPatternGroupOnCard(fromName, toName))
  {
    sendError(webServer, "Failed to copy group");
    return;
  }

  uiManagerReturnToGrooveboxScreen();

  sendOk(webServer);

} //   handleGroupsCopyRequest()

//-- POST /api/groups/delete — delete pattern group
static void handleGroupsDeleteRequest()
{

  if (!webServer.hasArg("plain"))
  {
    sendError(webServer, "Missing body");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, webServer.arg("plain"));
  if (error || !doc["name"].is<const char*>())
  {
    sendError(webServer, "Missing name");
    return;
  }

  String groupName = doc["name"].as<String>();

  String activeGroup = settingsStoreGetActivePatternGroup();
  if (groupName == activeGroup)
  {
    sendError(webServer, "Cannot delete active group");
    return;
  }

  if (!sampleManagerIsSdCardReady())
  {
    sendError(webServer, "SD card not ready", 503);
    return;
  }

  if (!settingsStoreDeletePatternGroupFromCard(groupName))
  {
    sendError(webServer, "Failed to delete group");
    return;
  }

  uiManagerReturnToGrooveboxScreen();

  sendOk(webServer);

} //   handleGroupsDeleteRequest()

//-- GET /api/patterns — list all loaded patterns
static void handlePatternsListRequest()
{

  JsonDocument doc;
  doc["ok"] = true;
  doc["activePatternIndex"] = 0; // Placeholder: would need sequencerGetView()

  SequencerView view;
  sequencerGetView(view);
  doc["activePatternIndex"] = view.activePatternIndex;

  uint8_t loadedCount = uiManagerGetLoadedPatternCount();
  JsonArray patternsArray = doc["patterns"].to<JsonArray>();

  for (uint8_t i = 0; i < loadedCount; i++)
  {
    JsonObject patObj = patternsArray.add<JsonObject>();
    patObj["name"] = slotIndexToPatternName(i);
    patObj["slotIndex"] = i;
  }

  sendJson(webServer, doc);

} //   handlePatternsListRequest()

//-- GET /api/patterns/active — get active pattern full data
static void handlePatternsActiveGetRequest()
{

  SequencerView view;
  sequencerGetView(view);

  JsonDocument doc;
  buildPatternJson(doc, view.activePatternIndex);

  sendJson(webServer, doc);

} //   handlePatternsActiveGetRequest()

//-- POST /api/patterns/active — set active pattern
static void handlePatternsActiveSetRequest()
{

  if (!webServer.hasArg("plain"))
  {
    sendError(webServer, "Missing body");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, webServer.arg("plain"));
  if (error || !doc["name"].is<const char*>())
  {
    sendError(webServer, "Missing name");
    return;
  }

  String patternName = doc["name"].as<String>();
  int16_t slotIndex = patternNameToSlotIndex(patternName);

  if (slotIndex < 0 || slotIndex >= uiManagerGetLoadedPatternCount())
  {
    sendError(webServer, "Invalid pattern name");
    return;
  }

  SequencerView view;
  sequencerGetView(view);

  if (view.playing)
  {
    sequencerRequestPatternSwitchAfterCurrentPattern((uint8_t)slotIndex);
  }
  else
  {
    sequencerSetActivePatternIndex((uint8_t)slotIndex);
  }

  sendOk(webServer);

} //   handlePatternsActiveSetRequest()

//-- GET /api/patterns/{patternName} — get full pattern data
static void handlePatternsGetRequest()
{

  String patternName = webServer.pathArg(0);
  int16_t slotIndex = patternNameToSlotIndex(patternName);

  if (slotIndex < 0 || slotIndex >= uiManagerGetLoadedPatternCount())
  {
    sendError(webServer, "Invalid pattern name", 404);
    return;
  }

  JsonDocument doc;
  buildPatternJson(doc, (uint8_t)slotIndex);
  sendJson(webServer, doc);

} //   handlePatternsGetRequest()

//-- PUT /api/patterns/{patternName} — replace entire pattern
static void handlePatternsPutRequest()
{

  String patternName = webServer.pathArg(0);
  int16_t slotIndex = patternNameToSlotIndex(patternName);

  if (slotIndex < 0 || slotIndex >= uiManagerGetLoadedPatternCount())
  {
    sendError(webServer, "Invalid pattern name", 404);
    return;
  }

  if (!webServer.hasArg("plain"))
  {
    sendError(webServer, "Missing body");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, webServer.arg("plain"));
  if (error)
  {
    sendError(webServer, "Invalid JSON");
    return;
  }

  PatternData patternData;
  if (!parsePatternFromJson(doc, patternData))
  {
    sendError(webServer, "Invalid pattern data");
    return;
  }

  sequencerImportPatternToSlot((uint8_t)slotIndex, patternData);
  uiManagerSetPatternGroupDirty(true);

  sendOk(webServer);

} //   handlePatternsPutRequest()

//-- POST /api/patterns/{patternName}/clear — clear all steps in pattern
static void handlePatternsClearRequest()
{

  String patternName = webServer.pathArg(0);
  int16_t slotIndex = patternNameToSlotIndex(patternName);

  if (slotIndex < 0 || slotIndex >= uiManagerGetLoadedPatternCount())
  {
    sendError(webServer, "Invalid pattern name", 404);
    return;
  }

  sequencerSetActivePatternIndex((uint8_t)slotIndex);
  sequencerClearActivePattern();
  uiManagerSetPatternGroupDirty(true);

  sendOk(webServer);
} //   handlePatternsClearRequest()

//-- POST /api/patterns/{patternName}/copy — copy pattern to another slot
static void handlePatternsCopyRequest()
{

  String patternName = webServer.pathArg(0);
  int16_t sourceSlot = patternNameToSlotIndex(patternName);

  if (sourceSlot < 0 || sourceSlot >= uiManagerGetLoadedPatternCount())
  {
    sendError(webServer, "Invalid source pattern", 404);
    return;
  }

  if (!webServer.hasArg("plain"))
  {
    sendError(webServer, "Missing body");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, webServer.arg("plain"));
  if (error || !doc["to"].is<const char*>())
  {
    sendError(webServer, "Missing 'to' in body");
    return;
  }

  String targetName = doc["to"].as<String>();
  int16_t targetSlot = patternNameToSlotIndex(targetName);

  if (targetSlot < 0 || targetSlot >= uiManagerGetLoadedPatternCount())
  {
    sendError(webServer, "Invalid target pattern");
    return;
  }

  PatternData patternData;
  sequencerExportPatternFromSlot((uint8_t)sourceSlot, patternData);
  sequencerImportPatternToSlot((uint8_t)targetSlot, patternData);

  uiManagerSetPatternGroupDirty(true);
  sendOk(webServer);

} //   handlePatternsCopyRequest()

//-- PUT /api/patterns/{patternName}/tracks/{trackIndex}/steps/{stepIndex}
static void handleStepEditRequest()
{
  String patternName = webServer.pathArg(0);
  uint8_t trackIndex = webServer.pathArg(1).toInt();
  uint8_t stepIndex = webServer.pathArg(2).toInt();

  if (trackIndex >= 6 || stepIndex >= 16)
  {
    sendError(webServer, "Invalid track or step index");
    return;
  }

  int16_t slotIndex = patternNameToSlotIndex(patternName);

  if (slotIndex < 0 || slotIndex >= uiManagerGetLoadedPatternCount())
  {
    sendError(webServer, "Invalid pattern name", 404);
    return;
  }

  if (!webServer.hasArg("plain"))
  {
    sendError(webServer, "Missing body");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, webServer.arg("plain"));

  if (error)
  {
    sendError(webServer, "Invalid JSON");
    return;
  }

  SequencerView view;
  PatternData patternData;

  sequencerGetView(view);
  sequencerExportPatternFromSlot((uint8_t)slotIndex, patternData);

  if (slotIndex == view.activePatternIndex)
  {
    patternData.chainEnabled = view.chainEnabled;
    patternData.chainLength = view.chainLength;
  }

  Step& step = patternData.pattern.tracks[trackIndex].steps[stepIndex];

  if (doc["trigger"].is<bool>())
  {
    step.trigger = doc["trigger"];
  }

  if (doc["mute"].is<bool>())
  {
    step.mute = doc["mute"];
  }

  if (doc["velocity"].is<uint8_t>())
  {
    step.velocity = doc["velocity"];
  }

  if (doc["probability"].is<uint8_t>())
  {
    step.probability = doc["probability"];
  }

  if (doc["lockEnabled"].is<bool>())
  {
    step.lockEnabled = doc["lockEnabled"];
  }

  if (doc["lockPitch"].is<int8_t>())
  {
    step.lockPitch = doc["lockPitch"];
  }

  if (doc["lockDecay"].is<uint8_t>())
  {
    step.lockDecay = doc["lockDecay"];
  }

  sequencerImportPatternToSlot((uint8_t)slotIndex, patternData);
  uiManagerSetPatternGroupDirty(true);

  sendOk(webServer);

} //   handleStepEditRequest()

//-- PUT /api/patterns/{patternName}/tracks/{trackIndex} — update track
static void handleTrackEditRequest()
{

  String patternName = webServer.pathArg(0);
  uint8_t trackIndex = webServer.pathArg(1).toInt();

  if (trackIndex >= 6)
  {
    sendError(webServer, "Invalid track index");
    return;
  }

  int16_t slotIndex = patternNameToSlotIndex(patternName);
  if (slotIndex < 0 || slotIndex >= uiManagerGetLoadedPatternCount())
  {
    sendError(webServer, "Invalid pattern name", 404);
    return;
  }

  if (!webServer.hasArg("plain"))
  {
    sendError(webServer, "Missing body");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, webServer.arg("plain"));
  if (error)
  {
    sendError(webServer, "Invalid JSON");
    return;
  }

  PatternData patternData;
  sequencerExportPatternFromSlot((uint8_t)slotIndex, patternData);

  if (doc["mute"].is<bool>())
  {
    patternData.pattern.tracks[trackIndex].mute = doc["mute"];
  }

  sequencerImportPatternToSlot((uint8_t)slotIndex, patternData);
  uiManagerSetPatternGroupDirty(true);

  sendOk(webServer);

} //   handleTrackEditRequest()

//-- POST /api/tracks/{trackIndex}/mute-toggle — toggle track mute
static void handleTrackMuteToggleRequest()
{

  uint8_t trackIndex = webServer.pathArg(0).toInt();
  if (trackIndex >= 6)
  {
    sendError(webServer, "Invalid track index");
    return;
  }

  SequencerView view;
  sequencerGetView(view);

  if (view.playing)
  {
    PatternData patternData;
    sequencerExportPatternFromSlot(view.activePatternIndex, patternData);
    patternData.pattern.tracks[trackIndex].mute = !patternData.pattern.tracks[trackIndex].mute;
    sequencerImportPatternToSlot(view.activePatternIndex, patternData);
    uiManagerSetPatternGroupDirty(true);
  }
  else
  {
    uint8_t previousTrack = view.selectedTrack;
    sequencerMoveTrack((int)trackIndex - (int)previousTrack);
    sequencerToggleMuteForSelectedTrack();
    if (previousTrack != trackIndex)
    {
      sequencerMoveTrack((int)previousTrack - (int)trackIndex);
    }
  }

  sendOk(webServer);

} //   handleTrackMuteToggleRequest()

//-- GET /api/sample-sets — list sample sets
static void handleSampleSetsListRequest()
{

  JsonDocument doc;
  doc["ok"] = true;
  doc["activeSampleSet"] = sampleManagerGetActiveSampleSet();

  char sampleSetNames[32][4];
  uint8_t sampleSetCount = 0;
  if (!sampleManagerListSampleSets((char (*)[4])sampleSetNames, 32, &sampleSetCount))
  {
    sendError(webServer, "Failed to list sample sets");
    return;
  }

  JsonArray setsArray = doc["sampleSets"].to<JsonArray>();
  for (uint8_t i = 0; i < sampleSetCount; i++)
  {
    setsArray.add((const char*)sampleSetNames[i]);
  }

  sendJson(webServer, doc);

} //   handleSampleSetsListRequest()

//-- GET /api/sample-sets/active — get active sample set info
static void handleSampleSetsActiveRequest()
{

  JsonDocument doc;
  doc["ok"] = true;
  doc["name"] = sampleManagerGetActiveSampleSet();

  sendJson(webServer, doc);

} //   handleSampleSetsActiveRequest()

//-- POST /api/sample-sets/load — load sample set
static void handleSampleSetsLoadRequest()
{

  if (!webServer.hasArg("plain"))
  {
    sendError(webServer, "Missing body");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, webServer.arg("plain"));
  if (error || !doc["name"].is<const char*>())
  {
    sendError(webServer, "Missing name");
    return;
  }

  String setName = doc["name"].as<String>();

  if (!sampleManagerLoadSampleSet(setName.c_str()))
  {
    sendError(webServer, "Failed to load sample set");
    return;
  }

  if (!settingsStoreSetActiveSampleSet(setName))
  {
    sendError(webServer, "Failed to set active sample set");
    return;
  }

  sendOk(webServer);

} //   handleSampleSetsLoadRequest()

//-- GET /api/samples — get all sample info
static void handleSamplesListRequest()
{

  JsonDocument doc;
  doc["ok"] = true;

  const char* sampleNames[] = {"kick", "snare", "ch", "oh", "tone", "metal"};
  JsonArray samplesArray = doc["samples"].to<JsonArray>();

  for (uint8_t i = 0; i < 6; i++)
  {
    const SampleSlot& slot = sampleManagerGetSample((SampleId)i);
    JsonObject sampleObj = samplesArray.add<JsonObject>();

    sampleObj["index"] = i;
    sampleObj["name"] = sampleNames[i];
    sampleObj["valid"] = slot.valid;
    sampleObj["frameCount"] = slot.frameCount;
    sampleObj["storedInPsram"] = slot.storedInPsram;
    sampleObj["fromSd"] = slot.fromSd;
    sampleObj["gainPercent"] = sampleManagerGetSampleGainPercent((SampleId)i);
  }

  sendJson(webServer, doc);

} //   handleSamplesListRequest()

//-- PUT /api/samples/gain — set sample gain
static void handleSamplesGainRequest()
{

  if (!webServer.hasArg("plain"))
  {
    sendError(webServer, "Missing body");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, webServer.arg("plain"));
  if (error || !doc["sampleIndex"].is<uint8_t>() || !doc["gainPercent"].is<uint16_t>())
  {
    sendError(webServer, "Missing sampleIndex or gainPercent");
    return;
  }

  uint8_t sampleIndex = doc["sampleIndex"];
  uint16_t gainPercent = doc["gainPercent"];

  if (sampleIndex >= 6)
  {
    sendError(webServer, "Invalid sample index");
    return;
  }

  if (!sampleManagerSetSampleGainPercent((SampleId)sampleIndex, gainPercent))
  {
    sendError(webServer, "Failed to set sample gain");
    return;
  }

  sendOk(webServer);

} //   handleSamplesGainRequest()

//-- GET /api/sequencer/view — sequencer state snapshot
static void handleSequencerViewRequest()
{

  SequencerView view;
  sequencerGetView(view);

  JsonDocument doc;
  doc["ok"] = true;
  doc["bpm"] = view.bpm;
  doc["swing"] = view.swingPercent;
  doc["currentStep"] = view.currentStep;
  doc["selectedTrack"] = view.selectedTrack;
  doc["cursorStep"] = view.cursorStep;
  doc["activePatternIndex"] = view.activePatternIndex;
  doc["playingPatternIndex"] = view.playingPatternIndex;
  doc["chainLength"] = view.chainLength;
  doc["playing"] = view.playing;
  doc["paused"] = view.paused;
  doc["editMode"] = view.editMode;
  doc["chainEnabled"] = view.chainEnabled;

  sendJson(webServer, doc);

} //   handleSequencerViewRequest()

//-- GET /api/sequencer/playhead — lightweight playhead info for frequent polling
static void handleSequencerPlayheadRequest()
{

  SequencerView view;
  sequencerGetView(view);

  JsonDocument doc;
  doc["ok"] = true;
  doc["bpm"] = view.bpm;
  doc["currentStep"] = view.currentStep;
  doc["playing"] = view.playing;
  doc["paused"] = view.paused;
  doc["activePatternIndex"] = view.activePatternIndex;
  doc["playingPatternIndex"] = view.playingPatternIndex;
  doc["chainEnabled"] = view.chainEnabled;
  doc["chainLength"] = view.chainLength;

  sendJson(webServer, doc);

} //   handleSequencerPlayheadRequest()

//-- POST /api/sequencer/cursor — set cursor position
static void handleSequencerCursorRequest()
{

  if (!webServer.hasArg("plain"))
  {
    sendError(webServer, "Missing body");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, webServer.arg("plain"));
  if (error || !doc["stepIndex"].is<uint8_t>() || !doc["trackIndex"].is<uint8_t>())
  {
    sendError(webServer, "Missing stepIndex or trackIndex");
    return;
  }

  uint8_t targetStep = doc["stepIndex"];
  uint8_t targetTrack = doc["trackIndex"];

  if (targetStep >= 16 || targetTrack >= 6)
  {
    sendError(webServer, "Invalid step or track index");
    return;
  }

  SequencerView view;
  sequencerGetView(view);

  int stepDelta = (int)targetStep - (int)view.cursorStep;
  int trackDelta = (int)targetTrack - (int)view.selectedTrack;

  if (stepDelta != 0)
    sequencerMoveCursor(stepDelta);
  if (trackDelta != 0)
    sequencerMoveTrack(trackDelta);

  sendOk(webServer);

} //   handleSequencerCursorRequest()

//-- POST /api/sequencer/edit-mode — set edit mode
static void handleSequencerEditModeRequest()
{

  if (!webServer.hasArg("plain"))
  {
    sendError(webServer, "Missing body");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, webServer.arg("plain"));
  if (error || !doc["enabled"].is<bool>())
  {
    sendError(webServer, "Missing enabled field");
    return;
  }

  bool targetMode = doc["enabled"];

  SequencerView view;
  sequencerGetView(view);

  if ((bool)view.editMode != targetMode)
  {
    sequencerToggleEditMode();
  }

  sendOk(webServer);

} //   handleSequencerEditModeRequest()

//-- Register all REST API routes onto the web server.
void webApiRegisterRoutes(WebServer& server)
{
  (void)server;

  webServer.on("/api/status", HTTP_GET, handleStatusRequest);
  webServer.on("/api/transport", HTTP_GET, handleTransportRequest);

  webServer.on("/api/transport/play", HTTP_POST, handleTransportPlayRequest);
  webServer.on("/api/transport/stop", HTTP_POST, handleTransportStopRequest);
  webServer.on("/api/transport/pause", HTTP_POST, handleTransportPauseRequest);
  webServer.on("/api/transport/continue", HTTP_POST, handleTransportContinueRequest);
  webServer.on("/api/transport/toggle", HTTP_POST, handleTransportToggleRequest);
  webServer.on("/api/transport/bpm", HTTP_POST, handleTransportBpmRequest);
  webServer.on("/api/transport/swing", HTTP_POST, handleTransportSwingRequest);

  webServer.on("/api/groups", HTTP_GET, handleGroupsListRequest);
  webServer.on("/api/groups/active", HTTP_GET, handleGroupsActiveRequest);
  webServer.on("/api/groups/load", HTTP_POST, handleGroupsLoadRequest);
  webServer.on("/api/groups/save", HTTP_POST, handleGroupsSaveRequest);
  webServer.on("/api/groups/new", HTTP_POST, handleGroupsNewRequest);
  webServer.on("/api/groups/rename", HTTP_POST, handleGroupsRenameRequest);
  webServer.on("/api/groups/copy", HTTP_POST, handleGroupsCopyRequest);
  webServer.on("/api/groups/delete", HTTP_POST, handleGroupsDeleteRequest);

  webServer.on("/api/patterns", HTTP_GET, handlePatternsListRequest);
  webServer.on("/api/patterns/active", HTTP_GET, handlePatternsActiveGetRequest);
  webServer.on("/api/patterns/active", HTTP_POST, handlePatternsActiveSetRequest);

  webServer.on(UriRegex("^\\/api\\/patterns\\/([pP][0-9][0-9])$"), HTTP_GET,
               handlePatternsGetRequest);
  webServer.on(UriRegex("^\\/api\\/patterns\\/([pP][0-9][0-9])$"), HTTP_PUT,
               handlePatternsPutRequest);
  webServer.on(UriRegex("^\\/api\\/patterns\\/([pP][0-9][0-9])\\/clear$"), HTTP_POST,
               handlePatternsClearRequest);
  webServer.on(UriRegex("^\\/api\\/patterns\\/([pP][0-9][0-9])\\/copy$"), HTTP_POST,
               handlePatternsCopyRequest);
  webServer.on(
      UriRegex("^\\/api\\/patterns\\/([pP][0-9][0-9])\\/tracks\\/([0-9]+)\\/steps\\/([0-9]+)$"),
      HTTP_PUT, handleStepEditRequest);
  webServer.on(UriRegex("^\\/api\\/patterns\\/([pP][0-9][0-9])\\/tracks\\/([0-9]+)$"), HTTP_PUT,
               handleTrackEditRequest);

  webServer.on(UriRegex("^\\/api\\/tracks\\/([0-9]+)\\/mute-toggle$"), HTTP_POST,
               handleTrackMuteToggleRequest);

  webServer.on("/api/sample-sets", HTTP_GET, handleSampleSetsListRequest);
  webServer.on("/api/sample-sets/active", HTTP_GET, handleSampleSetsActiveRequest);
  webServer.on("/api/sample-sets/load", HTTP_POST, handleSampleSetsLoadRequest);

  webServer.on("/api/samples", HTTP_GET, handleSamplesListRequest);
  webServer.on("/api/samples/gain", HTTP_PUT, handleSamplesGainRequest);

  webServer.on("/api/sequencer/view", HTTP_GET, handleSequencerViewRequest);
  webServer.on("/api/sequencer/playhead", HTTP_GET, handleSequencerPlayheadRequest);
  webServer.on("/api/sequencer/cursor", HTTP_POST, handleSequencerCursorRequest);
  webServer.on("/api/sequencer/edit-mode", HTTP_POST, handleSequencerEditModeRequest);

} //   webApiRegisterRoutes()
