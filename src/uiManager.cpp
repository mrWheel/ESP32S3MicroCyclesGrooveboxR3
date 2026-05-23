/*** Last Changed: 2026-05-23 - 16:09 ***/
#include "uiManager.h"

#include "DisplayDriver.h"
#include "audioEngine.h"
#include "colorSettings.h"
#include "settingsStore.h"
#include "sequencer.h"
#include "systemManager.h"
#include "InputClass.h"

#include <Arduino.h>
#include <esp_log.h>

//-- Logging tag.
static const char* logTag = "UiManager";

//-- UI refresh cadence.
static const uint32_t uiRefreshIntervalMs = 50;

//-- Settings menu entries.
static const int settingsEntryCount = 9;
static const int settingsFirstActionIndex = 3;

//-- Track labels for sequencer page.
static const char* trackNames[sequencerTrackCount] =
    {
        "KICK",
        "SNARE",
        "CH",
        "OH",
        "TONE",
        "METAL"};

//-- UI state container.
struct UiState
{
  bool menuOpen;
  bool bpmEditMode;
  bool wifiManagerConfirmOpen;
  bool eraseWifiConfirmOpen;
  bool wifiManagerWaitingForCredentials;
  bool wifiManagerPortalSeenActive;
  bool eraseWifiRestartPending;
  int wifiManagerConfirmSelection;
  int eraseWifiConfirmSelection;
  int menuSelection;
  uint32_t eraseWifiRestartAtMs;
  uint32_t lastDrawMs;
  bool dirty;
};

//-- Runtime state.
static UiState uiState;
static uint8_t lastSequencerStep = 0xFF;
static uint8_t lastSequencerCursor = 0xFF;
static bool lastSequencerPlaying = false;
static uint32_t lastSequencerVoiceCount = 0xFFFFFFFFUL;
static bool sequencerScreenDrawn = false;
static String lastSequencerFooterLine;

//-- Maximum item text length inside list rows (display width is 26 chars).
static const size_t listRowContentChars = 24;

//-- Maximum footer text length drawn directly on screen.
static const size_t footerLineChars = 26;

//-- Build 16-step trigger string for one track.
static String buildTrackStepText(const Track& track)
{
  char stepText[17];

  for (uint8_t stepIndex = 0; stepIndex < sequencerStepCount; stepIndex++)
  {
    stepText[stepIndex] = track.steps[stepIndex].trigger ? 'x' : '-';
  }

  stepText[16] = '\0';

  return String(stepText);

} //   buildTrackStepText()

//-- Build one Groovebox track row: left-aligned instrument, right-aligned 16 steps.
static String buildTrackRowText(const char* trackName, const Track& track)
{
  String row;
  String leftField;

  leftField = String(trackName);

  if (track.mute)
  {
    leftField += "*";
  }

  if (leftField.length() > 7)
  {
    leftField = leftField.substring(0, 7);
  }

  while (leftField.length() < 7)
  {
    leftField += " ";
  }

  row = leftField;
  row += buildTrackStepText(track);
  row += " ";

  return row;

} //   buildTrackRowText()

//-- Clip list item text to fit 26-char display with cursor wrappers.
static String fitListRowText(const String& text)
{
  if (text.length() <= listRowContentChars)
  {
    return text;
  }

  return text.substring(0, listRowContentChars);

} //   fitListRowText()

//-- Build fixed-width line for direct footer redraw.
static String fitFixedWidthText(const String& text, size_t width)
{
  String output = text;

  if (output.length() > width)
  {
    output = output.substring(0, width);
  }

  while (output.length() < width)
  {
    output += " ";
  }

  return output;

} //   fitFixedWidthText()

//-- Persist currently active editable system settings.
static void saveRuntimeSettingsFromCurrentState()
{
  RuntimeSettings settings;

  settings.displayRotation = static_cast<uint8_t>(displayGetRotation());
  settings.themeColorIndex = displayGetThemeColorIndex();
  settings.encoderDirectionReversed = input.getEncoderDirectionReversed();

  if (!settingsStoreSaveRuntimeSettings(settings))
  {
    ESP_LOGW(logTag, "Failed to persist runtime settings");
  }

} //   saveRuntimeSettingsFromCurrentState()

