/*** Last Changed: 2026-05-27 - 15:16 ***/
#include "uiManager.h"

#include "DisplayDriverClass.h"
#include "audioEngine.h"
#include "colorSettings.h"
#include "settingsStore.h"
#include "sequencer.h"
#include "systemManager.h"
#include "InputClass.h"
#include "progVersion.h"

#include <Arduino.h>
#include <ctype.h>
#include <esp_log.h>

//-- Logging tag.
static const char* logTag = "UiManager";

//-- UI refresh cadence.
static const uint32_t uiRefreshIntervalMs = 50;

//-- Contextual step parameter pages.
enum ParameterPage : uint8_t
{
  parameterPageTrig = 0,
  parameterPageVelocity,
  parameterPagePitch,
  parameterPageDecay,
  parameterPageProbability,
  parameterPageMute,
  parameterPageChain,
  parameterPageCount
};

//-- Settings menu entries.
static const int settingsEntryCount = 14;
static const int settingsFirstActionIndex = 3;
static const int patternListMaxEntries = static_cast<int>(patternStoreMaxEntries * 2U);
static const int menuVisibleLineCount = 9;

enum class PatternEntrySource : uint8_t
{
  Local = 0,
  Card = 1
};

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
  bool tempoEditOpen;
  bool editPopupOpen;
  bool editPopupValueEdit;
  uint8_t editPopupChainFocus;
  int tempoEditSelection;
  int editPopupSelection;
  bool wifiManagerConfirmOpen;
  bool eraseWifiConfirmOpen;
  bool patternListOpen;
  bool patternDeleteMode;
  bool newPatternLetterPickerOpen;
  bool patternStatusOpen;
  bool wifiManagerWaitingForCredentials;
  bool wifiManagerPortalSeenActive;
  bool eraseWifiRestartPending;
  int wifiManagerConfirmSelection;
  int eraseWifiConfirmSelection;
  int menuSelection;
  int menuFirstVisibleIndex;
  int patternListSelection;
  int patternListFirstVisibleIndex;
  int newPatternLetterSelection;
  int newPatternStorageSelection;
  int newPatternFieldSelection;
  int patternCount;
  bool newPatternFieldValueEdit;
  uint32_t patternStatusUntilMs;
  uint32_t eraseWifiRestartAtMs;
  uint32_t lastDrawMs;
  bool dirty;
  uint8_t parameterPageIndex;
  String activePatternName;
  String patternStatusText;
  String patternNames[patternListMaxEntries];
  PatternEntrySource patternSources[patternListMaxEntries];
  String chainSlotPatternNames[sequencerPatternCount];
};

//-- Runtime state.
static UiState uiState;
static uint8_t lastSequencerStep = 0xFF;
static uint8_t lastSequencerCursor = 0xFF;
static bool lastSequencerPlaying = false;
static uint8_t lastSequencerActivePatternIndex = 0xFF;
static uint32_t lastSequencerVoiceCount = 0xFFFFFFFFUL;
static bool sequencerScreenDrawn = false;
static String lastSequencerFooterLine;

//-- Maximum item text length inside list rows (display width is 26 chars).
static const size_t listRowContentChars = 24;

//-- Maximum footer text length drawn directly on screen.
static const size_t footerLineChars = 26;

//-- Conservative estimate used to map LittleFS free bytes to writable patterns.
static const size_t estimatedPatternBytes = 9957;

//-- Edit popup entries mapped 1:1 to ParameterPage values.
static const uint8_t editPopupPageMap[] =
    {
        parameterPageVelocity,
        parameterPagePitch,
        parameterPageDecay,
        parameterPageProbability,
        parameterPageMute,
        parameterPageChain};

static const int editPopupEntryCount = static_cast<int>(sizeof(editPopupPageMap) / sizeof(editPopupPageMap[0]));

static const char* editPopupEntries[editPopupEntryCount] =
    {
        "VELOCITY",
        "PITCH",
        "DECAY",
        "PROBABILITY",
        "MUTE",
        "CHAIN"};

//-- CHAIN value-edit focus fields.
static const uint8_t chainPopupFocusEnable = 0;
static const uint8_t chainPopupFocusLength = 1;
static const uint8_t chainPopupFocusPattern = 2;

//-- Select next/previous chain slot while keeping the same pattern letter group.
static void adjustChainPatternSlotWithLetterConstraint(int delta);

//-- Rotate selected New Pattern letter in full A..Z range.
static void cycleNewPatternLetter(int delta)
{
  if (delta < 0)
  {
    uiState.newPatternLetterSelection--;

    if (uiState.newPatternLetterSelection < 0)
    {
      uiState.newPatternLetterSelection = 25;
    }
  }
  else if (delta > 0)
  {
    uiState.newPatternLetterSelection++;

    if (uiState.newPatternLetterSelection > 25)
    {
      uiState.newPatternLetterSelection = 0;
    }
  }

} //   cycleNewPatternLetter()

//-- Format free-byte value as B, KB or MB depending on magnitude.
static String formatBytesAdaptive(size_t bytes)
{
  char buffer[32];

  if (bytes > (1024U * 1024U))
  {
    snprintf(buffer, sizeof(buffer), "%lu MB", static_cast<unsigned long>(bytes / (1024U * 1024U)));
  }
  else if (bytes > 1024U)
  {
    snprintf(buffer, sizeof(buffer), "%lu KB", static_cast<unsigned long>(bytes / 1024U));
  }
  else
  {
    snprintf(buffer, sizeof(buffer), "%lu B", static_cast<unsigned long>(bytes));
  }

  return String(buffer);

} //   formatBytesAdaptive()

//-- Resolve storage target directly from pattern letter: A-H Local, I-Z Card.
static PatternStorageTarget storageTargetFromPatternLetter(char patternLetter)
{
  char normalizedLetter = static_cast<char>(toupper(static_cast<unsigned char>(patternLetter)));

  if (normalizedLetter >= 'I' && normalizedLetter <= 'Z')
  {
    return PatternStorageTarget::Card;
  }

  return PatternStorageTarget::Local;

} //   storageTargetFromPatternLetter()

//-- Resolve storage target from full pattern name.
static PatternStorageTarget storageTargetFromPatternName(const String& patternName)
{
  if (patternName.length() > 0)
  {
    return storageTargetFromPatternLetter(patternName[0]);
  }

  return PatternStorageTarget::Local;

} //   storageTargetFromPatternName()

//-- Map current parameter page to popup selection index.
static int popupSelectionFromParameterPage(uint8_t parameterPage)
{
  for (int popupIndex = 0; popupIndex < editPopupEntryCount; popupIndex++)
  {
    if (editPopupPageMap[popupIndex] == parameterPage)
    {
      return popupIndex;
    }
  }

  return 0;

} //   popupSelectionFromParameterPage()

//-- Resolve popup selection index to parameter page.
static uint8_t popupSelectionToParameterPage(int popupSelection)
{
  if (popupSelection < 0 || popupSelection >= editPopupEntryCount)
  {
    return parameterPageVelocity;
  }

  return editPopupPageMap[popupSelection];

} //   popupSelectionToParameterPage()

