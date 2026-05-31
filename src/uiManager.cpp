/*** Last Changed: 2026-05-31 - 11:29 ***/
#include "uiManager.h"

#include "DisplayDriverClass.h"
#include "audioEngine.h"
#include "colorSettings.h"
#include "settingsStore.h"
#include "sampleManager.h"
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

//-- Pattern group name input configuration.
static const char* patternGroupNameInputTokens = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
static const int patternGroupNameInputLength = 8;

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
//-- Special list mode for Card pattern groups.
static const int patternListModeCardGroups = 1001;
static const int patternListModeMemoryPatterns = 1002;
//-- Card Storage menu item count.
static const int cardStorageMenuEntryCount = 5;
static const int sampleSetListMaxEntries = 9;
static const int settingsFirstActionIndex = 3;
static const int patternListMaxEntries = static_cast<int>(patternStoreMaxEntries * 2U);
static const int menuVisibleLineCount = 9;

enum class PatternEntrySource : uint8_t
{
  Local = 0,
  Card = 1
};

//-- Track labels for sequencer page.
static const char* trackNames[sequencerTrackCount] = {"KICK", "SNARE", "CH", "OH", "TONE", "METAL"};

//-- UI state container.
struct UiState
{
  bool menuOpen;
  bool tempoEditOpen;
  bool editPopupOpen;
  bool editPopupValueEdit;
  int editPopupSelection;
  uint8_t editPopupChainFocus;
  int tempoEditSelection;
  bool wifiManagerConfirmOpen;
  bool eraseWifiConfirmOpen;
  bool patternListOpen;
  bool patternDeleteMode;
  int patternListSourceFilter;
  bool patternListNeedsRefresh;
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
  int patternCount;
  uint32_t patternStatusUntilMs;
  uint32_t eraseWifiRestartAtMs;
  uint32_t lastDrawMs;
  bool dirty;
  uint8_t parameterPageIndex;
  String activePatternName;
  String patternStatusText;
  String chainTargetPatternName;
  bool chainTargetValid;
  bool chainSettingsDirty;
  String patternChainTargets[patternListMaxEntries];
  bool patternHasChainTarget[patternListMaxEntries];
  String chainSeriesPatternNames[patternStoreMaxEntries];
  int chainSeriesPatternCount;
  char chainSeriesPatternLetter;
  bool chainSeriesPatternCacheValid;
  String chainSlotTargetPatternNames[sequencerPatternCount];
  String patternNames[patternListMaxEntries];
  String patternListDisplayItems[patternListMaxEntries];
  PatternEntrySource patternSources[patternListMaxEntries];
  String chainSlotPatternNames[sequencerPatternCount];
  bool localStorageMenuOpen;
  bool cardStorageMenuOpen;
  int cardStorageMenuSelection;
  int cardStorageMenuFirstVisibleIndex;
  bool sampleSetListOpen;
  int sampleSetListSelection;
  int sampleSetListFirstVisibleIndex;
  int sampleSetCount;
  String sampleSetNames[sampleSetListMaxEntries];
  String sampleSetDisplayItems[sampleSetListMaxEntries];
  bool patternGroupNameInputOpen;
  bool patternGroupNameInputCopyMode;
  int patternGroupNameInputCursor;
  int patternGroupNameInputTokenIndex;
  char patternGroupNameInputValue[9];
};

//-- Runtime state.
static UiState uiState;
static uint8_t lastSequencerStep = 0xFF;
static uint8_t lastSequencerCursor = 0xFF;
static bool lastSequencerPlaying = false;
static uint8_t lastSequencerActivePatternIndex = 0xFF;
static bool sequencerScreenDrawn = false;
static String lastSequencerFooterLine;
static String patternScanBuffer[patternStoreMaxEntries];

//-- Maximum item text length inside list rows (display width is 26 chars).
static const size_t listRowContentChars = 24;

//-- Maximum footer text length drawn directly on screen.
static const size_t footerLineChars = 26;

//-- Conservative estimate used to map LittleFS free bytes to writable patterns.
static const size_t estimatedPatternBytes = 9957;

//-- Edit popup entries mapped 1:1 to ParameterPage values.
static const uint8_t editPopupPageMap[] = {parameterPageVelocity, parameterPagePitch,
                                           parameterPageDecay,    parameterPageProbability,
                                           parameterPageMute,     parameterPageChain};

static const int editPopupEntryCount =
    static_cast<int>(sizeof(editPopupPageMap) / sizeof(editPopupPageMap[0]));

static const char* editPopupEntries[editPopupEntryCount] = {"VELOCITY",    "PITCH", "DECAY",
                                                            "PROBABILITY", "MUTE",  "CHAIN"};

//-- CHAIN value-edit focus fields.
static const uint8_t chainPopupFocusEnable = 0;
static const uint8_t chainPopupFocusLength = 1;
static const uint8_t chainPopupFocusPattern = 2;

//-- Select next/previous chain target pattern in the same series letter.
static void selectNextPatternInSeries(int direction);

//-- Rebuild cached same-series chain target list.
static void refreshChainSeriesPatternCache();

//-- Build display label for chain target field.
static String formatChainTargetLabel();

//-- Load persisted chain settings for the active pattern.
static void loadChainSettingsForActivePattern();

//-- Save active-pattern chain settings without rewriting unrelated JSON fields.
static void saveChainSettingsForPattern();

//-- Commit pending chain settings to storage when UI leaves edit interactions.
static void flushPendingChainSettings();

//-aaw- Load one Card pattern group directly into sequencer memory by group name.
static bool loadCardPatternGroupIntoMemory(const String& groupName, bool showStatus);