//-- Keep menu selection on actionable entries only.
static void normalizeMenuSelection(int direction)
{
  if (uiState.menuSelection < settingsFirstActionIndex)
  {
    uiState.menuSelection = settingsFirstActionIndex;
  }

  if (uiState.menuSelection >= settingsEntryCount)
  {
    uiState.menuSelection = settingsEntryCount - 1;
  }

  while (uiState.menuSelection < settingsFirstActionIndex && direction != 0)
  {
    uiState.menuSelection += direction;

    if (uiState.menuSelection < settingsFirstActionIndex)
    {
      uiState.menuSelection = settingsEntryCount - 1;
    }

    if (uiState.menuSelection >= settingsEntryCount)
    {
      uiState.menuSelection = settingsFirstActionIndex;
    }
  }

} //   normalizeMenuSelection()

//-- Build sequencer footer line text.
static String buildSequencerFooterLine(const SequencerView& view, const AudioEngineStats& audioStats)
{
  char footerLine[32];

  snprintf(footerLine,
           sizeof(footerLine),
           "STEP %02u  CUR %02u  V%lu",
           static_cast<unsigned>(view.currentStep + 1U),
           static_cast<unsigned>(view.cursorStep + 1U),
           static_cast<unsigned long>(audioStats.activeVoiceCount));

  return String(footerLine);

} //   buildSequencerFooterLine()

//-- Draw generic Are-you-sure submenu with No/Yes choices.
static void drawConfirmationScreen(const char* title, int selection)
{
  String yesLine = (selection == 2) ? ">Yes<" : " Yes ";
  String noLine = (selection == 1) ? ">No<" : " No ";

  display.clearScreen(0x0000);
  display.drawHeader(title);
  display.drawCenteredLine("Are you sure?", 74, 0xFFFF, 0x0000);
  display.drawCenteredLine(noLine.c_str(), 112, 0xFFFF, 0x0000);
  display.drawCenteredLine(yesLine.c_str(), 142, 0xFFFF, 0x0000);

} //   drawConfirmationScreen()

//-- Draw required system settings menu.
static void drawSystemSettingsScreen()
{
  if (uiState.eraseWifiRestartPending)
  {
    display.drawWifiPortalScreen("Erasing credentials", "Restart Groovebox", "", "");
    return;
  }

  if (uiState.wifiManagerWaitingForCredentials)
  {
    String apSsid = systemManagerGetPortalApSsid();
    String line2 = String("Connect to ") + apSsid;

    display.drawWifiPortalScreen("WiFi portal started", line2.c_str(), "Enter credentials", "Waiting...");
    return;
  }

  if (uiState.eraseWifiConfirmOpen)
  {
    drawConfirmationScreen("Erase WiFi Credentials", uiState.eraseWifiConfirmSelection);
    return;
  }

  if (uiState.wifiManagerConfirmOpen)
  {
    drawConfirmationScreen("Start WiFi Manager", uiState.wifiManagerConfirmSelection);
    return;
  }

  String items[settingsEntryCount];
  bool disabledItems[settingsEntryCount] = {true, true, true, false, false, false, false, false, false};
  String ssidValue = systemManagerGetSsid();
  String ipValue = systemManagerGetIpAddress();
  String macValue = systemManagerGetMacAddress();
  int activeThemeIndex = displayGetThemeColorIndex();
  const char* themeName = colorProfiles[activeThemeIndex].colorName;
  int displayRotation = displayGetRotation();
  bool encoderReversed = input.getEncoderDirectionReversed();
  char themeEntry[32];
  char rotationEntry[40];
  char encoderOrderEntry[32];

  snprintf(themeEntry, sizeof(themeEntry), "Set Theme (%s)", themeName);
  snprintf(rotationEntry, sizeof(rotationEntry), "Rotate Display (%d)", displayRotation);
  snprintf(encoderOrderEntry, sizeof(encoderOrderEntry), "Encoder Order (%s)", encoderReversed ? "B-A" : "A-B");

  items[0] = fitListRowText("SSID: " + ssidValue);
  items[1] = fitListRowText("IP: " + ipValue);
  items[2] = fitListRowText("MAC: " + macValue);
  items[3] = "Erase WiFi Credentials";
  items[4] = "Start WiFi Manager";
  items[5] = fitListRowText(themeEntry);
  items[6] = fitListRowText(rotationEntry);
  items[7] = fitListRowText(encoderOrderEntry);
  items[8] = "Exit";

  display.drawListScreenWithDisabledItems("System Settings", items, settingsEntryCount, uiState.menuSelection, 0, disabledItems);

} //   drawSystemSettingsScreen()