//-- Build compact value text for one edit popup page.
static String buildEditPopupValueText(uint8_t pageIndex, const SequencerView& view)
{
  const Track& selectedTrack = view.pattern.tracks[view.selectedTrack];
  const Step& selectedStep = selectedTrack.steps[view.cursorStep];
  char valueBuffer[28];

  if (pageIndex == parameterPageTrig)
  {
    snprintf(valueBuffer,
             sizeof(valueBuffer),
             "S%02u %s",
             static_cast<unsigned>(view.cursorStep + 1U),
             selectedStep.trigger ? "ON" : "OFF");
  }
  else if (pageIndex == parameterPageVelocity)
  {
    snprintf(valueBuffer,
             sizeof(valueBuffer),
             "%03u",
             static_cast<unsigned>(selectedStep.velocity));
  }
  else if (pageIndex == parameterPagePitch)
  {
    snprintf(valueBuffer,
             sizeof(valueBuffer),
             "%+03d",
             static_cast<int>(selectedStep.lockPitch));
  }
  else if (pageIndex == parameterPageDecay)
  {
    snprintf(valueBuffer,
             sizeof(valueBuffer),
             "%03u%%",
             static_cast<unsigned>(selectedStep.lockDecay));
  }
  else if (pageIndex == parameterPageProbability)
  {
    snprintf(valueBuffer,
             sizeof(valueBuffer),
             "%03u%%",
             static_cast<unsigned>(selectedStep.probability));
  }
  else if (pageIndex == parameterPageMute)
  {
    snprintf(valueBuffer,
             sizeof(valueBuffer),
             "%s",
             selectedTrack.mute ? "ON" : "OFF");
  }
  else
  {
    snprintf(valueBuffer,
             sizeof(valueBuffer),
             "%s L%u P%u",
             view.chainEnabled ? "ON" : "OFF",
             static_cast<unsigned>(view.chainLength),
             static_cast<unsigned>(view.activePatternIndex + 1U));
  }

  return String(valueBuffer);

} //   buildEditPopupValueText()

//-- Build popup rows with selected-row label/value edit markers.
static void buildEditPopupRows(const SequencerView& view, String rows[editPopupEntryCount])
{
  for (int rowIndex = 0; rowIndex < editPopupEntryCount; rowIndex++)
  {
    uint8_t pageIndex = editPopupPageMap[rowIndex];
    String label = String(editPopupEntries[rowIndex]);
    String valueText = buildEditPopupValueText(pageIndex, view);

    if (pageIndex == parameterPageChain)
    {
      char chainLengthText[8];
      char chainPatternText[8];

      snprintf(chainLengthText,
               sizeof(chainLengthText),
               "L%u",
               static_cast<unsigned>(view.chainLength));

      snprintf(chainPatternText,
               sizeof(chainPatternText),
               "P%u",
               static_cast<unsigned>(view.activePatternIndex + 1U));

      if (rowIndex == uiState.editPopupSelection)
      {
        if (uiState.editPopupValueEdit)
        {
          if (uiState.editPopupChainFocus == chainPopupFocusLength)
          {
            rows[rowIndex] = " CHAIN " + String(view.chainEnabled ? "ON" : "OFF") + " >" + String(chainLengthText) + "< " + String(chainPatternText);
          }
          else if (uiState.editPopupChainFocus == chainPopupFocusPattern)
          {
            rows[rowIndex] = " CHAIN " + String(view.chainEnabled ? "ON" : "OFF") + " " + String(chainLengthText) + " >" + String(chainPatternText) + "<";
          }
          else
          {
            rows[rowIndex] = " CHAIN >" + String(view.chainEnabled ? "ON" : "OFF") + "< " + String(chainLengthText) + " " + String(chainPatternText);
          }
        }
        else
        {
          rows[rowIndex] = ">" + label + "< " + String(view.chainEnabled ? "ON" : "OFF") + " " + String(chainLengthText) + " " + String(chainPatternText);
        }
      }
      else
      {
        rows[rowIndex] = " CHAIN " + String(view.chainEnabled ? "ON" : "OFF") + " " + String(chainLengthText) + " " + String(chainPatternText);
      }

      continue;
    }

    if (rowIndex == uiState.editPopupSelection)
    {
      if (uiState.editPopupValueEdit)
      {
        rows[rowIndex] = " " + label + " >" + valueText + "<";
      }
      else
      {
        rows[rowIndex] = ">" + label + "< " + valueText;
      }
    }
    else
    {
      rows[rowIndex] = " " + label + " " + valueText;
    }
  }

} //   buildEditPopupRows()

//-- Apply encoder delta to selected popup value while value-edit mode is active.
static void applyEditPopupValueDelta(int delta)
{
  uint8_t pageIndex;
  SequencerView view;

  if (delta == 0)
  {
    return;
  }

  pageIndex = popupSelectionToParameterPage(uiState.editPopupSelection);

  if (pageIndex == parameterPageVelocity)
  {
    sequencerAdjustCurrentStepVelocity(delta > 0 ? 8 : -8);
  }
  else if (pageIndex == parameterPagePitch)
  {
    sequencerAdjustCurrentStepLockPitch(delta > 0 ? 1 : -1);
  }
  else if (pageIndex == parameterPageDecay)
  {
    sequencerAdjustCurrentStepLockDecay(delta > 0 ? 5 : -5);
  }
  else if (pageIndex == parameterPageProbability)
  {
    sequencerAdjustCurrentStepProbability(delta > 0 ? 5 : -5);
  }
  else if (pageIndex == parameterPageMute)
  {
    sequencerToggleMuteForSelectedTrack();
  }
  else
  {
    sequencerGetView(view);

    if (uiState.editPopupChainFocus == chainPopupFocusLength)
    {
      sequencerAdjustChainLength(delta > 0 ? 1 : -1);
    }
    else if (uiState.editPopupChainFocus == chainPopupFocusPattern)
    {
      adjustChainPatternSlotWithLetterConstraint(delta > 0 ? 1 : -1);
    }
    else
    {
      if (view.chainEnabled)
      {
        sequencerToggleChainEnabled();
      }
      else
      {
        if (view.chainLength <= 1U)
        {
          sequencerAdjustChainLength(1);
        }

        sequencerToggleChainEnabled();
      }
    }
  }

} //   applyEditPopupValueDelta()

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

//-- Return uppercase pattern letter from strict <LETTER><DIGIT><DIGIT> name.
static char patternLetterFromName(const String& patternName)
{
  char patternLetter;

  if (patternName.length() != 3)
  {
    return '\0';
  }

  patternLetter = patternName[0];

  if (patternLetter >= 'a' && patternLetter <= 'z')
  {
    patternLetter = static_cast<char>(patternLetter - ('a' - 'A'));
  }

  if (patternLetter < 'A' || patternLetter > 'Z')
  {
    return '\0';
  }

  if (patternName[1] < '0' || patternName[1] > '9' || patternName[2] < '0' || patternName[2] > '9')
  {
    return '\0';
  }

  return patternLetter;

} //   patternLetterFromName()

//-- Persist the currently active pattern name for one in-memory chain slot.
static void assignActivePatternNameToCurrentSlot()
{
  SequencerView view;

  sequencerGetView(view);

  if (view.activePatternIndex < sequencerPatternCount)
  {
    uiState.chainSlotPatternNames[view.activePatternIndex] = uiState.activePatternName;
  }

} //   assignActivePatternNameToCurrentSlot()

//-- Apply slot name back to active pattern label when known.
static void syncActivePatternNameFromSlot(uint8_t slotIndex)
{
  if (slotIndex >= sequencerPatternCount)
  {
    return;
  }

  if (!uiState.chainSlotPatternNames[slotIndex].isEmpty())
  {
    uiState.activePatternName = uiState.chainSlotPatternNames[slotIndex];
  }

} //   syncActivePatternNameFromSlot()