//-- Rotate selected New Pattern letter in full A..Z range.

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
  const Track& selectedTrack = view.pattern->tracks[view.selectedTrack];
  const Step& selectedStep = selectedTrack.steps[view.cursorStep];
  char valueBuffer[28];

  if (pageIndex == parameterPageTrig)
  {
    snprintf(valueBuffer, sizeof(valueBuffer), "S%02u %s",
             static_cast<unsigned>(view.cursorStep + 1U), selectedStep.trigger ? "ON" : "OFF");
  }
  else if (pageIndex == parameterPageVelocity)
  {
    snprintf(valueBuffer, sizeof(valueBuffer), "%03u",
             static_cast<unsigned>(selectedStep.velocity));
  }
  else if (pageIndex == parameterPagePitch)
  {
    snprintf(valueBuffer, sizeof(valueBuffer), "%+03d", static_cast<int>(selectedStep.lockPitch));
  }
  else if (pageIndex == parameterPageDecay)
  {
    snprintf(valueBuffer, sizeof(valueBuffer), "%03u%%",
             static_cast<unsigned>(selectedStep.lockDecay));
  }
  else if (pageIndex == parameterPageProbability)
  {
    snprintf(valueBuffer, sizeof(valueBuffer), "%03u%%",
             static_cast<unsigned>(selectedStep.probability));
  }
  else if (pageIndex == parameterPageMute)
  {
    snprintf(valueBuffer, sizeof(valueBuffer), "%s", selectedTrack.mute ? "ON" : "OFF");
  }
  else
  {
    snprintf(valueBuffer, sizeof(valueBuffer), "%s L%u P%u", view.chainEnabled ? "ON" : "OFF",
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
      String chainTargetLabel = formatChainTargetLabel();

      snprintf(chainLengthText, sizeof(chainLengthText), "L%u",
               static_cast<unsigned>(view.chainLength));

      if (rowIndex == uiState.editPopupSelection)
      {
        if (uiState.editPopupValueEdit)
        {
          if (uiState.editPopupChainFocus == chainPopupFocusLength)
          {
            rows[rowIndex] = " CHAIN " + String(view.chainEnabled ? "ON" : "OFF") + " >" +
                             String(chainLengthText) + "< " + chainTargetLabel;
          }
          else if (uiState.editPopupChainFocus == chainPopupFocusPattern)
          {
            rows[rowIndex] = " CHAIN " + String(view.chainEnabled ? "ON" : "OFF") + " " +
                             String(chainLengthText) + " >" + chainTargetLabel + "<";
          }
          else
          {
            rows[rowIndex] = " CHAIN >" + String(view.chainEnabled ? "ON" : "OFF") + "< " +
                             String(chainLengthText) + " " + chainTargetLabel;
          }
        }
        else
        {
          rows[rowIndex] = ">" + label + "< " + String(view.chainEnabled ? "ON" : "OFF") + " " +
                           String(chainLengthText) + " " + chainTargetLabel;
        }
      }
      else
      {
        rows[rowIndex] = " CHAIN " + String(view.chainEnabled ? "ON" : "OFF") + " " +
                         String(chainLengthText) + " " + chainTargetLabel;
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
      selectNextPatternInSeries(delta > 0 ? 1 : -1);
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

    uiState.chainSettingsDirty = true;
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

//-- Return pattern slot number from pNN name.
static int patternSlotNumberFromName(const String& patternName)
{
  String normalizedName = patternName;

  if (normalizedName.endsWith(".json"))
  {
    normalizedName = normalizedName.substring(0, normalizedName.length() - 5);
  }

  if (normalizedName.length() != 3)
  {
    return -1;
  }

  if (normalizedName[0] != 'p' && normalizedName[0] != 'P')
  {
    return -1;
  }

  if (!isDigit(normalizedName[1]) || !isDigit(normalizedName[2]))
  {
    return -1;
  }

  int slotNumber = normalizedName.substring(1).toInt();

  if (slotNumber < 1)
  {
    return -1;
  }

  return slotNumber;

} //   patternSlotNumberFromName()

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

//-- Scan existing patterns for one series letter.
static int scanPatternsForSeries(char seriesLetter, String outNames[patternStoreMaxEntries])
{
  size_t listedCount = 0;

  if (!settingsStoreListPatternsForSeries(seriesLetter, outNames, patternStoreMaxEntries,
                                          listedCount))
  {
    return 0;
  }

  return static_cast<int>(listedCount);

} //   scanPatternsForSeries()

//-- Rebuild cached chain target list from loaded RAM pattern slots.
static void refreshChainSeriesPatternCache()
{
  uiState.chainSeriesPatternCount = 0;
  uiState.chainSeriesPatternLetter = 'p';
  uiState.chainSeriesPatternCacheValid = false;

  for (uint8_t slotIndex = 0; slotIndex < sequencerPatternCount; slotIndex++)
  {
    String slotName = uiState.chainSlotPatternNames[slotIndex];

    if (slotName.isEmpty())
    {
      continue;
    }

    if (slotName == uiState.activePatternName)
    {
      continue;
    }

    if (patternSlotNumberFromName(slotName) < 1)
    {
      continue;
    }

    uiState.chainSeriesPatternNames[uiState.chainSeriesPatternCount] = slotName;
    uiState.chainSeriesPatternCount++;

    if (uiState.chainSeriesPatternCount >= static_cast<int>(patternStoreMaxEntries))
    {
      break;
    }
  }

  uiState.chainSeriesPatternCacheValid = true;

} //   refreshChainSeriesPatternCache()

//-- Return available loaded pNN targets excluding the active pattern name.
static int getAvailablePatternsForCurrentSeries(String outNames[patternStoreMaxEntries])
{
  if (!uiState.chainSeriesPatternCacheValid)
  {
    refreshChainSeriesPatternCache();
  }

  for (int nameIndex = 0; nameIndex < uiState.chainSeriesPatternCount; nameIndex++)
  {
    outNames[nameIndex] = uiState.chainSeriesPatternNames[nameIndex];
  }

  return uiState.chainSeriesPatternCount;

} //   getAvailablePatternsForCurrentSeries()

//-- Validate the current chain target against loaded RAM pattern slots.
static bool isCurrentChainTargetValid()
{
  String availableNames[patternStoreMaxEntries];
  int availableCount;

  if (uiState.chainTargetPatternName.isEmpty())
  {
    return false;
  }

  availableCount = getAvailablePatternsForCurrentSeries(availableNames);

  for (int nameIndex = 0; nameIndex < availableCount; nameIndex++)
  {
    if (availableNames[nameIndex] == uiState.chainTargetPatternName)
    {
      return true;
    }
  }

  return false;

} //   isCurrentChainTargetValid()

//-- Load chain settings for active RAM pattern without touching LittleFS.
static void loadChainSettingsForActivePattern()
{
  SequencerView view;

  uiState.chainTargetValid = false;
  uiState.chainSettingsDirty = false;
  uiState.chainTargetPatternName = "";

  sequencerGetView(view);

  if (view.activePatternIndex >= sequencerPatternCount)
  {
    return;
  }

  refreshChainSeriesPatternCache();

  if (!uiState.chainSlotTargetPatternNames[view.activePatternIndex].isEmpty())
  {
    uiState.chainTargetPatternName = uiState.chainSlotTargetPatternNames[view.activePatternIndex];
    uiState.chainTargetValid = isCurrentChainTargetValid();
  }

} //   loadChainSettingsForActivePattern()

//-- Build display label for chain target field.
static String formatChainTargetLabel()
{
  if (!uiState.chainTargetValid)
  {
    return "--";
  }

  return uiState.chainTargetPatternName;

} //   formatChainTargetLabel()

//-- Save active-pattern chain settings into RAM bookkeeping only.
static void saveChainSettingsForPattern()
{
  SequencerView view;

  sequencerGetView(view);

  if (view.activePatternIndex >= sequencerPatternCount)
  {
    return;
  }

  uiState.chainSlotPatternNames[view.activePatternIndex] = uiState.activePatternName;

  if (uiState.chainTargetValid)
  {
    uiState.chainSlotTargetPatternNames[view.activePatternIndex] = uiState.chainTargetPatternName;
  }
  else
  {
    uiState.chainSlotTargetPatternNames[view.activePatternIndex] = "";
  }

} //   saveChainSettingsForPattern()

//-- Commit pending chain settings to storage when UI leaves edit interactions.
static void flushPendingChainSettings()
{
  if (!uiState.chainSettingsDirty)
  {
    return;
  }

  saveChainSettingsForPattern();
  uiState.chainSettingsDirty = false;

} //   flushPendingChainSettings()

//-- Select next/previous chain target pattern in the same series letter.
static void selectNextPatternInSeries(int direction)
{
  String availableNames[patternStoreMaxEntries];
  int availableCount;
  int currentIndex = -1;

  if (direction == 0)
  {
    return;
  }

  availableCount = getAvailablePatternsForCurrentSeries(availableNames);

  if (availableCount <= 0)
  {
    uiState.chainTargetValid = false;
    return;
  }

  for (int nameIndex = 0; nameIndex < availableCount; nameIndex++)
  {
    if (availableNames[nameIndex] == uiState.chainTargetPatternName)
    {
      currentIndex = nameIndex;
      break;
    }
  }

  if (currentIndex < 0)
  {
    currentIndex = (direction > 0) ? 0 : (availableCount - 1);
  }
  else
  {
    currentIndex += (direction > 0) ? 1 : -1;

    if (currentIndex < 0)
    {
      currentIndex = availableCount - 1;
    }
    else if (currentIndex >= availableCount)
    {
      currentIndex = 0;
    }
  }

  uiState.chainTargetPatternName = availableNames[currentIndex];
  uiState.chainTargetValid = true;

} //   selectNextPatternInSeries()

//-- Draw status popup overlay with up to 3 wrapped lines split by '\n'.
static void drawPatternStatusPopupOverlay()
{
  String popupLines[3];
  int lineCount = 0;
  int cursor_start = 0;

  while (lineCount < 3)
  {
    int newLineIndex = uiState.patternStatusText.indexOf('\n', cursor_start);

    if (newLineIndex < 0)
    {
      String tail = uiState.patternStatusText.substring(cursor_start);

      if (tail.length() > 0)
      {
        popupLines[lineCount++] = fitListRowText(tail);
      }

      break;
    }

    String segment = uiState.patternStatusText.substring(cursor_start, newLineIndex);
    popupLines[lineCount++] = fitListRowText(segment);
    cursor_start = newLineIndex + 1;
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

//-- Build visible pattern group input text with cursor marker.
static String buildPatternGroupNameInputText()
{
  String outputText = "";

  for (int charIndex = 0; charIndex < patternGroupNameInputLength; charIndex++)
  {
    char currentChar = uiState.patternGroupNameInputValue[charIndex];

    if (currentChar == '\0' || currentChar == ' ')
    {
      currentChar = '-';
    }

    if (charIndex == uiState.patternGroupNameInputCursor)
    {
      outputText += "[";
      outputText += currentChar;
      outputText += "]";
    }
    else
    {
      outputText += currentChar;
    }
  }

  return outputText;

} //   buildPatternGroupNameInputText()

//-- Return trimmed pattern group name input.
static String getTrimmedPatternGroupNameInput()
{
  String groupName = "";

  for (int charIndex = 0; charIndex < patternGroupNameInputLength; charIndex++)
  {
    char currentChar = uiState.patternGroupNameInputValue[charIndex];

    if (currentChar != '\0' && currentChar != ' ')
    {
      groupName += currentChar;
    }
  }

  groupName.trim();

  return groupName;

} //   getTrimmedPatternGroupNameInput()

//-- Open pattern group name input for Rename or Copy.
static void openPatternGroupNameInput(bool copyMode)
{
  for (int charIndex = 0; charIndex < patternGroupNameInputLength; charIndex++)
  {
    uiState.patternGroupNameInputValue[charIndex] = ' ';
  }

  uiState.patternGroupNameInputValue[patternGroupNameInputLength] = '\0';
  uiState.patternGroupNameInputOpen = true;
  uiState.patternGroupNameInputCopyMode = copyMode;
  uiState.patternGroupNameInputCursor = 0;
  uiState.patternGroupNameInputTokenIndex = 0;
  uiState.dirty = true;

} //   openPatternGroupNameInput()

//-- Draw pattern group name input screen.
static void drawPatternGroupNameInput()
{
  String inputText = buildPatternGroupNameInputText();
  String currentToken =
      String(patternGroupNameInputTokens[uiState.patternGroupNameInputTokenIndex]);
  const char* title = uiState.patternGroupNameInputCopyMode ? "Copy Pattern" : "Rename Pattern";

  display.drawTextInput(title, inputText.c_str(), currentToken.c_str());

} //   drawPatternGroupNameInput()

//-- Apply encoder rotation to current pattern group input character.
static void rotatePatternGroupNameInput(int delta)
{
  int tokenCount = static_cast<int>(strlen(patternGroupNameInputTokens));

  uiState.patternGroupNameInputTokenIndex += delta;

  while (uiState.patternGroupNameInputTokenIndex < 0)
  {
    uiState.patternGroupNameInputTokenIndex += tokenCount;
  }

  while (uiState.patternGroupNameInputTokenIndex >= tokenCount)
  {
    uiState.patternGroupNameInputTokenIndex -= tokenCount;
  }

  uiState.patternGroupNameInputValue[uiState.patternGroupNameInputCursor] =
      patternGroupNameInputTokens[uiState.patternGroupNameInputTokenIndex];

  uiState.dirty = true;

} //   rotatePatternGroupNameInput()

//-- Accept current character and advance cursor.
static void acceptPatternGroupNameInputCharacter()
{
  uiState.patternGroupNameInputValue[uiState.patternGroupNameInputCursor] =
      patternGroupNameInputTokens[uiState.patternGroupNameInputTokenIndex];

  if (uiState.patternGroupNameInputCursor < patternGroupNameInputLength - 1)
  {
    uiState.patternGroupNameInputCursor++;
  }

  uiState.dirty = true;

} //   acceptPatternGroupNameInputCharacter()

//-- Backspace or cancel pattern group name input.
static void backspacePatternGroupNameInput()
{
  if (uiState.patternGroupNameInputCursor <= 0)
  {
    uiState.patternGroupNameInputOpen = false;
    uiState.dirty = true;
    return;
  }

  uiState.patternGroupNameInputValue[uiState.patternGroupNameInputCursor] = ' ';
  uiState.patternGroupNameInputCursor--;
  uiState.patternGroupNameInputValue[uiState.patternGroupNameInputCursor] = ' ';

  uiState.dirty = true;

} //   backspacePatternGroupNameInput()

//-- Commit Rename or Copy pattern group input.
static void commitPatternGroupNameInput()
{
  String oldGroupName = settingsStoreGetActivePatternGroup();
  String newGroupName = getTrimmedPatternGroupNameInput();
  bool copyMode = uiState.patternGroupNameInputCopyMode;
  bool success = false;

  if (newGroupName.isEmpty())
  {
    uiState.patternGroupNameInputOpen = false;
    showPatternStatus("Name empty", 2000);
    uiState.dirty = true;

    return;
  }

  //-- Close input first so the busy popup can be drawn before the SD action starts.
  uiState.patternGroupNameInputOpen = false;
  uiState.patternStatusOpen = false;
  uiState.patternStatusText = "";
  uiState.dirty = true;

  if (copyMode)
  {
    display.drawMessage("Copy Pattern", "Copying...");
  }
  else
  {
    display.drawMessage("Rename Pattern", "Renaming...");
  }

  delay(50);

  if (copyMode)
  {
    success = settingsStoreCopyPatternGroupOnCard(oldGroupName, newGroupName);
  }
  else
  {
    success = settingsStoreRenamePatternGroupOnCard(oldGroupName, newGroupName);
  }

  if (!success)
  {
    showPatternStatus("Group failed\n" + newGroupName, 2500);
    uiState.dirty = true;

    return;
  }

  settingsStoreSetActivePatternGroup(newGroupName);

  if (copyMode)
  {
    loadCardPatternGroupIntoMemory(newGroupName, false);
    showPatternStatus("Copied group\n" + newGroupName, 2500);
  }
  else
  {
    showPatternStatus("Renamed group\n" + newGroupName, 2500);
  }

  uiState.patternListNeedsRefresh = true;
  uiState.dirty = true;

} //   commitPatternGroupNameInput()

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
    uiState.menuSelection = 0;
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
      uiState.menuSelection = 0;
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
  else if (selectedIndex >= (firstVisibleIndex + lowerScrollTrigger) &&
           firstVisibleIndex < maxFirstVisible)
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

//-- Return pattern name for a loaded pattern slot.
static String getPatternNameForSlot(uint8_t slotIndex)
{
  char fallbackName[8];

  if (slotIndex < sequencerPatternCount && !uiState.chainSlotPatternNames[slotIndex].isEmpty())
  {
    return uiState.chainSlotPatternNames[slotIndex];
  }

  snprintf(fallbackName, sizeof(fallbackName), "p%02u", static_cast<unsigned>(slotIndex + 1U));

  return String(fallbackName);

} //   getPatternNameForSlot()

//-- Count loaded pattern slots currently known by the UI.
static uint8_t getLoadedPatternSlotCount()
{
  uint8_t loadedPatternCount = 0;

  for (uint8_t slotIndex = 0; slotIndex < sequencerPatternCount; slotIndex++)
  {
    if (!uiState.chainSlotPatternNames[slotIndex].isEmpty())
    {
      loadedPatternCount = static_cast<uint8_t>(slotIndex + 1U);
    }
  }

  if (loadedPatternCount < 1)
  {
    loadedPatternCount = 1;
  }

  return loadedPatternCount;

} //   getLoadedPatternSlotCount()

//-- Refresh cached pattern names.
static void refreshPatternList()
{
  size_t listedCount = 0;

  uiState.patternCount = 0;

  if (uiState.patternListSourceFilter == patternListModeMemoryPatterns)
  {
    uint8_t loadedPatternCount = getLoadedPatternSlotCount();

    for (uint8_t slotIndex = 0;
         slotIndex < loadedPatternCount && uiState.patternCount < patternListMaxEntries;
         slotIndex++)
    {
      uiState.patternNames[uiState.patternCount] = getPatternNameForSlot(slotIndex);
      uiState.patternSources[uiState.patternCount] = PatternEntrySource::Local;
      uiState.patternCount++;
    }

    if (uiState.patternListSelection >= uiState.patternCount)
    {
      uiState.patternListSelection = (uiState.patternCount > 0) ? (uiState.patternCount - 1) : 0;
    }

    return;
  }

  if (uiState.patternListSourceFilter == patternListModeCardGroups)
  {
    if (settingsStoreListPatternGroupsOnCard(patternScanBuffer, patternStoreMaxEntries,
                                             listedCount))
    {
      for (size_t index = 0; index < listedCount && uiState.patternCount < patternListMaxEntries;
           index++)
      {
        uiState.patternNames[uiState.patternCount] = patternScanBuffer[index];
        uiState.patternSources[uiState.patternCount] = PatternEntrySource::Card;
        uiState.patternCount++;
      }
    }

    for (int leftIndex = 0; leftIndex < uiState.patternCount; leftIndex++)
    {
      for (int rightIndex = leftIndex + 1; rightIndex < uiState.patternCount; rightIndex++)
      {
        if (uiState.patternNames[rightIndex].compareTo(uiState.patternNames[leftIndex]) < 0)
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

    for (int patternIndex = 0; patternIndex < patternListMaxEntries; patternIndex++)
    {
      uiState.patternHasChainTarget[patternIndex] = false;
      uiState.patternChainTargets[patternIndex] = "";
    }

    return;
  }

  bool includeLocal =
      (uiState.patternListSourceFilter < 0 ||
       uiState.patternListSourceFilter == static_cast<int>(PatternStorageTarget::Local));

  bool includeCard =
      (uiState.patternListSourceFilter < 0 ||
       uiState.patternListSourceFilter == static_cast<int>(PatternStorageTarget::Card));

  if (includeLocal &&
      settingsStoreListPatterns(patternScanBuffer, patternStoreMaxEntries, listedCount))
  {
    for (size_t index = 0; index < listedCount && uiState.patternCount < patternListMaxEntries;
         index++)
    {
      uiState.patternNames[uiState.patternCount] = patternScanBuffer[index];
      uiState.patternSources[uiState.patternCount] = PatternEntrySource::Local;
      uiState.patternCount++;
    }
  }

  if (includeCard)
  {
    String groupName = settingsStoreGetActivePatternGroup();

    if (settingsStoreListPatternsInGroupOnCard(groupName, patternScanBuffer, patternStoreMaxEntries,
                                               listedCount))
    {
      for (size_t index = 0; index < listedCount && uiState.patternCount < patternListMaxEntries;
           index++)
      {
        uiState.patternNames[uiState.patternCount] = patternScanBuffer[index];
        uiState.patternSources[uiState.patternCount] = PatternEntrySource::Card;
        uiState.patternCount++;
      }
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
      else if (uiState.patternNames[rightIndex] == uiState.patternNames[leftIndex] &&
               uiState.patternSources[rightIndex] < uiState.patternSources[leftIndex])
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

  for (int patternIndex = 0; patternIndex < uiState.patternCount; patternIndex++)
  {
    uiState.patternHasChainTarget[patternIndex] = false;
    uiState.patternChainTargets[patternIndex] = "";
  }

  refreshChainSeriesPatternCache();

  uiState.chainTargetValid = isCurrentChainTargetValid();

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
    String groupName = settingsStoreGetActivePatternGroup();
    if (!settingsStoreSavePatternToCard(groupName, targetName, patternData))
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
  saveChainSettingsForPattern();
  loadChainSettingsForActivePattern();
  saveRuntimeSettingsFromCurrentState();
  uiState.patternListNeedsRefresh = true;

  return true;

} //   saveActivePattern()

//-- Create a new pattern copy with automatic name.
static bool createNewPatternWithLetter(char patternLetter)
{
  String targetName;
  PatternData patternData;
  SequencerView view;
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
  patternData.chainEnabled = false;
  patternData.chainLength = 1;

  if (storageTarget == PatternStorageTarget::Card)
  {
    String groupName = settingsStoreGetActivePatternGroup();
    if (!settingsStoreSavePatternToCard(groupName, targetName, patternData))
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

  uiState.chainTargetPatternName = "";
  uiState.chainTargetValid = false;
  uiState.chainSettingsDirty = false;

  sequencerGetView(view);

  if (view.activePatternIndex < sequencerPatternCount)
  {
    uiState.chainSlotTargetPatternNames[view.activePatternIndex] = "";
  }

  loadChainSettingsForActivePattern();
  saveRuntimeSettingsFromCurrentState();
  uiState.patternListNeedsRefresh = true;

  return true;

} //   createNewPatternWithLetter()

//-- Load one pattern from current list selection.
static bool loadSelectedPattern()
{
  PatternData patternData;
  PatternEntrySource selectedSource;

  if (uiState.patternCount <= 0 || uiState.patternListSelection < 0 ||
      uiState.patternListSelection >= uiState.patternCount)
  {
    return false;
  }

  String selectedName = uiState.patternNames[uiState.patternListSelection];
  selectedSource = uiState.patternSources[uiState.patternListSelection];

  if (selectedSource == PatternEntrySource::Card)
  {
    String groupName = settingsStoreGetActivePatternGroup();
    if (!settingsStoreLoadPatternFromCard(groupName, selectedName, patternData))
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
  refreshChainSeriesPatternCache();
  loadChainSettingsForActivePattern();
  saveRuntimeSettingsFromCurrentState();

  return true;

} //   loadSelectedPattern()

//-- Build a canonical pNN name for one zero-based pattern slot.
static String buildPatternNameForSlot(uint8_t slotIndex)
{
  char patternName[8];

  snprintf(patternName, sizeof(patternName), "p%02u", static_cast<unsigned>(slotIndex + 1U));

  return String(patternName);

} //   buildPatternNameForSlot()

//-- Rebuild loaded RAM pattern names after add/delete operations.
static void rebuildLoadedPatternNames(uint8_t loadedPatternCount)
{
  if (loadedPatternCount > sequencerPatternCount)
  {
    loadedPatternCount = sequencerPatternCount;
  }

  for (uint8_t slotIndex = 0; slotIndex < sequencerPatternCount; slotIndex++)
  {
    if (slotIndex < loadedPatternCount)
    {
      uiState.chainSlotPatternNames[slotIndex] = buildPatternNameForSlot(slotIndex);
    }
    else
    {
      uiState.chainSlotPatternNames[slotIndex] = "";
      uiState.chainSlotTargetPatternNames[slotIndex] = "";
    }
  }

} //   rebuildLoadedPatternNames()

//-- Add a new RAM pattern after the last loaded pattern.
static bool addPatternInMemory()
{
  SequencerView view;
  uint8_t loadedPatternCount = getLoadedPatternSlotCount();
  uint8_t newSlotIndex = loadedPatternCount;
  uint8_t newPatternNumber = static_cast<uint8_t>(loadedPatternCount + 1U);

  sequencerGetView(view);

  if (view.playing)
  {
    sequencerStopImmediately();
  }

  flushPendingChainSettings();

  if (loadedPatternCount >= sequencerPatternCount)
  {
    showPatternStatus("Pattern memory\nfull", 2500);
    return false;
  }

  sequencerCreatePatternSlot(newSlotIndex, newPatternNumber);

  uiState.chainSlotPatternNames[newSlotIndex] = buildPatternNameForSlot(newSlotIndex);
  uiState.chainSlotTargetPatternNames[newSlotIndex] = "";
  uiState.activePatternName = uiState.chainSlotPatternNames[newSlotIndex];

  refreshChainSeriesPatternCache();
  loadChainSettingsForActivePattern();

  showPatternStatus("Added pattern\n" + uiState.activePatternName, 2000);

  uiState.dirty = true;

  return true;

} //   addPatternInMemory()

//-- Open RAM pattern delete selector.
static void openDeletePatternFromMemory()
{
  uiState.patternListOpen = true;
  uiState.patternDeleteMode = true;
  uiState.patternListSourceFilter = patternListModeMemoryPatterns;
  uiState.patternListSelection = 0;
  uiState.patternListFirstVisibleIndex = 0;
  uiState.patternListNeedsRefresh = true;

} //   openDeletePatternFromMemory()

//-- Delete selected RAM pattern and compact remaining RAM patterns.
static bool deleteSelectedPatternFromMemory()
{
  SequencerView view;
  uint8_t loadedPatternCount = getLoadedPatternSlotCount();
  uint8_t deleteSlotIndex;

  if (uiState.patternCount <= 0 || uiState.patternListSelection < 0 ||
      uiState.patternListSelection >= uiState.patternCount)
  {
    return false;
  }

  deleteSlotIndex = static_cast<uint8_t>(uiState.patternListSelection);

  sequencerGetView(view);

  if (view.playing)
  {
    sequencerStopImmediately();
  }

  flushPendingChainSettings();

  sequencerDeletePatternSlot(deleteSlotIndex, loadedPatternCount);

  if (loadedPatternCount > 1U)
  {
    loadedPatternCount--;
  }
  else
  {
    loadedPatternCount = 1;
  }

  rebuildLoadedPatternNames(loadedPatternCount);

  sequencerGetView(view);
  uiState.activePatternName = getPatternNameForSlot(view.activePatternIndex);
  uiState.chainTargetPatternName = "";
  uiState.chainTargetValid = false;
  uiState.chainSettingsDirty = false;

  refreshChainSeriesPatternCache();
  loadChainSettingsForActivePattern();

  uiState.patternListNeedsRefresh = true;
  uiState.dirty = true;

  showPatternStatus("Deleted pattern", 2000);

  return true;

} //   deleteSelectedPatternFromMemory()

//-- Move Groovebox cursor across tracks and loaded pattern slots.
static void moveGrooveboxCursorAcrossPatterns(int delta)
{
  SequencerView view;
  uint8_t loadedPatternCount = getLoadedPatternSlotCount();

  flushPendingChainSettings();

  sequencerMoveTrackAndPattern(delta, loadedPatternCount);

  sequencerGetView(view);

  uiState.activePatternName = getPatternNameForSlot(view.activePatternIndex);

  loadChainSettingsForActivePattern();

  uiState.dirty = true;

} //   moveGrooveboxCursorAcrossPatterns()

//-- Load selected Card pattern group directly into sequencer memory.
static bool loadSelectedCardPatternGroup()
{
  SequencerView view;

  if (uiState.patternCount <= 0 || uiState.patternListSelection < 0 ||
      uiState.patternListSelection >= uiState.patternCount)
  {
    return false;
  }

  String selectedGroupName = uiState.patternNames[uiState.patternListSelection];

  sequencerGetView(view);

  if (view.playing)
  {
    sequencerStopImmediately();
  }

  if (!loadCardPatternGroupIntoMemory(selectedGroupName, true))
  {
    return false;
  }

  if (!settingsStoreSetActivePatternGroup(selectedGroupName))
  {
    showPatternStatus("NVS save failed\n" + selectedGroupName, 2500);
    return false;
  }

  saveRuntimeSettingsFromCurrentState();

  ESP_LOGI(logTag, "Active Card pattern group stored in NVS: %s", selectedGroupName.c_str());

  return true;

} //   loadSelectedCardPatternGroup()

//-- Load one Card pattern group directly into sequencer memory by group name.
static bool loadCardPatternGroupIntoMemory(const String& groupName, bool showStatus)
{
  PatternData patternData;
  String cardPatternNames[patternStoreMaxEntries];
  size_t cardPatternCount = 0;

  if (groupName.isEmpty())
  {
    if (showStatus)
    {
      showPatternStatus("No active\ngroup", 2500);
    }

    return false;
  }

  if (!settingsStoreListPatternsInGroupOnCard(groupName, cardPatternNames, patternStoreMaxEntries,
                                              cardPatternCount))
  {
    if (showStatus)
    {
      showPatternStatus("List failed\n" + groupName, 2500);
    }

    return false;
  }

  if (cardPatternCount == 0)
  {
    if (showStatus)
    {
      showPatternStatus("Empty group\n" + groupName, 2500);
    }

    return false;
  }

  for (uint8_t slotIndex = 0; slotIndex < sequencerPatternCount; slotIndex++)
  {
    uiState.chainSlotPatternNames[slotIndex] = "";
    uiState.chainSlotTargetPatternNames[slotIndex] = "";
  }

  for (size_t patternIndex = 0;
       patternIndex < cardPatternCount && patternIndex < sequencerPatternCount; patternIndex++)
  {
    if (!settingsStoreLoadPatternFromCard(groupName, cardPatternNames[patternIndex], patternData))
    {
      if (showStatus)
      {
        showPatternStatus("Load failed\n" + cardPatternNames[patternIndex], 2500);
      }

      return false;
    }

    sequencerImportPatternToSlot(static_cast<uint8_t>(patternIndex), patternData);

    uiState.chainSlotPatternNames[patternIndex] = cardPatternNames[patternIndex];
    uiState.chainSlotTargetPatternNames[patternIndex] = patternData.chainTarget;
  }

  sequencerSetActivePatternIndex(0);

  uiState.activePatternName = cardPatternNames[0];
  uiState.chainTargetPatternName = "";
  uiState.chainTargetValid = false;
  uiState.chainSettingsDirty = false;
  uiState.patternListNeedsRefresh = true;

  refreshChainSeriesPatternCache();
  loadChainSettingsForActivePattern();

  if (showStatus)
  {
    showPatternStatus("Loaded group\n" + groupName, 2500);
  }

  ESP_LOGI(logTag, "Loaded Card group %s into RAM (%u patterns)", groupName.c_str(),
           static_cast<unsigned>(cardPatternCount));

  return true;

} //   loadCardPatternGroupIntoMemory()

//-- Save loaded in-memory pattern slots to active Card pattern group.
static bool saveLoadedPatternGroupToCard()
{
  PatternData patternData;
  String groupName = settingsStoreGetActivePatternGroup();
  int savedCount = 0;

  if (groupName.isEmpty())
  {
    showPatternStatus("No active\ngroup name", 2500);
    return false;
  }

  SequencerView view;

  sequencerGetView(view);

  if (view.playing)
  {
    sequencerStopImmediately();
  }

  flushPendingChainSettings();
  uint8_t loadedPatternCount = getLoadedPatternSlotCount();

  for (uint8_t slotIndex = 0; slotIndex < loadedPatternCount; slotIndex++)
  {
    String patternName = uiState.chainSlotPatternNames[slotIndex];

    if (patternName.isEmpty())
    {
      char patternNameBuffer[8];

      snprintf(patternNameBuffer, sizeof(patternNameBuffer), "p%02u",
               static_cast<unsigned>(slotIndex + 1U));

      patternName = String(patternNameBuffer);
    }

    sequencerExportPatternFromSlot(slotIndex, patternData);
    patternData.chainTarget = uiState.chainSlotTargetPatternNames[slotIndex];

    if (!settingsStoreSavePatternToCard(groupName, patternName, patternData))
    {
      showPatternStatus("Save failed\n" + patternName, 2500);
      return false;
    }

    savedCount++;
  }

  String existingPatternNames[patternStoreMaxEntries];
  size_t existingPatternCount = 0;

  if (settingsStoreListPatternsInGroupOnCard(groupName, existingPatternNames,
                                             patternStoreMaxEntries, existingPatternCount))
  {
    for (size_t patternIndex = 0; patternIndex < existingPatternCount; patternIndex++)
    {
      String existingName = existingPatternNames[patternIndex];
      int existingNumber = existingName.substring(1).toInt();

      if (existingNumber > loadedPatternCount)
      {
        settingsStoreDeletePatternFromCard(groupName, existingName);
      }
    }
  }
  saveRuntimeSettingsFromCurrentState();

  showPatternStatus("Saved group\n" + groupName, 2500);

  ESP_LOGI(logTag, "Saved %d in-memory patterns to Card group %s", savedCount, groupName.c_str());

  return true;

} //   saveLoadedPatternGroupToCard()

//-- Delete one pattern from current list selection.
static bool deleteSelectedPattern(String* outDeletedName = nullptr,
                                  PatternEntrySource* outDeletedSource = nullptr)
{
  PatternEntrySource selectedSource;

  if (uiState.patternCount <= 0 || uiState.patternListSelection < 0 ||
      uiState.patternListSelection >= uiState.patternCount)
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
    String groupName = settingsStoreGetActivePatternGroup();
    if (!settingsStoreDeletePatternFromCard(groupName, selectedName))
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
    uiState.chainTargetPatternName = "";
    uiState.chainTargetValid = false;
    uiState.chainSettingsDirty = false;
    for (uint8_t slotIndex = 0; slotIndex < sequencerPatternCount; slotIndex++)
    {
      if (uiState.chainSlotPatternNames[slotIndex] == selectedName)
      {
        uiState.chainSlotTargetPatternNames[slotIndex] = "";
      }
    }
    saveRuntimeSettingsFromCurrentState();
  }

  uiState.patternListNeedsRefresh = true;

  return true;

} //   deleteSelectedPattern()

//-- Return true when chain mode is musically active.
static bool isPatternChainEnabled(const SequencerView& view)
{
  return view.chainEnabled && view.chainLength > 1U;

} //   isPatternChainEnabled()

//-- Resolve active pattern display text or fallback to slot label.
static String getCurrentPatternDisplayName(const SequencerView& view)
{
  char slotLabel[8];

  if (!uiState.activePatternName.isEmpty())
  {
    return uiState.activePatternName;
  }

  snprintf(slotLabel, sizeof(slotLabel), "p%02u",
           static_cast<unsigned>(view.activePatternIndex + 1U));
  return String(slotLabel);

} //   getCurrentPatternDisplayName()

//-- Resolve the active Card pattern group name for compact display.
static String getActivePatternGroupDisplayName()
{
  String groupName = settingsStoreGetActivePatternGroup();

  if (groupName.isEmpty())
  {
    return "";
  }

  return groupName;

} //   getActivePatternGroupDisplayName()

//-- Resolve next pattern display text for chain preview.
static String getNextPatternDisplayName(const SequencerView& view)
{
  if (!isPatternChainEnabled(view))
  {
    return "";
  }

  if (!uiState.chainTargetValid || uiState.chainTargetPatternName.isEmpty())
  {
    return "";
  }

  return uiState.chainTargetPatternName;

} //   getNextPatternDisplayName()

//-- Build compact Groovebox footer line with playback and chain context.
static String formatGrooveboxFooter(const SequencerView& view)
{
  char footerLine[64];
  String playingPatternName = getPatternNameForSlot(view.playingPatternIndex);
  String nextPatternName = "";
  String groupName = settingsStoreGetActivePatternGroup();

  if (view.editMode)
  {
    snprintf(footerLine, sizeof(footerLine), "%s S:%02u", trackNames[view.selectedTrack],
             static_cast<unsigned>(view.cursorStep + 1U));

    return String(footerLine);
  }

  if (view.chainEnabled && view.chainLength > 1U)
  {
    uint8_t nextSlotIndex =
        static_cast<uint8_t>((view.playingPatternIndex + 1U) % view.chainLength);

    nextPatternName = getPatternNameForSlot(nextSlotIndex);
  }

  if (!nextPatternName.isEmpty() && !groupName.isEmpty())
  {
    snprintf(footerLine, sizeof(footerLine), "S:%02u %s->%s %s",
             static_cast<unsigned>(view.currentStep + 1U), playingPatternName.c_str(),
             nextPatternName.c_str(), groupName.c_str());
  }
  else if (!nextPatternName.isEmpty())
  {
    snprintf(footerLine, sizeof(footerLine), "S:%02u %s->%s",
             static_cast<unsigned>(view.currentStep + 1U), playingPatternName.c_str(),
             nextPatternName.c_str());
  }
  else if (!groupName.isEmpty())
  {
    snprintf(footerLine, sizeof(footerLine), "S:%02u %s %s",
             static_cast<unsigned>(view.currentStep + 1U), playingPatternName.c_str(),
             groupName.c_str());
  }
  else
  {
    snprintf(footerLine, sizeof(footerLine), "S:%02u %s",
             static_cast<unsigned>(view.currentStep + 1U), playingPatternName.c_str());
  }

  return String(footerLine);

} //   formatGrooveboxFooter()

//-- Build contextual parameter overlay text for the selected step.
static String buildParameterOverlayLine(const SequencerView& view)
{
  const Track& selectedTrack = view.pattern->tracks[view.selectedTrack];
  const Step& selectedStep = selectedTrack.steps[view.cursorStep];
  char lineBuffer[72];

  if (uiState.parameterPageIndex == parameterPageTrig)
  {
    return "";
  }
  else if (uiState.parameterPageIndex == parameterPageVelocity)
  {
    snprintf(lineBuffer, sizeof(lineBuffer), "VEL   %03u",
             static_cast<unsigned>(selectedStep.velocity));
  }
  else if (uiState.parameterPageIndex == parameterPagePitch)
  {
    snprintf(lineBuffer, sizeof(lineBuffer), "PITCH %+03d  LOCK %s",
             static_cast<int>(selectedStep.lockPitch), selectedStep.lockEnabled ? "ON" : "OFF");
  }
  else if (uiState.parameterPageIndex == parameterPageDecay)
  {
    snprintf(lineBuffer, sizeof(lineBuffer), "DECAY %03u%% LOCK %s",
             static_cast<unsigned>(selectedStep.lockDecay),
             selectedStep.lockEnabled ? "ON" : "OFF");
  }
  else if (uiState.parameterPageIndex == parameterPageProbability)
  {
    snprintf(lineBuffer, sizeof(lineBuffer), "PROB  %03u%%",
             static_cast<unsigned>(selectedStep.probability));
  }
  else if (uiState.parameterPageIndex == parameterPageMute)
  {
    snprintf(lineBuffer, sizeof(lineBuffer), "MUTE  %s", selectedTrack.mute ? "ON" : "OFF");
  }
  else
  {
    snprintf(lineBuffer, sizeof(lineBuffer), "CHAIN %s L%u P%u", view.chainEnabled ? "ON" : "OFF",
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

//-- Stop playback immediately before storage-related UI actions.
static void stopPlaybackForStorageAction()
{
  SequencerView view;

  sequencerGetView(view);

  if (view.playing)
  {
    sequencerStopImmediately();
  }

} //   stopPlaybackForStorageAction()

//-- Start immediately or request musical deferred stop from the Groovebox screen.
static void handleGrooveboxTransportButton()
{
  SequencerView view;
  uint8_t finalPatternIndex = 0;

  sequencerGetView(view);

  if (!view.playing)
  {
    sequencerTogglePlay();
    return;
  }

  if (!settingsStoreFindHighestLocalPatternIndex(finalPatternIndex))
  {
    finalPatternIndex = view.activePatternIndex;
  }

  sequencerRequestStopAfterFinalPattern(finalPatternIndex);

} //   handleGrooveboxTransportButton()

//-- Refresh sample set list from SD card.
static void refreshSampleSetList()
{
  char sampleSetNames[sampleSetListMaxEntries][4] = {0};
  uint8_t sampleSetCount = 0;

  uiState.sampleSetCount = 0;
  uiState.sampleSetListSelection = 0;
  uiState.sampleSetListFirstVisibleIndex = 0;

  if (!sampleManagerListSampleSets(sampleSetNames, sampleSetListMaxEntries, &sampleSetCount))
  {
    return;
  }

  for (uint8_t sampleSetIndex = 0; sampleSetIndex < sampleSetCount; sampleSetIndex++)
  {
    uiState.sampleSetNames[sampleSetIndex] = String(sampleSetNames[sampleSetIndex]);
    uiState.sampleSetDisplayItems[sampleSetIndex] =
        fitListRowText(uiState.sampleSetNames[sampleSetIndex]);
    uiState.sampleSetCount++;
  }

} //   refreshSampleSetList()

//-- Load selected sample set from menu.
//-- Store selected sample set and restart safely.
static void loadSelectedSampleSetFromMenu()
{
  if (uiState.sampleSetCount <= 0 || uiState.sampleSetListSelection < 0 ||
      uiState.sampleSetListSelection >= uiState.sampleSetCount)
  {
    showPatternStatus("No sample set", 2000);
    return;
  }

  String selectedSampleSet = uiState.sampleSetNames[uiState.sampleSetListSelection];

  stopPlaybackForStorageAction();

  if (settingsStoreSetActiveSampleSet(selectedSampleSet.c_str()))
  {
    showPatternStatus("Restarting..\nSample set " + selectedSampleSet, 1000);
    systemManagerQueueCommand(SystemCommand::restartNow);
  }
  else
  {
    showPatternStatus("Save sample set\nfailed", 2000);
  }

  uiState.sampleSetListOpen = false;
  uiState.sampleSetListFirstVisibleIndex = 0;

} //   loadSelectedSampleSetFromMenu()

//-- Draw Card Storage submenu.
static void drawCardStorageMenu()
{
  String items[cardStorageMenuEntryCount];

  items[0] = "Load Pattern";
  items[1] = "Save Pattern";
  items[2] = "Rename Pattern";
  items[3] = "Copy Pattern";
  items[4] = "Exit";

  updateListFirstVisibleIndex(uiState.cardStorageMenuSelection, cardStorageMenuEntryCount,
                              uiState.cardStorageMenuFirstVisibleIndex);

  display.drawListScreen("Card Storage", items, cardStorageMenuEntryCount,
                         uiState.cardStorageMenuSelection,
                         uiState.cardStorageMenuFirstVisibleIndex);

  if (uiState.patternStatusOpen)
  {
    drawPatternStatusPopupOverlay();
  }

} //   drawCardStorageMenu()

//-- Draw required system settings menu.
static void drawSystemSettingsScreen()
{
  // Card Storage is a submenu, not a replacement for System Settings
  if (uiState.cardStorageMenuOpen)
  {
    drawCardStorageMenu();
    return;
  }
  if (uiState.eraseWifiRestartPending)
  {
    display.drawWifiPortalScreen("Erasing credentials", "Restart Groovebox", "", "");
    return;
  }

  if (uiState.wifiManagerWaitingForCredentials)
  {
    String apSsid = systemManagerGetPortalApSsid();
    String line2 = String("Connect to ") + apSsid;

    display.drawWifiPortalScreen("WiFi portal started", line2.c_str(), "Enter credentials",
                                 "Waiting...");
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
  //----
  if (uiState.sampleSetListOpen)
  {
    int itemCount = uiState.sampleSetCount;

    if (itemCount <= 0)
    {
      uiState.sampleSetDisplayItems[0] = "(No sample sets)";
      itemCount = 1;
      uiState.sampleSetListSelection = 0;
    }
    else
    {
      updateListFirstVisibleIndex(uiState.sampleSetListSelection, itemCount,
                                  uiState.sampleSetListFirstVisibleIndex);
    }

    display.drawListScreen("Load Sample Set", uiState.sampleSetDisplayItems,
                           static_cast<size_t>(itemCount), uiState.sampleSetListSelection,
                           uiState.sampleSetListFirstVisibleIndex);

    if (uiState.patternStatusOpen)
    {
      drawPatternStatusPopupOverlay();
    }

    return;
  }
  //----

  if (uiState.patternListOpen)
  {
    if (uiState.patternListNeedsRefresh)
    {
      refreshPatternList();
      uiState.patternListNeedsRefresh = false;
    }

    int itemCount = uiState.patternCount;
    bool activeMarked = false;
    const char* title = (uiState.patternListSourceFilter == patternListModeCardGroups)
                            ? "Load Pattern"
                            : (uiState.patternDeleteMode
                                   ? "Delete Pattern"
                                   : (uiState.patternListSourceFilter ==
                                              static_cast<int>(PatternStorageTarget::Card)
                                          ? "Load from Card"
                                          : (uiState.patternListSourceFilter ==
                                                     static_cast<int>(PatternStorageTarget::Local)
                                                 ? "Load from Local"
                                                 : "Load Pattern")));

    if (itemCount <= 0)
    {
      uiState.patternListDisplayItems[0] = "(No patterns found)";
      itemCount = 1;
      uiState.patternListSelection = 0;
    }
    else
    {
      for (int itemIndex = 0; itemIndex < itemCount; itemIndex++)
      {
        String patternName = uiState.patternNames[itemIndex];
        String lineText;

        if (uiState.patternDeleteMode || uiState.patternListSourceFilter < 0)
        {
          lineText = (uiState.patternSources[itemIndex] == PatternEntrySource::Card) ? "Card:  "
                                                                                     : "Local: ";
        }

        lineText += patternName;

        if (uiState.patternHasChainTarget[itemIndex] &&
            !uiState.patternChainTargets[itemIndex].isEmpty())
        {
          lineText += " -> ";
          lineText += uiState.patternChainTargets[itemIndex];
        }

        if (!activeMarked && !uiState.activePatternName.isEmpty() &&
            patternName == uiState.activePatternName)
        {
          lineText += " *";
          activeMarked = true;
        }

        uiState.patternListDisplayItems[itemIndex] = fitListRowText(lineText);
      }

      updateListFirstVisibleIndex(uiState.patternListSelection, itemCount,
                                  uiState.patternListFirstVisibleIndex);
    }

    display.drawListScreen(title, uiState.patternListDisplayItems, static_cast<size_t>(itemCount),
                           uiState.patternListSelection, uiState.patternListFirstVisibleIndex);

    if (uiState.patternStatusOpen)
    {
      drawPatternStatusPopupOverlay();
    }

    return;
  }

  String items[settingsEntryCount];
  bool disabledItems[settingsEntryCount] = {true,  true,  true,  false, false, false, false,
                                            false, false, false, false, false, false, false};
  String ssidValue = systemManagerGetSsid();
  String ipValue = systemManagerGetIpAddress();
  String macValue = systemManagerGetMacAddress();
  String patternLabel =
      uiState.activePatternName.isEmpty() ? String("-") : uiState.activePatternName;
  int activeThemeIndex = displayGetThemeColorIndex();
  const char* themeName = colorProfiles[activeThemeIndex].colorName;
  int displayRotation = displayGetRotation();
  bool encoderReversed = input.getEncoderDirectionReversed();

  char cardStorageEntry[40];
  char themeEntry[32];
  char rotationEntry[40];
  char encoderOrderEntry[32];

  snprintf(cardStorageEntry, sizeof(cardStorageEntry), "Card Storage");
  snprintf(themeEntry, sizeof(themeEntry), "Set Theme (%s)", themeName);
  snprintf(rotationEntry, sizeof(rotationEntry), "Rotate Display (%d)", displayRotation);
  snprintf(encoderOrderEntry, sizeof(encoderOrderEntry), "Encoder Order (%s)",
           encoderReversed ? "B-A" : "A-B");

  items[0] = fitListRowText("SSID: " + ssidValue);
  items[1] = fitListRowText("IP: " + ipValue);
  items[2] = fitListRowText("MAC: " + macValue);
  items[3] = "Add Pattern";
  items[4] = "Delete Pattern";
  items[5] = fitListRowText(cardStorageEntry);
  items[6] = "Load Sample Set";
  items[7] = "Erase WiFi credentials";
  items[8] = "Start WiFiManager";
  items[9] = fitListRowText(themeEntry);
  items[10] = fitListRowText(rotationEntry);
  items[11] = fitListRowText(encoderOrderEntry);
  items[12] = "Restart Groovebox";
  items[13] = "Exit";

  updateListFirstVisibleIndex(uiState.menuSelection, settingsEntryCount,
                              uiState.menuFirstVisibleIndex);
  display.drawListScreenWithDisabledItems("System Settings", items, settingsEntryCount,
                                          uiState.menuSelection, uiState.menuFirstVisibleIndex,
                                          disabledItems);

  if (uiState.patternStatusOpen)
  {
    drawPatternStatusPopupOverlay();
  }

} //   drawSystemSettingsScreen()

//-- Draw the main Groovebox sequencer screen.
static void drawSequencerScreen()
{
  SequencerView view;
  String lines[9];
  String popupRows[editPopupEntryCount];
  String parameterLine;
  String viewPatternName;
  int selectedLine = 1;
  char headerLine[48];

  sequencerGetView(view);

  viewPatternName = getPatternNameForSlot(view.activePatternIndex);

  snprintf(headerLine, sizeof(headerLine), "BPM %03u SW %02u %s %s",
           static_cast<unsigned>(view.bpm), static_cast<unsigned>(view.swingPercent),
           view.playing ? "PLAY" : "STOP", viewPatternName.c_str());

  lines[0] = fitListRowText(headerLine);

  for (uint8_t trackIndex = 0; trackIndex < sequencerTrackCount; trackIndex++)
  {
    lines[trackIndex + 1] =
        fitListRowText(buildTrackRowText(trackNames[trackIndex], view.pattern->tracks[trackIndex]));
  }

  parameterLine = "";

  if (!uiState.tempoEditOpen && !uiState.editPopupOpen && view.editMode)
  {
    parameterLine = buildParameterOverlayLine(view);
  }

  lines[7] = parameterLine.isEmpty() ? String("") : fitListRowText(parameterLine);
  lines[8] = fitListRowText(formatGrooveboxFooter(view));

  selectedLine = static_cast<int>(view.selectedTrack) + 1;

  display.drawListScreen("Groovebox", lines, 9, selectedLine, 0, PROG_VERSION);

  if (uiState.tempoEditOpen)
  {
    display.drawTempoOverlay(view.bpm, view.swingPercent, uiState.tempoEditSelection == 0);
  }
  else if (uiState.editPopupOpen)
  {
    buildEditPopupRows(view, popupRows);

    display.drawSelectionOverlay("Edit Track", popupRows, editPopupEntryCount,
                                 uiState.editPopupSelection);
  }

  if (view.editMode && !uiState.tempoEditOpen && !uiState.editPopupOpen)
  {
    int stepCharIndex = 7 + static_cast<int>(view.cursorStep);

    if (stepCharIndex >= 0 && stepCharIndex < static_cast<int>(lines[selectedLine].length()))
    {
      display.drawListCharacterHighlight(selectedLine, stepCharIndex,
                                         lines[selectedLine][stepCharIndex]);
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
  display.drawSelectionOverlay("Edit Track", popupRows, editPopupEntryCount,
                               uiState.editPopupSelection);

} //   drawEditPopupOverlayOnly()

//-- Update only the dynamic Groovebox footer row while running.
static void drawSequencerFooterUpdate(const SequencerView& view)
{
  String footerLine = fitListRowText(formatGrooveboxFooter(view));

  if (footerLine == lastSequencerFooterLine)
  {
    return;
  }

  display.drawListLine(8, footerLine, false);
  lastSequencerFooterLine = footerLine;

} //   drawSequencerFooterUpdate()

//-- Execute selected System Settings menu action.
static void executeMenuAction()
{
  int activeThemeIndex;
  int nextThemeIndex;
  int nextRotation;

  if (uiState.menuSelection == 3)
  {
    addPatternInMemory();
  }
  else if (uiState.menuSelection == 4)
  {
    openDeletePatternFromMemory();
  }
  else if (uiState.menuSelection == 5)
  {
    uiState.cardStorageMenuOpen = true;
    uiState.cardStorageMenuSelection = 0;
    uiState.cardStorageMenuFirstVisibleIndex = 0;
    uiState.dirty = true;
    return;
  }
  else if (uiState.menuSelection == 6)
  {
    SequencerView view;

    sequencerGetView(view);

    if (view.playing)
    {
      sequencerStopImmediately();
    }

    refreshSampleSetList();
    uiState.sampleSetListOpen = true;
    uiState.dirty = true;
    return;
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

//-- Initialize UI state and load persisted Card pattern group from NVS.
void uiManagerInit()
{
  RuntimeSettings runtimeSettings;
  String activeGroupName;
  bool startupGroupLoaded = false;

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
  uiState.patternListSourceFilter = -1;
  uiState.patternListNeedsRefresh = true;
  uiState.sampleSetListOpen = false;
  uiState.patternStatusOpen = false;
  uiState.wifiManagerWaitingForCredentials = false;
  uiState.wifiManagerPortalSeenActive = false;
  uiState.eraseWifiRestartPending = false;
  uiState.wifiManagerConfirmSelection = 0;
  uiState.eraseWifiConfirmSelection = 0;
  uiState.patternListSelection = 0;
  uiState.menuFirstVisibleIndex = 0;
  uiState.patternListFirstVisibleIndex = 0;
  uiState.sampleSetListSelection = 0;
  uiState.sampleSetListFirstVisibleIndex = 0;
  uiState.sampleSetCount = 0;
  uiState.patternCount = 0;
  uiState.patternStatusUntilMs = 0;
  uiState.eraseWifiRestartAtMs = 0;
  uiState.menuSelection = settingsEntryCount - 1;
  uiState.lastDrawMs = 0;
  uiState.dirty = true;
  uiState.parameterPageIndex = parameterPageTrig;
  uiState.activePatternName = "";
  uiState.patternStatusText = "";
  uiState.chainTargetPatternName = "";
  uiState.chainTargetValid = false;
  uiState.chainSettingsDirty = false;
  uiState.chainSeriesPatternCount = 0;
  uiState.chainSeriesPatternLetter = '\0';
  uiState.chainSeriesPatternCacheValid = false;
  uiState.localStorageMenuOpen = false;
  uiState.cardStorageMenuOpen = false;
  uiState.cardStorageMenuSelection = 0;
  uiState.cardStorageMenuFirstVisibleIndex = 0;
  uiState.patternGroupNameInputOpen = false;
  uiState.patternGroupNameInputCopyMode = false;
  uiState.patternGroupNameInputCursor = 0;
  uiState.patternGroupNameInputTokenIndex = 0;

  for (int charIndex = 0; charIndex <= patternGroupNameInputLength; charIndex++)
  {
    uiState.patternGroupNameInputValue[charIndex] = '\0';
  }

  lastSequencerStep = 0xFF;
  lastSequencerCursor = 0xFF;
  lastSequencerPlaying = false;
  lastSequencerActivePatternIndex = 0xFF;
  sequencerScreenDrawn = false;
  lastSequencerFooterLine = "";

  for (uint8_t slotIndex = 0; slotIndex < sequencerPatternCount; slotIndex++)
  {
    uiState.chainSlotPatternNames[slotIndex] = "";
    uiState.chainSlotTargetPatternNames[slotIndex] = "";
  }

  for (int patternIndex = 0; patternIndex < patternListMaxEntries; patternIndex++)
  {
    uiState.patternHasChainTarget[patternIndex] = false;
    uiState.patternChainTargets[patternIndex] = "";
  }

  settingsStoreLoadRuntimeSettings(runtimeSettings);

  activeGroupName = settingsStoreGetActivePatternGroup();

  ESP_LOGI(logTag, "Startup active Card pattern group from NVS: %s", activeGroupName.c_str());

  if (!activeGroupName.isEmpty())
  {
    startupGroupLoaded = loadCardPatternGroupIntoMemory(activeGroupName, false);
  }

  refreshPatternList();

  if (!startupGroupLoaded)
  {
    ESP_LOGW(logTag,
             "No startup Card group loaded. Use System Settings -> Card Storage -> Load Pattern");
  }

  display.drawMessage("ESP32 Groovebox", startupGroupLoaded ? activeGroupName.c_str() : "Ready");

} //   uiManagerInit()

//-- Update UI rendering at controlled refresh cadence.
void uiManagerUpdate()
{
  uint32_t nowMs = millis();
  SequencerView view;
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

    if (view.activePatternIndex != lastSequencerActivePatternIndex)
    {
      syncActivePatternNameFromSlot(view.activePatternIndex);
      loadChainSettingsForActivePattern();
      lastSequencerActivePatternIndex = view.activePatternIndex;
    }

    footerStateChanged =
        (view.currentStep != lastSequencerStep) || (view.cursorStep != lastSequencerCursor);

    transportStateChanged = (view.playing != lastSequencerPlaying);

    if (transportStateChanged)
    {
      uiState.dirty = true;
    }
  }

  if (!uiState.dirty && !uiState.menuOpen && !uiState.tempoEditOpen && !uiState.editPopupOpen &&
      view.playing && footerStateChanged && sequencerScreenDrawn)
  {
    if (nowMs - uiState.lastDrawMs < uiRefreshIntervalMs)
    {
      return;
    }

    uiState.lastDrawMs = nowMs;

    drawSequencerFooterUpdate(view);

    lastSequencerStep = view.currentStep;
    lastSequencerCursor = view.cursorStep;

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

  uiState.lastDrawMs = nowMs;

  if (uiState.menuOpen)
  {
    sequencerScreenDrawn = false;

    if (uiState.patternGroupNameInputOpen)
    {
      drawPatternGroupNameInput();
    }
    else
    {
      drawSystemSettingsScreen();
    }
  }
  else
  {
    drawSequencerScreen();

    sequencerGetView(view);

    lastSequencerPlaying = view.playing;
    lastSequencerStep = view.currentStep;
    lastSequencerCursor = view.cursorStep;
    lastSequencerActivePatternIndex = view.activePatternIndex;
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
  if (uiState.patternGroupNameInputOpen)
  {
    if (encoderEvent == ENCODER_EVENT_LEFT)
    {
      rotatePatternGroupNameInput(-1);
    }
    else if (encoderEvent == ENCODER_EVENT_RIGHT)
    {
      rotatePatternGroupNameInput(1);
    }
    else if (encoderEvent == ENCODER_EVENT_SHORT_PRESS)
    {
      acceptPatternGroupNameInputCharacter();
    }
    else if (encoderEvent == ENCODER_EVENT_MEDIUM_PRESS || encoderEvent == ENCODER_EVENT_LONG_PRESS)
    {
      commitPatternGroupNameInput();
    }

    uiState.dirty = true;

    return;
  }

  SequencerView view;
  sequencerGetView(view);

  ESP_LOGI(logTag, "Encoder event=%d, menuOpen=%d", static_cast<int>(encoderEvent),
           uiState.menuOpen ? 1 : 0);

  if (encoderEvent == ENCODER_EVENT_LONG_PRESS)
  {
    if (uiState.editPopupOpen)
    {
      flushPendingChainSettings();
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

    flushPendingChainSettings();

    uiState.menuOpen = !uiState.menuOpen;
    uiState.tempoEditOpen = false;
    uiState.wifiManagerConfirmOpen = false;
    uiState.eraseWifiConfirmOpen = false;
    uiState.patternListOpen = false;
    uiState.patternDeleteMode = false;
    uiState.patternStatusOpen = false;
    uiState.patternStatusText = "";

    if (uiState.menuOpen)
    {
      uiState.menuSelection = settingsFirstActionIndex;
      uiState.menuFirstVisibleIndex = 0;
    }

    normalizeMenuSelection(0);
    updateListFirstVisibleIndex(uiState.menuSelection, settingsEntryCount,
                                uiState.menuFirstVisibleIndex);
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

    // Card Storage menu navigation
    if (uiState.cardStorageMenuOpen)
    {
      if (encoderEvent == ENCODER_EVENT_LEFT)
      {
        uiState.cardStorageMenuSelection--;

        if (uiState.cardStorageMenuSelection < 0)
        {
          uiState.cardStorageMenuSelection = cardStorageMenuEntryCount - 1;
        }

        updateListFirstVisibleIndex(uiState.cardStorageMenuSelection, cardStorageMenuEntryCount,
                                    uiState.cardStorageMenuFirstVisibleIndex);
      }
      else if (encoderEvent == ENCODER_EVENT_RIGHT)
      {
        uiState.cardStorageMenuSelection++;

        if (uiState.cardStorageMenuSelection >= cardStorageMenuEntryCount)
        {
          uiState.cardStorageMenuSelection = 0;
        }

        updateListFirstVisibleIndex(uiState.cardStorageMenuSelection, cardStorageMenuEntryCount,
                                    uiState.cardStorageMenuFirstVisibleIndex);
      }
      else if (encoderEvent == ENCODER_EVENT_SHORT_PRESS ||
               encoderEvent == ENCODER_EVENT_MEDIUM_PRESS)
      {
        if (uiState.cardStorageMenuSelection == 0)
        {
          uiState.patternListOpen = true;
          uiState.patternDeleteMode = false;
          uiState.patternListSourceFilter = patternListModeCardGroups;
          uiState.patternListSelection = 0;
          uiState.patternListFirstVisibleIndex = 0;
          uiState.patternListNeedsRefresh = true;
          uiState.cardStorageMenuOpen = false;
        }
        else if (uiState.cardStorageMenuSelection == 1)
        {
          saveLoadedPatternGroupToCard();
        }
        else if (uiState.cardStorageMenuSelection == 2)
        {
          openPatternGroupNameInput(false);
        }
        else if (uiState.cardStorageMenuSelection == 3)
        {
          openPatternGroupNameInput(true);
        }
        else if (uiState.cardStorageMenuSelection == 4)
        {
          uiState.cardStorageMenuOpen = false;
          uiState.cardStorageMenuSelection = 0;
          uiState.cardStorageMenuFirstVisibleIndex = 0;
        }
        else
        {
          showPatternStatus("Not implemented\nyet", 2000);
        }
      }
      uiState.dirty = true;

      return;
    }

    if (encoderEvent == ENCODER_EVENT_LEFT)
    {
      if (uiState.sampleSetListOpen)
      {
        if (uiState.sampleSetCount > 0)
        {
          uiState.sampleSetListSelection--;
          if (uiState.sampleSetListSelection < 0)
          {
            uiState.sampleSetListSelection = uiState.sampleSetCount - 1;
          }
          updateListFirstVisibleIndex(uiState.sampleSetListSelection, uiState.sampleSetCount,
                                      uiState.sampleSetListFirstVisibleIndex);
        }
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
          updateListFirstVisibleIndex(uiState.patternListSelection, uiState.patternCount,
                                      uiState.patternListFirstVisibleIndex);
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
        updateListFirstVisibleIndex(uiState.menuSelection, settingsEntryCount,
                                    uiState.menuFirstVisibleIndex);
      }
    }
    else if (encoderEvent == ENCODER_EVENT_RIGHT)
    {
      if (uiState.sampleSetListOpen)
      {
        if (uiState.sampleSetCount > 0)
        {
          uiState.sampleSetListSelection++;
          if (uiState.sampleSetListSelection >= uiState.sampleSetCount)
          {
            uiState.sampleSetListSelection = 0;
          }
          updateListFirstVisibleIndex(uiState.sampleSetListSelection, uiState.sampleSetCount,
                                      uiState.sampleSetListFirstVisibleIndex);
        }
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
          updateListFirstVisibleIndex(uiState.patternListSelection, uiState.patternCount,
                                      uiState.patternListFirstVisibleIndex);
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
        updateListFirstVisibleIndex(uiState.menuSelection, settingsEntryCount,
                                    uiState.menuFirstVisibleIndex);
      }
    }
    else if (encoderEvent == ENCODER_EVENT_SHORT_PRESS ||
             encoderEvent == ENCODER_EVENT_MEDIUM_PRESS)
    {
      if (uiState.sampleSetListOpen)
      {
        loadSelectedSampleSetFromMenu();
      }
      else if (uiState.patternListOpen)
      {
        if (uiState.patternCount > 0)
        {
          if (uiState.patternListSourceFilter == patternListModeCardGroups)
          {
            if (loadSelectedCardPatternGroup())
            {
              uiState.patternListOpen = false;
              uiState.patternListSourceFilter = -1;
              uiState.patternListFirstVisibleIndex = 0;
            }
          }
          if (uiState.patternListSourceFilter == patternListModeMemoryPatterns)
          {
            if (deleteSelectedPatternFromMemory())
            {
              uiState.patternListOpen = false;
              uiState.patternDeleteMode = false;
              uiState.patternListSourceFilter = -1;
              uiState.patternListFirstVisibleIndex = 0;
            }
          }
          else if (!uiState.patternDeleteMode)
          {
            if (loadSelectedPattern())
            {
              uiState.menuOpen = false;
              uiState.patternListOpen = false;
              uiState.patternListFirstVisibleIndex = 0;
            }
          }
          else if (encoderEvent == ENCODER_EVENT_SHORT_PRESS ||
                   encoderEvent == ENCODER_EVENT_MEDIUM_PRESS)
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
        flushPendingChainSettings();
      }
    }
    else if (encoderEvent == ENCODER_EVENT_MEDIUM_PRESS)
    {
      uiState.parameterPageIndex = popupSelectionToParameterPage(uiState.editPopupSelection);
      uiState.editPopupValueEdit = false;
      uiState.editPopupChainFocus = chainPopupFocusEnable;
      flushPendingChainSettings();
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
        //-weg- sequencerMoveTrack(-1);
        moveGrooveboxCursorAcrossPatterns(-1);
      }
      else
      {
        sequencerAdjustChainLength(-1);
        uiState.chainSettingsDirty = true;
      }
    }
    else
    {
      moveGrooveboxCursorAcrossPatterns(-1);
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
        //-weg- sequencerMoveTrack(1);
        moveGrooveboxCursorAcrossPatterns(1);
      }
      else
      {
        sequencerAdjustChainLength(1);
        uiState.chainSettingsDirty = true;
      }
    }
    else
    {
      moveGrooveboxCursorAcrossPatterns(1);
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
      else if (uiState.parameterPageIndex == parameterPagePitch ||
               uiState.parameterPageIndex == parameterPageDecay)
      {
        sequencerToggleCurrentStepLock();
      }
      else if (uiState.parameterPageIndex == parameterPageChain)
      {
        sequencerToggleChainEnabled();
        uiState.chainSettingsDirty = true;
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
  if (uiState.patternGroupNameInputOpen)
  {
    if (buttonEvent == BUTTON_EVENT_SHORT_PRESS)
    {
      backspacePatternGroupNameInput();
    }
    else if (buttonEvent == BUTTON_EVENT_MEDIUM_PRESS || buttonEvent == BUTTON_EVENT_LONG_PRESS)
    {
      uiState.patternGroupNameInputOpen = false;
      uiState.dirty = true;
    }

    return;
  }

  if (uiState.menuOpen)
  {
    if (buttonEvent == BUTTON_EVENT_SHORT_PRESS)
    {
      if (uiState.cardStorageMenuOpen)
      {
        // KEY0 short-press in Card Storage = Exit
        uiState.cardStorageMenuOpen = false;
        uiState.cardStorageMenuSelection = 0;
        uiState.cardStorageMenuFirstVisibleIndex = 0;
      }
      else if (uiState.sampleSetListOpen)
      {
        uiState.sampleSetListOpen = false;
        uiState.sampleSetListFirstVisibleIndex = 0;
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
        //-- Stay on waiting screen until credentials are entered or portal closes.
      }
      else
      {
        flushPendingChainSettings();

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
    if (buttonEvent == BUTTON_EVENT_SHORT_PRESS || buttonEvent == BUTTON_EVENT_MEDIUM_PRESS ||
        buttonEvent == BUTTON_EVENT_LONG_PRESS)
    {
      uiState.tempoEditOpen = false;
      uiState.tempoEditSelection = 0;
      uiState.dirty = true;
    }

    return;
  }

  if (uiState.editPopupOpen)
  {
    if (buttonEvent == BUTTON_EVENT_SHORT_PRESS || buttonEvent == BUTTON_EVENT_MEDIUM_PRESS ||
        buttonEvent == BUTTON_EVENT_LONG_PRESS)
    {
      flushPendingChainSettings();

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
      flushPendingChainSettings();

      sequencerToggleEditMode();

      uiState.parameterPageIndex = parameterPageTrig;
      uiState.editPopupOpen = false;
      uiState.editPopupValueEdit = false;
      uiState.editPopupChainFocus = chainPopupFocusEnable;
    }
    else
    {
      handleGrooveboxTransportButton();
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