//-- Draw sequencer overview page.
static void drawSequencerScreen()
{
  SequencerView view;
  String lines[8];
  int selectedLine = 1;
  char headerLine[48];
  AudioEngineStats audioStats;

  sequencerGetView(view);
  audioEngineGetStats(audioStats);

  snprintf(headerLine, sizeof(headerLine), "BPM %u  SW %u  %s %s", static_cast<unsigned>(view.bpm), static_cast<unsigned>(view.swingPercent), view.playing ? "PLAY" : "STOP", uiState.bpmEditMode ? "[BPM]" : (view.shiftMode ? "[SHIFT]" : ""));
  lines[0] = fitListRowText(headerLine);

  for (uint8_t trackIndex = 0; trackIndex < sequencerTrackCount; trackIndex++)
  {
    lines[trackIndex + 1] = fitListRowText(buildTrackRowText(trackNames[trackIndex], view.pattern.tracks[trackIndex]));
  }

  lines[7] = fitListRowText(buildSequencerFooterLine(view, audioStats));

  selectedLine = static_cast<int>(view.selectedTrack) + 1;
  display.drawListScreen("Groovebox", lines, 8, selectedLine, 0);
  lastSequencerFooterLine = lines[7];
  sequencerScreenDrawn = true;

} //   drawSequencerScreen()

//-- Update only the dynamic Groovebox footer row while running.
static void drawSequencerFooterUpdate(const SequencerView& view, const AudioEngineStats& audioStats)
{
  String footerLine = fitFixedWidthText(buildSequencerFooterLine(view, audioStats), footerLineChars);

  if (footerLine == lastSequencerFooterLine)
  {
    return;
  }

  display.drawCenteredLine(footerLine.c_str(), 184, 0xFFFF, 0x0000);
  lastSequencerFooterLine = footerLine;

} //   drawSequencerFooterUpdate()

//-- Run selected action from settings menu.
static void executeMenuAction()
{
  int activeThemeIndex;
  int nextThemeIndex;
  int nextRotation;

  if (uiState.menuSelection == 3)
  {
    uiState.eraseWifiConfirmOpen = true;
    uiState.eraseWifiConfirmSelection = 1;
  }
  else if (uiState.menuSelection == 4)
  {
    uiState.wifiManagerConfirmOpen = true;
    uiState.wifiManagerConfirmSelection = 1;
  }
  else if (uiState.menuSelection == 5)
  {
    activeThemeIndex = displayGetThemeColorIndex();
    nextThemeIndex = (activeThemeIndex + 1) % colorProfileCount;
    displaySetThemeColorIndex(nextThemeIndex);
    saveRuntimeSettingsFromCurrentState();
  }
  else if (uiState.menuSelection == 6)
  {
    nextRotation = (displayGetRotation() == 1) ? 3 : 1;
    displaySetRotation(nextRotation);
    saveRuntimeSettingsFromCurrentState();
  }
  else if (uiState.menuSelection == 7)
  {
    input.setEncoderDirectionReversed(!input.getEncoderDirectionReversed());
    saveRuntimeSettingsFromCurrentState();
  }
  else if (uiState.menuSelection == 8)
  {
    uiState.menuOpen = false;
  }

} //   executeMenuAction()

//-- Initialize splash and UI state.
void uiManagerInit()
{
  uiState.menuOpen = false;
  uiState.bpmEditMode = false;
  uiState.wifiManagerConfirmOpen = false;
  uiState.eraseWifiConfirmOpen = false;
  uiState.wifiManagerWaitingForCredentials = false;
  uiState.wifiManagerPortalSeenActive = false;
  uiState.eraseWifiRestartPending = false;
  uiState.wifiManagerConfirmSelection = 0;
  uiState.eraseWifiConfirmSelection = 0;
  uiState.eraseWifiRestartAtMs = 0;
  uiState.menuSelection = settingsEntryCount - 1;
  uiState.lastDrawMs = 0;
  uiState.dirty = true;
  lastSequencerStep = 0xFF;
  lastSequencerCursor = 0xFF;
  lastSequencerPlaying = false;
  lastSequencerVoiceCount = 0xFFFFFFFFUL;
  sequencerScreenDrawn = false;
  lastSequencerFooterLine = "";

  display.drawMessage("ESP32 Groovebox", "Phase 1 + 2 Boot");

} //   uiManagerInit()