//-- Select next/previous chain slot while keeping the same pattern letter group.
static void adjustChainPatternSlotWithLetterConstraint(int delta)
{
  SequencerView view;
  int chainLimit;
  int direction;
  int candidateSlot;
  char activeLetter;

  if (delta == 0)
  {
    return;
  }

  sequencerGetView(view);

  chainLimit = static_cast<int>(view.chainLength);

  if (chainLimit < 1)
  {
    chainLimit = 1;
  }

  if (chainLimit == 1)
  {
    sequencerSetActivePatternIndex(0);
    return;
  }

  activeLetter = patternLetterFromName(uiState.activePatternName);

  if (activeLetter == '\0')
  {
    sequencerAdjustActivePatternIndex(delta > 0 ? 1 : -1);
    return;
  }

  direction = (delta > 0) ? 1 : -1;
  candidateSlot = static_cast<int>(view.activePatternIndex);

  for (int attempt = 0; attempt < chainLimit; attempt++)
  {
    char candidateLetter;

    candidateSlot += direction;

    if (candidateSlot < 0)
    {
      candidateSlot = chainLimit - 1;
    }
    else if (candidateSlot >= chainLimit)
    {
      candidateSlot = 0;
    }

    candidateLetter = patternLetterFromName(uiState.chainSlotPatternNames[candidateSlot]);

    if (candidateLetter == activeLetter)
    {
      sequencerSetActivePatternIndex(static_cast<uint8_t>(candidateSlot));
      syncActivePatternNameFromSlot(static_cast<uint8_t>(candidateSlot));
      return;
    }
  }

} //   adjustChainPatternSlotWithLetterConstraint()

//-- Draw letter picker popup overlay for New Pattern action.
static void drawNewPatternLetterPickerOverlay()
{
  char selectedLetter = static_cast<char>('A' + uiState.newPatternLetterSelection);
  String nextPatternName = "(No slots)";
  String options[4];
  char popupTitle[32];
  char nameLine[32];
  char freeLine[32];
  char bytesLine[32];
  String freeText;
  int byteBasedSlots = -1;
  int nameBasedSlots = -1;
  int freeSlots = -1;
  size_t totalBytes = 0;
  size_t usedBytes = 0;
  size_t freeBytes = 0;
  bool usageAvailable = false;
  PatternStorageTarget storageTarget = storageTargetFromPatternLetter(selectedLetter);
  const char* storageLabel = (storageTarget == PatternStorageTarget::Card) ? "Card" : "Local";

  if (storageTarget == PatternStorageTarget::Card)
  {
    settingsStoreFindNextPatternNameForLetterOnCard(selectedLetter, nextPatternName);
    settingsStoreCountAvailablePatternSlotsForLetterOnCard(selectedLetter, nameBasedSlots);
    usageAvailable = settingsStoreGetSdUsage(totalBytes, usedBytes, freeBytes);
  }
  else
  {
    settingsStoreFindNextPatternNameForLetter(selectedLetter, nextPatternName);
    settingsStoreCountAvailablePatternSlotsForLetter(selectedLetter, nameBasedSlots);
    usageAvailable = settingsStoreGetLittleFsUsage(totalBytes, usedBytes, freeBytes);
  }

  if (nextPatternName != "(No slots)")
  {
    snprintf(popupTitle, sizeof(popupTitle), "New Pattern %s", nextPatternName.c_str());
  }
  else
  {
    snprintf(popupTitle, sizeof(popupTitle), "New Pattern (full)");
  }

  snprintf(nameLine, sizeof(nameLine), "Pattern Name >%c<", selectedLetter);

  if (usageAvailable)
  {
    byteBasedSlots = static_cast<int>(freeBytes / estimatedPatternBytes);
    freeText = formatBytesAdaptive(freeBytes);
    snprintf(bytesLine, sizeof(bytesLine), "%s free: %s", storageLabel, freeText.c_str());
  }
  else
  {
    snprintf(bytesLine, sizeof(bytesLine), "%s free: ?", storageLabel);
  }

  if (nameBasedSlots >= 0 && byteBasedSlots >= 0)
  {
    freeSlots = (nameBasedSlots < byteBasedSlots) ? nameBasedSlots : byteBasedSlots;
  }
  else if (nameBasedSlots >= 0)
  {
    freeSlots = nameBasedSlots;
  }
  else if (byteBasedSlots >= 0)
  {
    freeSlots = byteBasedSlots;
  }

  if (freeSlots >= 0)
  {
    snprintf(freeLine, sizeof(freeLine), "Free slots: %d", freeSlots);
  }
  else
  {
    snprintf(freeLine, sizeof(freeLine), "Free slots: ?");
  }

  options[0] = nameLine;
  options[1] = freeLine;
  options[2] = bytesLine;
  options[3] = "Medium=create";

  display.drawSelectionOverlay(popupTitle, options, 4, 0);

} //   drawNewPatternLetterPickerOverlay()

//-- Redraw only New Pattern popup overlay for smooth encoder interaction.
static void drawNewPatternPopupOverlayOnly()
{
  drawNewPatternLetterPickerOverlay();

} //   drawNewPatternPopupOverlayOnly()

//-- Draw status popup overlay with up to 3 wrapped lines split by '\n'.
static void drawPatternStatusPopupOverlay()
{
  String popupLines[3];
  int lineCount = 0;
  int cursorStart = 0;

  while (lineCount < 3)
  {
    int newLineIndex = uiState.patternStatusText.indexOf('\n', cursorStart);

    if (newLineIndex < 0)
    {
      String tail = uiState.patternStatusText.substring(cursorStart);

      if (tail.length() > 0)
      {
        popupLines[lineCount++] = fitListRowText(tail);
      }

      break;
    }

    String segment = uiState.patternStatusText.substring(cursorStart, newLineIndex);
    popupLines[lineCount++] = fitListRowText(segment);
    cursorStart = newLineIndex + 1;
  }

  if (lineCount == 0)
  {
    popupLines[0] = "Done";
    lineCount = 1;
  }

  display.drawSelectionOverlay("Status", popupLines, static_cast<size_t>(lineCount), 0);

} //   drawPatternStatusPopupOverlay()

//-- Show temporary status and return to settings menu after timeout.
static void showPatternStatus(const String& statusText, uint32_t durationMs)
{
  uiState.patternStatusText = statusText;
  uiState.patternStatusOpen = true;
  uiState.patternStatusUntilMs = millis() + durationMs;

} //   showPatternStatus()

//-- Persist currently active editable system settings.
static void saveRuntimeSettingsFromCurrentState()
{
  RuntimeSettings settings;

  settings.displayRotation = static_cast<uint8_t>(displayGetRotation());
  settings.themeColorIndex = displayGetThemeColorIndex();
  settings.encoderDirectionReversed = input.getEncoderDirectionReversed();
  settings.activePatternName = uiState.activePatternName;

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

//-- Update list viewport with 2nd-top / 2nd-bottom scroll trigger behavior.
static void updateListFirstVisibleIndex(int selectedIndex, int itemCount, int& firstVisibleIndex)
{
  int maxFirstVisible = 0;
  int upperScrollTrigger = 2;
  int lowerScrollTrigger = menuVisibleLineCount - 2;

  if (itemCount <= menuVisibleLineCount)
  {
    firstVisibleIndex = 0;
    return;
  }

  maxFirstVisible = itemCount - menuVisibleLineCount;

  if (firstVisibleIndex < 0)
  {
    firstVisibleIndex = 0;
  }
  else if (firstVisibleIndex > maxFirstVisible)
  {
    firstVisibleIndex = maxFirstVisible;
  }

  if (selectedIndex < firstVisibleIndex)
  {
    firstVisibleIndex = selectedIndex;
  }
  else if (selectedIndex > (firstVisibleIndex + menuVisibleLineCount - 1))
  {
    firstVisibleIndex = selectedIndex - menuVisibleLineCount + 1;
  }

  if (selectedIndex <= (firstVisibleIndex + upperScrollTrigger) && firstVisibleIndex > 0)
  {
    firstVisibleIndex--;
  }
  else if (selectedIndex >= (firstVisibleIndex + lowerScrollTrigger) && firstVisibleIndex < maxFirstVisible)
  {
    firstVisibleIndex++;
  }

  if (firstVisibleIndex < 0)
  {
    firstVisibleIndex = 0;
  }
  else if (firstVisibleIndex > maxFirstVisible)
  {
    firstVisibleIndex = maxFirstVisible;
  }

} //   updateListFirstVisibleIndex()

//-- Refresh cached pattern names from LittleFS.
static void refreshPatternList()
{
  size_t localCount = 0;
  size_t cardCount = 0;
  String localPatterns[patternStoreMaxEntries];
  String cardPatterns[patternStoreMaxEntries];

  uiState.patternCount = 0;

  if (settingsStoreListPatterns(localPatterns, patternStoreMaxEntries, localCount))
  {
    for (size_t index = 0; index < localCount && uiState.patternCount < patternListMaxEntries; index++)
    {
      uiState.patternNames[uiState.patternCount] = localPatterns[index];
      uiState.patternSources[uiState.patternCount] = PatternEntrySource::Local;
      uiState.patternCount++;
    }
  }

  if (settingsStoreListPatternsOnCard(cardPatterns, patternStoreMaxEntries, cardCount))
  {
    for (size_t index = 0; index < cardCount && uiState.patternCount < patternListMaxEntries; index++)
    {
      uiState.patternNames[uiState.patternCount] = cardPatterns[index];
      uiState.patternSources[uiState.patternCount] = PatternEntrySource::Card;
      uiState.patternCount++;
    }
  }

  for (int leftIndex = 0; leftIndex < uiState.patternCount; leftIndex++)
  {
    for (int rightIndex = leftIndex + 1; rightIndex < uiState.patternCount; rightIndex++)
    {
      bool shouldSwap = false;

      if (uiState.patternNames[rightIndex].compareTo(uiState.patternNames[leftIndex]) < 0)
      {
        shouldSwap = true;
      }
      else if (uiState.patternNames[rightIndex] == uiState.patternNames[leftIndex] && uiState.patternSources[rightIndex] < uiState.patternSources[leftIndex])
      {
        shouldSwap = true;
      }

      if (shouldSwap)
      {
        String temporaryName = uiState.patternNames[leftIndex];
        PatternEntrySource temporarySource = uiState.patternSources[leftIndex];

        uiState.patternNames[leftIndex] = uiState.patternNames[rightIndex];
        uiState.patternSources[leftIndex] = uiState.patternSources[rightIndex];
        uiState.patternNames[rightIndex] = temporaryName;
        uiState.patternSources[rightIndex] = temporarySource;
      }
    }
  }

  if (uiState.patternListSelection >= uiState.patternCount)
  {
    uiState.patternListSelection = (uiState.patternCount > 0) ? (uiState.patternCount - 1) : 0;
  }

} //   refreshPatternList()

//-- Save current sequencer state under active pattern name.
static bool saveActivePattern(PatternStorageTarget* outStorageTarget = nullptr)
{
  PatternData patternData;
  String targetName = uiState.activePatternName;
  PatternStorageTarget storageTarget;

  if (targetName.isEmpty())
  {
    if (!settingsStoreFindNextPatternName(targetName))
    {
      return false;
    }
  }

  storageTarget = storageTargetFromPatternName(targetName);

  if (outStorageTarget != nullptr)
  {
    *outStorageTarget = storageTarget;
  }

  sequencerExportPattern(patternData);

  if (storageTarget == PatternStorageTarget::Card)
  {
    if (!settingsStoreSavePatternToCard(targetName, patternData))
    {
      return false;
    }
  }
  else if (!settingsStoreSavePattern(targetName, patternData))
  {
    return false;
  }

  uiState.activePatternName = targetName;
  assignActivePatternNameToCurrentSlot();
  saveRuntimeSettingsFromCurrentState();
  refreshPatternList();

  return true;

} //   saveActivePattern()

//-- Create a new pattern copy with automatic name.
static bool createNewPatternWithLetter(char patternLetter)
{
  String targetName;
  PatternData patternData;
  char normalizedLetter = static_cast<char>(toupper(static_cast<unsigned char>(patternLetter)));
  PatternStorageTarget storageTarget = storageTargetFromPatternLetter(normalizedLetter);

  if (storageTarget == PatternStorageTarget::Card)
  {
    if (!settingsStoreFindNextPatternNameForLetterOnCard(normalizedLetter, targetName))
    {
      return false;
    }
  }
  else if (!settingsStoreFindNextPatternNameForLetter(normalizedLetter, targetName))
  {
    return false;
  }

  sequencerExportPattern(patternData);

  if (storageTarget == PatternStorageTarget::Card)
  {
    if (!settingsStoreSavePatternToCard(targetName, patternData))
    {
      return false;
    }
  }
  else if (!settingsStoreSavePattern(targetName, patternData))
  {
    return false;
  }

  uiState.activePatternName = targetName;
  assignActivePatternNameToCurrentSlot();
  saveRuntimeSettingsFromCurrentState();
  refreshPatternList();

  return true;

} //   createNewPatternWithLetter()

//-- Load one pattern from current list selection.
static bool loadSelectedPattern()
{
  PatternData patternData;
  PatternEntrySource selectedSource;

  if (uiState.patternCount <= 0 || uiState.patternListSelection < 0 || uiState.patternListSelection >= uiState.patternCount)
  {
    return false;
  }

  String selectedName = uiState.patternNames[uiState.patternListSelection];
  selectedSource = uiState.patternSources[uiState.patternListSelection];

  if (selectedSource == PatternEntrySource::Card)
  {
    if (!settingsStoreLoadPatternFromCard(selectedName, patternData))
    {
      return false;
    }
  }
  else if (!settingsStoreLoadPattern(selectedName, patternData))
  {
    return false;
  }

  sequencerImportPattern(patternData);
  uiState.activePatternName = selectedName;
  assignActivePatternNameToCurrentSlot();
  saveRuntimeSettingsFromCurrentState();

  return true;

} //   loadSelectedPattern()

//-- Delete one pattern from current list selection.
static bool deleteSelectedPattern(String* outDeletedName = nullptr, PatternEntrySource* outDeletedSource = nullptr)
{
  PatternEntrySource selectedSource;

  if (uiState.patternCount <= 0 || uiState.patternListSelection < 0 || uiState.patternListSelection >= uiState.patternCount)
  {
    return false;
  }

  String selectedName = uiState.patternNames[uiState.patternListSelection];
  selectedSource = uiState.patternSources[uiState.patternListSelection];

  if (outDeletedName != nullptr)
  {
    *outDeletedName = selectedName;
  }

  if (outDeletedSource != nullptr)
  {
    *outDeletedSource = selectedSource;
  }

  if (selectedSource == PatternEntrySource::Card)
  {
    if (!settingsStoreDeletePatternFromCard(selectedName))
    {
      return false;
    }
  }
  else if (!settingsStoreDeletePattern(selectedName))
  {
    return false;
  }

  if (uiState.activePatternName == selectedName)
  {
    uiState.activePatternName = "";
    saveRuntimeSettingsFromCurrentState();
  }

  refreshPatternList();

  return true;

} //   deleteSelectedPattern()

//-- Build sequencer footer line text.
static String buildSequencerFooterLine(const SequencerView& view, const AudioEngineStats& audioStats)
{
  char footerLine[32];

  snprintf(footerLine,
           sizeof(footerLine),
           "STEP %02u CUR %02u P%u V%lu",
           static_cast<unsigned>(view.currentStep + 1U),
           static_cast<unsigned>(view.cursorStep + 1U),
           static_cast<unsigned>(view.activePatternIndex + 1U),
           static_cast<unsigned long>(audioStats.activeVoiceCount));

  return String(footerLine);

} //   buildSequencerFooterLine()

//-- Build contextual parameter overlay text for the selected step.
static String buildParameterOverlayLine(const SequencerView& view)
{
  const Track& selectedTrack = view.pattern.tracks[view.selectedTrack];
  const Step& selectedStep = selectedTrack.steps[view.cursorStep];
  char lineBuffer[72];

  if (uiState.parameterPageIndex == parameterPageTrig)
  {
    return "";
  }
  else if (uiState.parameterPageIndex == parameterPageVelocity)
  {
    snprintf(lineBuffer,
             sizeof(lineBuffer),
             "VEL   %03u",
             static_cast<unsigned>(selectedStep.velocity));
  }
  else if (uiState.parameterPageIndex == parameterPagePitch)
  {
    snprintf(lineBuffer,
             sizeof(lineBuffer),
             "PITCH %+03d  LOCK %s",
             static_cast<int>(selectedStep.lockPitch),
             selectedStep.lockEnabled ? "ON" : "OFF");
  }
  else if (uiState.parameterPageIndex == parameterPageDecay)
  {
    snprintf(lineBuffer,
             sizeof(lineBuffer),
             "DECAY %03u%% LOCK %s",
             static_cast<unsigned>(selectedStep.lockDecay),
             selectedStep.lockEnabled ? "ON" : "OFF");
  }
  else if (uiState.parameterPageIndex == parameterPageProbability)
  {
    snprintf(lineBuffer,
             sizeof(lineBuffer),
             "PROB  %03u%%",
             static_cast<unsigned>(selectedStep.probability));
  }
  else if (uiState.parameterPageIndex == parameterPageMute)
  {
    snprintf(lineBuffer,
             sizeof(lineBuffer),
             "MUTE  %s",
             selectedTrack.mute ? "ON" : "OFF");
  }
  else
  {
    snprintf(lineBuffer,
             sizeof(lineBuffer),
             "CHAIN %s L%u P%u",
             view.chainEnabled ? "ON" : "OFF",
             static_cast<unsigned>(view.chainLength),
             static_cast<unsigned>(view.activePatternIndex + 1U));
  }

  return String(lineBuffer);

} //   buildParameterOverlayLine()

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

  if (uiState.patternListOpen)
  {
    String items[patternListMaxEntries];
    int itemCount = uiState.patternCount;
    bool activeMarked = false;
    const char* title = uiState.patternDeleteMode ? "Delete Pattern" : "Load Pattern";

    if (itemCount <= 0)
    {
      items[0] = "(No patterns found)";
      itemCount = 1;
      uiState.patternListSelection = 0;
    }
    else
    {
      for (int itemIndex = 0; itemIndex < itemCount; itemIndex++)
      {
        String patternName = uiState.patternNames[itemIndex];
        String lineText = patternName;
        const char* sourceLabel = (uiState.patternSources[itemIndex] == PatternEntrySource::Card) ? " (On SDcard)" : " (Local)";

        lineText += sourceLabel;

        if (!activeMarked && !uiState.activePatternName.isEmpty() && patternName == uiState.activePatternName)
        {
          lineText += " *";
          activeMarked = true;
        }

        items[itemIndex] = fitListRowText(lineText);
      }

      updateListFirstVisibleIndex(uiState.patternListSelection, itemCount, uiState.patternListFirstVisibleIndex);
    }

    display.drawListScreen(title, items, static_cast<size_t>(itemCount), uiState.patternListSelection, uiState.patternListFirstVisibleIndex);

    if (uiState.patternStatusOpen)
    {
      drawPatternStatusPopupOverlay();
    }

    return;
  }

  String items[settingsEntryCount];
  bool disabledItems[settingsEntryCount] = {true, true, true, false, false, false, false, false, false, false, false, false, false, false};
  String ssidValue = systemManagerGetSsid();
  String ipValue = systemManagerGetIpAddress();
  String macValue = systemManagerGetMacAddress();
  String patternLabel = uiState.activePatternName.isEmpty() ? String("-") : uiState.activePatternName;
  int activeThemeIndex = displayGetThemeColorIndex();
  const char* themeName = colorProfiles[activeThemeIndex].colorName;
  int displayRotation = displayGetRotation();
  bool encoderReversed = input.getEncoderDirectionReversed();
  char loadEntry[40];
  char saveEntry[40];
  char themeEntry[32];
  char rotationEntry[40];
  char encoderOrderEntry[32];

  snprintf(loadEntry, sizeof(loadEntry), "Load Pattern");
  snprintf(saveEntry, sizeof(saveEntry), "Save Pattern (%s)", patternLabel.c_str());
  snprintf(themeEntry, sizeof(themeEntry), "Set Theme (%s)", themeName);
  snprintf(rotationEntry, sizeof(rotationEntry), "Rotate Display (%d)", displayRotation);
  snprintf(encoderOrderEntry, sizeof(encoderOrderEntry), "Encoder Order (%s)", encoderReversed ? "B-A" : "A-B");

  items[0] = fitListRowText("SSID: " + ssidValue);
  items[1] = fitListRowText("IP: " + ipValue);
  items[2] = fitListRowText("MAC: " + macValue);
  items[3] = fitListRowText(loadEntry);
  items[4] = fitListRowText(saveEntry);
  items[5] = "New Pattern";
  items[6] = "Delete Pattern";
  items[7] = "Erase WiFi Credentials";
  items[8] = "Start WiFi Manager";
  items[9] = fitListRowText(themeEntry);
  items[10] = fitListRowText(rotationEntry);
  items[11] = fitListRowText(encoderOrderEntry);
  items[12] = "Restart Groovebox";
  items[13] = "Exit";

  updateListFirstVisibleIndex(uiState.menuSelection, settingsEntryCount, uiState.menuFirstVisibleIndex);
  display.drawListScreenWithDisabledItems("System Settings", items, settingsEntryCount, uiState.menuSelection, uiState.menuFirstVisibleIndex, disabledItems);

  if (uiState.newPatternLetterPickerOpen)
  {
    drawNewPatternLetterPickerOverlay();
  }

  if (uiState.patternStatusOpen)
  {
    drawPatternStatusPopupOverlay();
  }

} //   drawSystemSettingsScreen()