//-- Periodic UI redraw/update.
void uiManagerUpdate()
{
  uint32_t nowMs = millis();
  SequencerView view;
  AudioEngineStats audioStats;
  bool footerStateChanged = false;
  bool transportStateChanged = false;

  if (uiState.eraseWifiRestartPending && nowMs >= uiState.eraseWifiRestartAtMs)
  {
    systemManagerQueueCommand(SystemCommand::eraseWifiCredentials);
    uiState.eraseWifiRestartPending = false;
    return;
  }

  if (uiState.wifiManagerWaitingForCredentials)
  {
    if (systemManagerIsWifiPortalActive())
    {
      uiState.wifiManagerPortalSeenActive = true;
    }
    else if (uiState.wifiManagerPortalSeenActive)
    {
      uiState.wifiManagerWaitingForCredentials = false;
      uiState.wifiManagerPortalSeenActive = false;
      uiState.dirty = true;
    }
  }

  if (!uiState.menuOpen)
  {
    sequencerGetView(view);
    audioEngineGetStats(audioStats);

    footerStateChanged = (view.currentStep != lastSequencerStep) ||
                         (view.cursorStep != lastSequencerCursor) ||
                         (audioStats.activeVoiceCount != lastSequencerVoiceCount);
    transportStateChanged = (view.playing != lastSequencerPlaying);

    if (transportStateChanged)
    {
      uiState.dirty = true;
    }
  }

  if (!uiState.dirty && !uiState.menuOpen && view.playing && footerStateChanged && sequencerScreenDrawn)
  {
    if (nowMs - uiState.lastDrawMs < uiRefreshIntervalMs)
    {
      return;
    }

    uiState.lastDrawMs = nowMs;
    drawSequencerFooterUpdate(view, audioStats);

    lastSequencerStep = view.currentStep;
    lastSequencerCursor = view.cursorStep;
    lastSequencerVoiceCount = audioStats.activeVoiceCount;

    return;
  }

  if (!uiState.dirty)
  {
    return;
  }

  if (nowMs - uiState.lastDrawMs < uiRefreshIntervalMs)
  {
    return;
  }

  uiState.lastDrawMs = nowMs;

  if (uiState.menuOpen)
  {
    sequencerScreenDrawn = false;
    drawSystemSettingsScreen();
  }
  else
  {
    drawSequencerScreen();

    sequencerGetView(view);
    audioEngineGetStats(audioStats);

    lastSequencerPlaying = view.playing;
    lastSequencerStep = view.currentStep;
    lastSequencerCursor = view.cursorStep;
    lastSequencerVoiceCount = audioStats.activeVoiceCount;
  }

  uiState.dirty = false;

} //   uiManagerUpdate()