//-- Draw sequencer overview page.
static void drawSequencerScreen()
{
  SequencerView view;
  String lines[9];
  String popupRows[editPopupEntryCount];
  String parameterLine;
  int selectedLine = 1;
  char headerLine[40];
  AudioEngineStats audioStats;

  sequencerGetView(view);
  audioEngineGetStats(audioStats);

  snprintf(headerLine,
           sizeof(headerLine),
           "BPM %03u  SW %02u  %s",
           static_cast<unsigned>(view.bpm),
           static_cast<unsigned>(view.swingPercent),
           view.playing ? "PLAY" : "STOP");
  lines[0] = fitListRowText(headerLine);

  for (uint8_t trackIndex = 0; trackIndex < sequencerTrackCount; trackIndex++)
  {
    lines[trackIndex + 1] = fitListRowText(buildTrackRowText(trackNames[trackIndex], view.pattern.tracks[trackIndex]));
  }

  parameterLine = "";

  if (!uiState.tempoEditOpen && !uiState.editPopupOpen && view.editMode)
  {
    parameterLine = buildParameterOverlayLine(view);
  }

  lines[7] = parameterLine.isEmpty() ? String("") : fitListRowText(parameterLine);
  lines[8] = fitListRowText(buildSequencerFooterLine(view, audioStats));

  selectedLine = static_cast<int>(view.selectedTrack) + 1;
  display.drawListScreen("Groovebox", lines, 9, selectedLine, 0, PROG_VERSION);

  if (uiState.tempoEditOpen)
  {
    display.drawTempoOverlay(view.bpm, view.swingPercent, uiState.tempoEditSelection == 0);
  }
  else if (uiState.editPopupOpen)
  {
    buildEditPopupRows(view, popupRows);
    display.drawSelectionOverlay("Edit Track", popupRows, editPopupEntryCount, uiState.editPopupSelection);
  }

  if (view.editMode && !uiState.tempoEditOpen && !uiState.editPopupOpen)
  {
    int stepCharIndex = 7 + static_cast<int>(view.cursorStep);

    if (stepCharIndex >= 0 && stepCharIndex < static_cast<int>(lines[selectedLine].length()))
    {
      display.drawListCharacterHighlight(selectedLine, stepCharIndex, lines[selectedLine][stepCharIndex]);
    }
  }

  lastSequencerFooterLine = lines[8];
  sequencerScreenDrawn = true;

} //   drawSequencerScreen()

//-- Redraw only the edit popup card for smoother interaction.
static void drawEditPopupOverlayOnly()
{
  SequencerView view;
  String popupRows[editPopupEntryCount];

  sequencerGetView(view);
  buildEditPopupRows(view, popupRows);
  display.drawSelectionOverlay("Edit Track", popupRows, editPopupEntryCount, uiState.editPopupSelection);

} //   drawEditPopupOverlayOnly()

//-- Update only the dynamic Groovebox footer row while running.
static void drawSequencerFooterUpdate(const SequencerView& view, const AudioEngineStats& audioStats)
{
  String footerLine = fitListRowText(buildSequencerFooterLine(view, audioStats));

  if (footerLine == lastSequencerFooterLine)
  {
    return;
  }

  display.drawListLine(8, footerLine, false);
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
    refreshPatternList();
    uiState.patternListOpen = true;
    uiState.patternDeleteMode = false;
    uiState.patternListSelection = 0;
    uiState.patternListFirstVisibleIndex = 0;
  }
  else if (uiState.menuSelection == 4)
  {
    PatternStorageTarget savedTarget = PatternStorageTarget::Local;

    if (saveActivePattern(&savedTarget))
    {
      String targetLabel = (savedTarget == PatternStorageTarget::Card) ? "SD card" : "Local storage";
      showPatternStatus(String("Save OK\n") + uiState.activePatternName + " -> " + targetLabel, 2000);
    }
    else
    {
      showPatternStatus("Save failed", 2000);
    }
  }
  else if (uiState.menuSelection == 5)
  {
    char activeLetter = patternLetterFromName(uiState.activePatternName);

    if (activeLetter < 'A' || activeLetter > 'Z')
    {
      activeLetter = 'A';
    }

    uiState.newPatternLetterPickerOpen = true;
    uiState.newPatternLetterSelection = static_cast<int>(activeLetter - 'A');
    uiState.newPatternStorageSelection = static_cast<int>(PatternStorageTarget::Local);
    uiState.newPatternFieldSelection = 0;
    uiState.newPatternFieldValueEdit = true;
  }
  else if (uiState.menuSelection == 6)
  {
    refreshPatternList();
    uiState.patternListOpen = true;
    uiState.patternDeleteMode = true;
    uiState.patternListSelection = 0;
    uiState.patternListFirstVisibleIndex = 0;
  }
  else if (uiState.menuSelection == 7)
  {
    uiState.eraseWifiConfirmOpen = true;
    uiState.eraseWifiConfirmSelection = 1;
  }
  else if (uiState.menuSelection == 8)
  {
    uiState.wifiManagerConfirmOpen = true;
    uiState.wifiManagerConfirmSelection = 1;
  }
  else if (uiState.menuSelection == 9)
  {
    activeThemeIndex = displayGetThemeColorIndex();
    nextThemeIndex = (activeThemeIndex + 1) % colorProfileCount;
    displaySetThemeColorIndex(nextThemeIndex);
    saveRuntimeSettingsFromCurrentState();
  }
  else if (uiState.menuSelection == 10)
  {
    nextRotation = (displayGetRotation() == 1) ? 3 : 1;
    displaySetRotation(nextRotation);
    saveRuntimeSettingsFromCurrentState();
  }
  else if (uiState.menuSelection == 11)
  {
    input.setEncoderDirectionReversed(!input.getEncoderDirectionReversed());
    saveRuntimeSettingsFromCurrentState();
  }
  else if (uiState.menuSelection == 12)
  {
    systemManagerQueueCommand(SystemCommand::restartNow);
  }
  else if (uiState.menuSelection == 13)
  {
    uiState.menuOpen = false;
  }

} //   executeMenuAction()