//-- Route encoder events into UI state machine.
void uiManagerHandleEncoderEvent(EncoderEvent encoderEvent)
{
  if (encoderEvent == ENCODER_EVENT_NONE)
  {
    return;
  }

  ESP_LOGI(logTag, "Encoder event=%d, menuOpen=%d", static_cast<int>(encoderEvent), uiState.menuOpen ? 1 : 0);

  if (encoderEvent == ENCODER_EVENT_LONG_PRESS)
  {
    if (uiState.wifiManagerWaitingForCredentials)
    {
      systemManagerQueueCommand(SystemCommand::restartNow);
      return;
    }

    uiState.menuOpen = !uiState.menuOpen;
    uiState.bpmEditMode = false;
    uiState.wifiManagerConfirmOpen = false;
    uiState.eraseWifiConfirmOpen = false;
    normalizeMenuSelection(0);
    uiState.dirty = true;
    ESP_LOGI(logTag, "Long press toggled settings menu: menuOpen=%d", uiState.menuOpen ? 1 : 0);

    if (uiState.menuOpen)
    {
      display.drawMessage("System Settings", "Opening menu...");
    }

    return;
  }

  if (uiState.menuOpen)
  {
    if (encoderEvent == ENCODER_EVENT_LEFT)
    {
      if (uiState.wifiManagerConfirmOpen)
      {
        uiState.wifiManagerConfirmSelection = (uiState.wifiManagerConfirmSelection <= 1) ? 2 : 1;
      }
      else if (uiState.eraseWifiConfirmOpen)
      {
        uiState.eraseWifiConfirmSelection = (uiState.eraseWifiConfirmSelection <= 1) ? 2 : 1;
      }
      else if (!uiState.wifiManagerWaitingForCredentials)
      {
        uiState.menuSelection--;
        normalizeMenuSelection(-1);
      }
    }
    else if (encoderEvent == ENCODER_EVENT_RIGHT)
    {
      if (uiState.wifiManagerConfirmOpen)
      {
        uiState.wifiManagerConfirmSelection = (uiState.wifiManagerConfirmSelection >= 2) ? 1 : 2;
      }
      else if (uiState.eraseWifiConfirmOpen)
      {
        uiState.eraseWifiConfirmSelection = (uiState.eraseWifiConfirmSelection >= 2) ? 1 : 2;
      }
      else if (!uiState.wifiManagerWaitingForCredentials)
      {
        uiState.menuSelection++;
        normalizeMenuSelection(1);
      }
    }
    else if (encoderEvent == ENCODER_EVENT_SHORT_PRESS || encoderEvent == ENCODER_EVENT_MEDIUM_PRESS)
    {
      if (uiState.wifiManagerConfirmOpen)
      {
        if (uiState.wifiManagerConfirmSelection == 2)
        {
          systemManagerQueueCommand(SystemCommand::startWifiManager);
          uiState.wifiManagerWaitingForCredentials = true;
          uiState.wifiManagerPortalSeenActive = false;
        }

        uiState.wifiManagerConfirmOpen = false;
      }
      else if (uiState.eraseWifiConfirmOpen)
      {
        if (uiState.eraseWifiConfirmSelection == 2)
        {
          uiState.eraseWifiRestartPending = true;
          uiState.eraseWifiRestartAtMs = millis() + 1200;
        }

        uiState.eraseWifiConfirmOpen = false;
      }
      else if (!uiState.wifiManagerWaitingForCredentials)
      {
        executeMenuAction();
      }
    }

    uiState.dirty = true;

    return;
  }

  SequencerView view;
  sequencerGetView(view);

  if (encoderEvent == ENCODER_EVENT_LEFT)
  {
    if (uiState.bpmEditMode)
    {
      sequencerAdjustBpm(-1);
    }
    else if (view.shiftMode)
    {
      sequencerMoveCursor(-1);
    }
    else
    {
      sequencerMoveTrack(-1);
    }
  }
  else if (encoderEvent == ENCODER_EVENT_RIGHT)
  {
    if (uiState.bpmEditMode)
    {
      sequencerAdjustBpm(1);
    }
    else if (view.shiftMode)
    {
      sequencerMoveCursor(1);
    }
    else
    {
      sequencerMoveTrack(1);
    }
  }
  else if (encoderEvent == ENCODER_EVENT_SHORT_PRESS)
  {
    if (uiState.bpmEditMode)
    {
      uiState.bpmEditMode = false;
    }
    else
    {
      sequencerToggleCurrentStep();
    }
  }
  else if (encoderEvent == ENCODER_EVENT_MEDIUM_PRESS)
  {
    if (view.shiftMode)
    {
      sequencerToggleMuteForSelectedTrack();
    }
    else
    {
      uiState.bpmEditMode = !uiState.bpmEditMode;
    }
  }

  uiState.dirty = true;

} //   uiManagerHandleEncoderEvent()

//-- Route KEY0 events into UI state machine.
void uiManagerHandleAuxButtonEvent(ButtonEvent buttonEvent)
{
  if (buttonEvent == BUTTON_EVENT_NONE)
  {
    return;
  }

  if (uiState.menuOpen)
  {
    if (buttonEvent == BUTTON_EVENT_SHORT_PRESS)
    {
      if (uiState.wifiManagerConfirmOpen)
      {
        uiState.wifiManagerConfirmOpen = false;
      }
      else if (uiState.eraseWifiConfirmOpen)
      {
        uiState.eraseWifiConfirmOpen = false;
      }
      else if (uiState.wifiManagerWaitingForCredentials)
      {
        // Stay on waiting screen until credentials are entered or portal closes.
      }
      else
      {
        uiState.menuOpen = false;
        uiState.bpmEditMode = false;
      }

      uiState.dirty = true;
    }

    return;
  }

  if (buttonEvent == BUTTON_EVENT_SHORT_PRESS)
  {
    sequencerTogglePlay();
  }
  else if (buttonEvent == BUTTON_EVENT_MEDIUM_PRESS)
  {
    sequencerToggleShiftMode();
  }
  else if (buttonEvent == BUTTON_EVENT_LONG_PRESS)
  {
    sequencerAdjustSwing(2);
  }

  uiState.dirty = true;

} //   uiManagerHandleAuxButtonEvent()