//-- Initialize splash and UI state.
void uiManagerInit()
{
  RuntimeSettings runtimeSettings;
  PatternData startupPatternData;
  bool startupPatternLoaded = false;

  uiState.menuOpen = false;
  uiState.tempoEditOpen = false;
  uiState.editPopupOpen = false;
  uiState.editPopupValueEdit = false;
  uiState.editPopupChainFocus = chainPopupFocusEnable;
  uiState.tempoEditSelection = 0;
  uiState.editPopupSelection = 0;
  uiState.wifiManagerConfirmOpen = false;
  uiState.eraseWifiConfirmOpen = false;
  uiState.patternListOpen = false;
  uiState.patternDeleteMode = false;
  uiState.newPatternLetterPickerOpen = false;
  uiState.patternStatusOpen = false;
  uiState.wifiManagerWaitingForCredentials = false;
  uiState.wifiManagerPortalSeenActive = false;
  uiState.eraseWifiRestartPending = false;
  uiState.wifiManagerConfirmSelection = 0;
  uiState.eraseWifiConfirmSelection = 0;
  uiState.patternListSelection = 0;
  uiState.menuFirstVisibleIndex = 0;
  uiState.patternListFirstVisibleIndex = 0;
  uiState.newPatternLetterSelection = 0;
  uiState.newPatternStorageSelection = static_cast<int>(PatternStorageTarget::Local);
  uiState.newPatternFieldSelection = 0;
  uiState.newPatternFieldValueEdit = false;
  uiState.patternCount = 0;
  uiState.patternStatusUntilMs = 0;
  uiState.eraseWifiRestartAtMs = 0;
  uiState.menuSelection = settingsEntryCount - 1;
  uiState.lastDrawMs = 0;
  uiState.dirty = true;
  uiState.parameterPageIndex = parameterPageTrig;
  uiState.activePatternName = "";
  uiState.patternStatusText = "";
  lastSequencerStep = 0xFF;
  lastSequencerCursor = 0xFF;
  lastSequencerPlaying = false;
  lastSequencerActivePatternIndex = 0xFF;
  lastSequencerVoiceCount = 0xFFFFFFFFUL;
  sequencerScreenDrawn = false;
  lastSequencerFooterLine = "";

  for (uint8_t slotIndex = 0; slotIndex < sequencerPatternCount; slotIndex++)
  {
    uiState.chainSlotPatternNames[slotIndex] = "";
  }

  settingsStoreLoadRuntimeSettings(runtimeSettings);

  if (!runtimeSettings.activePatternName.isEmpty() && settingsStoreLoadPattern(runtimeSettings.activePatternName, startupPatternData))
  {
    sequencerImportPattern(startupPatternData);
    uiState.activePatternName = runtimeSettings.activePatternName;
    uiState.chainSlotPatternNames[0] = uiState.activePatternName;
    startupPatternLoaded = true;
  }

  refreshPatternList();

  if (!startupPatternLoaded && uiState.patternCount > 0)
  {
    if (settingsStoreLoadPattern(uiState.patternNames[0], startupPatternData))
    {
      sequencerImportPattern(startupPatternData);
      uiState.activePatternName = uiState.patternNames[0];
      uiState.chainSlotPatternNames[0] = uiState.activePatternName;
      saveRuntimeSettingsFromCurrentState();
    }
  }

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

  if (uiState.patternStatusOpen && nowMs >= uiState.patternStatusUntilMs)
  {
    uiState.patternStatusOpen = false;
    uiState.patternStatusText = "";
    uiState.dirty = true;
  }

  if (!uiState.menuOpen)
  {
    sequencerGetView(view);
    audioEngineGetStats(audioStats);

    if (view.activePatternIndex != lastSequencerActivePatternIndex)
    {
      syncActivePatternNameFromSlot(view.activePatternIndex);
      lastSequencerActivePatternIndex = view.activePatternIndex;
    }

    footerStateChanged = (view.currentStep != lastSequencerStep) ||
                         (view.cursorStep != lastSequencerCursor) ||
                         (audioStats.activeVoiceCount != lastSequencerVoiceCount);
    transportStateChanged = (view.playing != lastSequencerPlaying);

    if (transportStateChanged)
    {
      uiState.dirty = true;
    }
  }

  if (!uiState.dirty && !uiState.menuOpen && !uiState.tempoEditOpen && !uiState.editPopupOpen && view.playing && footerStateChanged && sequencerScreenDrawn)
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

  if (uiState.editPopupOpen && !uiState.menuOpen && !uiState.tempoEditOpen)
  {
    uiState.lastDrawMs = nowMs;
    drawEditPopupOverlayOnly();
    uiState.dirty = false;
    return;
  }

  if (uiState.menuOpen && uiState.newPatternLetterPickerOpen && !uiState.patternStatusOpen)
  {
    uiState.lastDrawMs = nowMs;
    drawNewPatternPopupOverlayOnly();
    uiState.dirty = false;
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
    lastSequencerActivePatternIndex = view.activePatternIndex;
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

  SequencerView view;
  sequencerGetView(view);

  ESP_LOGI(logTag, "Encoder event=%d, menuOpen=%d", static_cast<int>(encoderEvent), uiState.menuOpen ? 1 : 0);

  if (encoderEvent == ENCODER_EVENT_LONG_PRESS)
  {
    if (uiState.editPopupOpen)
    {
      uiState.editPopupOpen = false;
      uiState.editPopupValueEdit = false;
      uiState.editPopupChainFocus = chainPopupFocusEnable;
      uiState.dirty = true;
      return;
    }

    if (!uiState.menuOpen && !uiState.tempoEditOpen && view.editMode)
    {
      if (uiState.parameterPageIndex == 0)
      {
        uiState.parameterPageIndex = parameterPageCount - 1;
      }
      else
      {
        uiState.parameterPageIndex--;
      }

      uiState.dirty = true;
      return;
    }

    if (uiState.wifiManagerWaitingForCredentials)
    {
      systemManagerQueueCommand(SystemCommand::restartNow);
      return;
    }

    uiState.menuOpen = !uiState.menuOpen;
    uiState.tempoEditOpen = false;
    uiState.wifiManagerConfirmOpen = false;
    uiState.eraseWifiConfirmOpen = false;
    uiState.patternListOpen = false;
    uiState.patternDeleteMode = false;
    uiState.newPatternLetterPickerOpen = false;
    uiState.patternStatusOpen = false;
    uiState.patternStatusText = "";

    if (uiState.menuOpen)
    {
      uiState.menuSelection = settingsFirstActionIndex;
      uiState.menuFirstVisibleIndex = 0;
    }

    normalizeMenuSelection(0);
    updateListFirstVisibleIndex(uiState.menuSelection, settingsEntryCount, uiState.menuFirstVisibleIndex);
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
    if (uiState.patternStatusOpen)
    {
      uiState.dirty = true;
      return;
    }

    if (encoderEvent == ENCODER_EVENT_LEFT)
    {
      if (uiState.newPatternLetterPickerOpen)
      {
        cycleNewPatternLetter(-1);
      }
      else if (uiState.patternListOpen)
      {
        if (uiState.patternCount > 0)
        {
          uiState.patternListSelection--;

          if (uiState.patternListSelection < 0)
          {
            uiState.patternListSelection = uiState.patternCount - 1;
          }

          updateListFirstVisibleIndex(uiState.patternListSelection, uiState.patternCount, uiState.patternListFirstVisibleIndex);
        }
      }
      else if (uiState.wifiManagerConfirmOpen)
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
        updateListFirstVisibleIndex(uiState.menuSelection, settingsEntryCount, uiState.menuFirstVisibleIndex);
      }
    }
    else if (encoderEvent == ENCODER_EVENT_RIGHT)
    {
      if (uiState.newPatternLetterPickerOpen)
      {
        cycleNewPatternLetter(1);
      }
      else if (uiState.patternListOpen)
      {
        if (uiState.patternCount > 0)
        {
          uiState.patternListSelection++;

          if (uiState.patternListSelection >= uiState.patternCount)
          {
            uiState.patternListSelection = 0;
          }

          updateListFirstVisibleIndex(uiState.patternListSelection, uiState.patternCount, uiState.patternListFirstVisibleIndex);
        }
      }
      else if (uiState.wifiManagerConfirmOpen)
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
        updateListFirstVisibleIndex(uiState.menuSelection, settingsEntryCount, uiState.menuFirstVisibleIndex);
      }
    }
    else if (encoderEvent == ENCODER_EVENT_SHORT_PRESS || encoderEvent == ENCODER_EVENT_MEDIUM_PRESS)
    {
      if (uiState.newPatternLetterPickerOpen)
      {
        char selectedLetter = static_cast<char>('A' + uiState.newPatternLetterSelection);
        PatternStorageTarget storageTarget = storageTargetFromPatternLetter(selectedLetter);

        if (createNewPatternWithLetter(selectedLetter))
        {
          String destinationLabel = (storageTarget == PatternStorageTarget::Card) ? " on Card" : " local";

          showPatternStatus(String("Created ") + uiState.activePatternName + destinationLabel, 2000);
          uiState.newPatternLetterPickerOpen = false;
          uiState.newPatternFieldValueEdit = false;
          uiState.patternListOpen = false;
          uiState.patternDeleteMode = false;
          uiState.patternListFirstVisibleIndex = 0;
        }
        else
        {
          showPatternStatus("Create failed", 2000);
        }
      }
      else if (uiState.patternListOpen)
      {
        if (uiState.patternCount > 0)
        {
          if (!uiState.patternDeleteMode)
          {
            if (loadSelectedPattern())
            {
              uiState.menuOpen = false;
              uiState.patternListOpen = false;
              uiState.patternListFirstVisibleIndex = 0;
            }
          }
          else if (encoderEvent == ENCODER_EVENT_SHORT_PRESS || encoderEvent == ENCODER_EVENT_MEDIUM_PRESS)
          {
            String deletedName;
            PatternEntrySource deletedSource = PatternEntrySource::Local;

            if (deleteSelectedPattern(&deletedName, &deletedSource))
            {
              if (deletedSource == PatternEntrySource::Card)
              {
                showPatternStatus(deletedName + " deleted from\nSD card", 2000);
              }
              else
              {
                showPatternStatus(deletedName + " deleted from\nLocal storage", 2000);
              }
            }
            else
            {
              showPatternStatus("Delete failed", 2000);
            }

            if (uiState.patternCount == 0)
            {
              uiState.patternListOpen = false;
              uiState.patternListFirstVisibleIndex = 0;
            }
          }
        }
      }
      else if (uiState.wifiManagerConfirmOpen)
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

  if (uiState.tempoEditOpen)
  {
    if (encoderEvent == ENCODER_EVENT_LEFT)
    {
      if (uiState.tempoEditSelection == 0)
      {
        sequencerAdjustBpm(-1);
      }
      else
      {
        sequencerAdjustSwing(-1);
      }
    }
    else if (encoderEvent == ENCODER_EVENT_RIGHT)
    {
      if (uiState.tempoEditSelection == 0)
      {
        sequencerAdjustBpm(1);
      }
      else
      {
        sequencerAdjustSwing(1);
      }
    }
    else if (encoderEvent == ENCODER_EVENT_SHORT_PRESS)
    {
      uiState.tempoEditSelection = (uiState.tempoEditSelection == 0) ? 1 : 0;
    }
    else if (encoderEvent == ENCODER_EVENT_MEDIUM_PRESS)
    {
      uiState.tempoEditOpen = false;
      uiState.tempoEditSelection = 0;
    }

    uiState.dirty = true;
    return;
  }

  if (uiState.editPopupOpen)
  {
    if (encoderEvent == ENCODER_EVENT_LEFT)
    {
      if (uiState.editPopupValueEdit)
      {
        applyEditPopupValueDelta(-1);
      }
      else
      {
        uiState.editPopupSelection--;

        if (uiState.editPopupSelection < 0)
        {
          uiState.editPopupSelection = editPopupEntryCount - 1;
        }

        uiState.editPopupChainFocus = chainPopupFocusEnable;
        uiState.parameterPageIndex = popupSelectionToParameterPage(uiState.editPopupSelection);
      }
    }
    else if (encoderEvent == ENCODER_EVENT_RIGHT)
    {
      if (uiState.editPopupValueEdit)
      {
        applyEditPopupValueDelta(1);
      }
      else
      {
        uiState.editPopupSelection++;

        if (uiState.editPopupSelection >= editPopupEntryCount)
        {
          uiState.editPopupSelection = 0;
        }

        uiState.editPopupChainFocus = chainPopupFocusEnable;
        uiState.parameterPageIndex = popupSelectionToParameterPage(uiState.editPopupSelection);
      }
    }
    else if (encoderEvent == ENCODER_EVENT_SHORT_PRESS)
    {
      uiState.parameterPageIndex = popupSelectionToParameterPage(uiState.editPopupSelection);

      if (!uiState.editPopupValueEdit)
      {
        uiState.editPopupValueEdit = true;
        uiState.editPopupChainFocus = chainPopupFocusEnable;
      }
      else if (uiState.parameterPageIndex == parameterPageChain)
      {
        uiState.editPopupChainFocus = static_cast<uint8_t>((uiState.editPopupChainFocus + 1U) % 3U);
      }
      else
      {
        uiState.editPopupValueEdit = false;
      }
    }
    else if (encoderEvent == ENCODER_EVENT_MEDIUM_PRESS)
    {
      uiState.parameterPageIndex = popupSelectionToParameterPage(uiState.editPopupSelection);
      uiState.editPopupValueEdit = false;
      uiState.editPopupChainFocus = chainPopupFocusEnable;
    }

    uiState.dirty = true;
    return;
  }

  if (encoderEvent == ENCODER_EVENT_LEFT)
  {
    if (view.editMode)
    {
      if (uiState.parameterPageIndex == parameterPageTrig)
      {
        sequencerMoveCursor(-1);
      }
      else if (uiState.parameterPageIndex == parameterPageVelocity)
      {
        sequencerAdjustCurrentStepVelocity(-8);
      }
      else if (uiState.parameterPageIndex == parameterPagePitch)
      {
        sequencerAdjustCurrentStepLockPitch(-1);
      }
      else if (uiState.parameterPageIndex == parameterPageDecay)
      {
        sequencerAdjustCurrentStepLockDecay(-5);
      }
      else if (uiState.parameterPageIndex == parameterPageProbability)
      {
        sequencerAdjustCurrentStepProbability(-5);
      }
      else if (uiState.parameterPageIndex == parameterPageMute)
      {
        sequencerMoveTrack(-1);
      }
      else
      {
        sequencerAdjustChainLength(-1);
      }
    }
    else
    {
      sequencerMoveTrack(-1);
    }
  }
  else if (encoderEvent == ENCODER_EVENT_RIGHT)
  {
    if (view.editMode)
    {
      if (uiState.parameterPageIndex == parameterPageTrig)
      {
        sequencerMoveCursor(1);
      }
      else if (uiState.parameterPageIndex == parameterPageVelocity)
      {
        sequencerAdjustCurrentStepVelocity(8);
      }
      else if (uiState.parameterPageIndex == parameterPagePitch)
      {
        sequencerAdjustCurrentStepLockPitch(1);
      }
      else if (uiState.parameterPageIndex == parameterPageDecay)
      {
        sequencerAdjustCurrentStepLockDecay(5);
      }
      else if (uiState.parameterPageIndex == parameterPageProbability)
      {
        sequencerAdjustCurrentStepProbability(5);
      }
      else if (uiState.parameterPageIndex == parameterPageMute)
      {
        sequencerMoveTrack(1);
      }
      else
      {
        sequencerAdjustChainLength(1);
      }
    }
    else
    {
      sequencerMoveTrack(1);
    }
  }
  else if (encoderEvent == ENCODER_EVENT_SHORT_PRESS)
  {
    if (!view.editMode)
    {
      sequencerToggleEditMode();
    }
    else
    {
      if (uiState.parameterPageIndex == parameterPageTrig)
      {
        sequencerToggleCurrentStep();
      }
      else if (uiState.parameterPageIndex == parameterPageMute)
      {
        sequencerToggleMuteForSelectedTrack();
      }
      else if (uiState.parameterPageIndex == parameterPagePitch || uiState.parameterPageIndex == parameterPageDecay)
      {
        sequencerToggleCurrentStepLock();
      }
      else if (uiState.parameterPageIndex == parameterPageChain)
      {
        sequencerToggleChainEnabled();
      }
      else
      {
        sequencerToggleCurrentStep();
      }
    }
  }
  else if (encoderEvent == ENCODER_EVENT_MEDIUM_PRESS)
  {
    if (view.editMode)
    {
      uiState.editPopupSelection = popupSelectionFromParameterPage(uiState.parameterPageIndex);
      uiState.editPopupOpen = true;
      uiState.editPopupValueEdit = false;
      uiState.editPopupChainFocus = chainPopupFocusEnable;
    }
    else
    {
      uiState.tempoEditOpen = true;
      uiState.tempoEditSelection = 0;
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
      if (uiState.newPatternLetterPickerOpen)
      {
        uiState.newPatternLetterPickerOpen = false;
        showPatternStatus("Creation\nCancelled", 2000);
      }
      else if (uiState.patternListOpen)
      {
        uiState.patternListOpen = false;
        uiState.patternDeleteMode = false;
        uiState.patternListFirstVisibleIndex = 0;
      }
      else if (uiState.wifiManagerConfirmOpen)
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
        uiState.tempoEditOpen = false;
        uiState.editPopupOpen = false;
        uiState.editPopupValueEdit = false;
        uiState.editPopupChainFocus = chainPopupFocusEnable;
        uiState.tempoEditSelection = 0;
      }

      uiState.dirty = true;
    }

    return;
  }

  if (uiState.tempoEditOpen)
  {
    if (buttonEvent == BUTTON_EVENT_SHORT_PRESS || buttonEvent == BUTTON_EVENT_MEDIUM_PRESS || buttonEvent == BUTTON_EVENT_LONG_PRESS)
    {
      uiState.tempoEditOpen = false;
      uiState.tempoEditSelection = 0;
      uiState.dirty = true;
    }

    return;
  }

  if (uiState.editPopupOpen)
  {
    if (buttonEvent == BUTTON_EVENT_SHORT_PRESS || buttonEvent == BUTTON_EVENT_MEDIUM_PRESS || buttonEvent == BUTTON_EVENT_LONG_PRESS)
    {
      uiState.editPopupOpen = false;
      uiState.editPopupValueEdit = false;
      uiState.editPopupChainFocus = chainPopupFocusEnable;
      uiState.dirty = true;
    }

    return;
  }

  if (buttonEvent == BUTTON_EVENT_SHORT_PRESS)
  {
    SequencerView view;
    sequencerGetView(view);

    if (view.editMode)
    {
      sequencerToggleEditMode();
      uiState.parameterPageIndex = parameterPageTrig;
      uiState.editPopupOpen = false;
      uiState.editPopupValueEdit = false;
      uiState.editPopupChainFocus = chainPopupFocusEnable;
    }
    else
    {
      sequencerTogglePlay();
    }
  }
  else if (buttonEvent == BUTTON_EVENT_MEDIUM_PRESS)
  {
    uiState.tempoEditOpen = true;
    uiState.tempoEditSelection = 0;
  }
  else if (buttonEvent == BUTTON_EVENT_LONG_PRESS)
  {
    uiState.tempoEditOpen = true;
    uiState.tempoEditSelection = 1;
  }

  uiState.dirty = true;

} //   uiManagerHandleAuxButtonEvent()