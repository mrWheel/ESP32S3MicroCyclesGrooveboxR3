/*** Last Changed: 2026-06-02 - 11:54 ***/
#include "uiGrooveboxScreen.h"

#include "progVersion.h"
#include "settingsStore.h"

#include <ctype.h>

//-- Display row width.
static const size_t listRowContentChars = 24;

//-- Contextual step parameter pages.
static const uint8_t parameterPageTrig = 0;
static const uint8_t parameterPageVelocity = 1;
static const uint8_t parameterPagePitch = 2;
static const uint8_t parameterPageDecay = 3;
static const uint8_t parameterPageProbability = 4;
static const uint8_t parameterPageMute = 5;
static const uint8_t parameterPageChain = 6;

//-- Edit popup entries mapped 1:1 to parameter pages.
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

//-- Clip list item text to fit display row width.
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

//-- Resolve pNN pattern name to zero-based RAM slot index.
static bool patternSlotIndexFromName(const String& patternName, uint8_t& outSlotIndex)
{
  int slotNumber = patternSlotNumberFromName(patternName);

  if (slotNumber < 1)
  {
    return false;
  }

  if (slotNumber > sequencerPatternCount)
  {
    return false;
  }

  outSlotIndex = static_cast<uint8_t>(slotNumber - 1);

  return true;

} //   patternSlotIndexFromName()

//-- Return pattern name for a loaded pattern slot.
static String getPatternNameForSlot(uint8_t slotIndex, const String chainSlotPatternNames[])
{
  char fallbackName[8];

  if (slotIndex < sequencerPatternCount && !chainSlotPatternNames[slotIndex].isEmpty())
  {
    return chainSlotPatternNames[slotIndex];
  }

  snprintf(fallbackName, sizeof(fallbackName), "p%02u", static_cast<unsigned>(slotIndex + 1U));

  return String(fallbackName);

} //   getPatternNameForSlot()

//-- Build display label for chain target field.
static String formatChainTargetLabel(bool chainTargetValid, const String& chainTargetPatternName)
{
  if (!chainTargetValid)
  {
    return "--";
  }

  return chainTargetPatternName;

} //   formatChainTargetLabel()

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
String uiGrooveboxScreenBuildTrackRowText(const char* trackName, const Track& track)
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

} //   uiGrooveboxScreenBuildTrackRowText()

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
static void buildEditPopupRows(const SequencerView& view, String rows[editPopupEntryCount],
                               int editPopupSelection, bool editPopupValueEdit,
                               uint8_t editPopupChainFocus, bool chainTargetValid,
                               const String& chainTargetPatternName)
{
  for (int rowIndex = 0; rowIndex < editPopupEntryCount; rowIndex++)
  {
    uint8_t pageIndex = editPopupPageMap[rowIndex];
    String label = String(editPopupEntries[rowIndex]);
    String valueText = buildEditPopupValueText(pageIndex, view);

    if (pageIndex == parameterPageChain)
    {
      char chainLengthText[8];
      String chainTargetLabel = formatChainTargetLabel(chainTargetValid, chainTargetPatternName);

      snprintf(chainLengthText, sizeof(chainLengthText), "L%u",
               static_cast<unsigned>(view.chainLength));

      if (rowIndex == editPopupSelection)
      {
        if (editPopupValueEdit)
        {
          if (editPopupChainFocus == chainPopupFocusLength)
          {
            rows[rowIndex] = " CHAIN " + String(view.chainEnabled ? "ON" : "OFF") + " >" +
                             String(chainLengthText) + "< " + chainTargetLabel;
          }
          else if (editPopupChainFocus == chainPopupFocusPattern)
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

    if (rowIndex == editPopupSelection)
    {
      if (editPopupValueEdit)
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

//-- Build compact Groovebox footer line with playback and chain context.
static String formatGrooveboxFooter(const SequencerView& view,
                                    const String chainSlotTargetPatternNames[],
                                    const String chainSlotPatternNames[])
{
  char footerLine[64];
  String playingPatternName =
      getPatternNameForSlot(view.playingPatternIndex, chainSlotPatternNames);
  String nextPatternName = "";
  String groupName = settingsStoreGetActivePatternGroup();

  if (view.editMode)
  {
    snprintf(footerLine, sizeof(footerLine), "TR%u S:%02u",
             static_cast<unsigned>(view.selectedTrack + 1U),
             static_cast<unsigned>(view.cursorStep + 1U));

    return String(footerLine);
  }

  if (view.chainEnabled)
  {
    uint8_t nextSlotIndex = 0;

    if (view.playingPatternIndex < sequencerPatternCount &&
        patternSlotIndexFromName(chainSlotTargetPatternNames[view.playingPatternIndex],
                                 nextSlotIndex))
    {
      nextPatternName = getPatternNameForSlot(nextSlotIndex, chainSlotPatternNames);
    }
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
static String buildParameterOverlayLine(const SequencerView& view, uint8_t parameterPageIndex)
{
  const Track& selectedTrack = view.pattern->tracks[view.selectedTrack];
  const Step& selectedStep = selectedTrack.steps[view.cursorStep];
  char lineBuffer[72];

  if (parameterPageIndex == parameterPageTrig)
  {
    return "";
  }
  else if (parameterPageIndex == parameterPageVelocity)
  {
    snprintf(lineBuffer, sizeof(lineBuffer), "VEL   %03u",
             static_cast<unsigned>(selectedStep.velocity));
  }
  else if (parameterPageIndex == parameterPagePitch)
  {
    snprintf(lineBuffer, sizeof(lineBuffer), "PITCH %+03d  LOCK %s",
             static_cast<int>(selectedStep.lockPitch), selectedStep.lockEnabled ? "ON" : "OFF");
  }
  else if (parameterPageIndex == parameterPageDecay)
  {
    snprintf(lineBuffer, sizeof(lineBuffer), "DECAY %03u%% LOCK %s",
             static_cast<unsigned>(selectedStep.lockDecay),
             selectedStep.lockEnabled ? "ON" : "OFF");
  }
  else if (parameterPageIndex == parameterPageProbability)
  {
    snprintf(lineBuffer, sizeof(lineBuffer), "PROB  %03u%%",
             static_cast<unsigned>(selectedStep.probability));
  }
  else if (parameterPageIndex == parameterPageMute)
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

//-- Build dynamic title for the Edit popup.
static String buildEditPopupTitle(const SequencerView& view, const char* const trackNames[])
{
  const char* trackName = "Track";
  char titleBuffer[32];

  if (view.selectedTrack < sequencerTrackCount && trackNames[view.selectedTrack] != nullptr)
  {
    trackName = trackNames[view.selectedTrack];
  }

  snprintf(titleBuffer, sizeof(titleBuffer), "Edit %s S:%02u", trackName,
           static_cast<unsigned>(view.cursorStep + 1U));

  return String(titleBuffer);

} //   buildEditPopupTitle()

//-- Draw the complete Groovebox sequencer screen.
void uiGrooveboxScreenDraw(DisplayDriver& display, const SequencerView& view,
                           const char* const trackNames[], uint8_t parameterPageIndex,
                           bool tempoEditOpen, int tempoEditSelection, bool editPopupOpen,
                           int editPopupSelection, bool editPopupValueEdit,
                           uint8_t editPopupChainFocus, bool chainTargetValid,
                           const String& chainTargetPatternName,
                           const String chainSlotTargetPatternNames[],
                           const String chainSlotPatternNames[], String& lastFooterLine,
                           bool& screenDrawn)
{
  String lines[9];
  String popupRows[editPopupEntryCount];
  String parameterLine;
  String viewPatternName;
  String editPopupTitle;
  int selectedLine = 1;
  char headerLine[48];

  viewPatternName = getPatternNameForSlot(view.activePatternIndex, chainSlotPatternNames);

  snprintf(headerLine, sizeof(headerLine), "BPM %03u SW %02u %s %s",
           static_cast<unsigned>(view.bpm), static_cast<unsigned>(view.swingPercent),
           view.playing ? "PLAY" : "STOP", viewPatternName.c_str());

  lines[0] = fitListRowText(headerLine);

  for (uint8_t trackIndex = 0; trackIndex < sequencerTrackCount; trackIndex++)
  {
    lines[trackIndex + 1] = fitListRowText(uiGrooveboxScreenBuildTrackRowText(
        trackNames[trackIndex], view.pattern->tracks[trackIndex]));
  }

  parameterLine = "";

  if (!tempoEditOpen && !editPopupOpen && view.editMode)
  {
    parameterLine = buildParameterOverlayLine(view, parameterPageIndex);
  }

  lines[7] = parameterLine.isEmpty() ? String("") : fitListRowText(parameterLine);

  lines[8] = fitListRowText(
      formatGrooveboxFooter(view, chainSlotTargetPatternNames, chainSlotPatternNames));

  selectedLine = static_cast<int>(view.selectedTrack) + 1;

  display.drawListScreen("Groovebox", lines, 9, selectedLine, 0, PROG_VERSION);

  if (tempoEditOpen)
  {
    display.drawTempoOverlay(view.bpm, view.swingPercent, tempoEditSelection == 0);
  }
  else if (editPopupOpen)
  {
    buildEditPopupRows(view, popupRows, editPopupSelection, editPopupValueEdit, editPopupChainFocus,
                       chainTargetValid, chainTargetPatternName);

    editPopupTitle = buildEditPopupTitle(view, trackNames);

    display.drawSelectionOverlay(editPopupTitle.c_str(), popupRows, editPopupEntryCount,
                                 editPopupSelection);
  }

  if (view.editMode && !tempoEditOpen && !editPopupOpen)
  {
    int stepCharIndex = 7 + static_cast<int>(view.cursorStep);

    if (stepCharIndex >= 0 && stepCharIndex < static_cast<int>(lines[selectedLine].length()))
    {
      display.drawListCharacterHighlight(selectedLine, stepCharIndex,
                                         lines[selectedLine][stepCharIndex]);
    }
  }

  lastFooterLine = lines[8];
  screenDrawn = true;

} //   uiGrooveboxScreenDraw()

//-- Redraw only the Edit Track popup overlay.
void uiGrooveboxScreenDrawEditPopupOverlayOnly(DisplayDriver& display, const SequencerView& view,
                                               const char* const trackNames[],
                                               int editPopupSelection, bool editPopupValueEdit,
                                               uint8_t editPopupChainFocus, bool chainTargetValid,
                                               const String& chainTargetPatternName)
{
  String popupRows[editPopupEntryCount];
  String editPopupTitle;

  buildEditPopupRows(view, popupRows, editPopupSelection, editPopupValueEdit, editPopupChainFocus,
                     chainTargetValid, chainTargetPatternName);

  editPopupTitle = buildEditPopupTitle(view, trackNames);

  display.drawSelectionOverlay(editPopupTitle.c_str(), popupRows, editPopupEntryCount,
                               editPopupSelection);

} //   uiGrooveboxScreenDrawEditPopupOverlayOnly()

//-- Update only the Groovebox footer row when its text changed.
void uiGrooveboxScreenDrawFooterUpdate(DisplayDriver& display, const SequencerView& view,
                                       const String chainSlotTargetPatternNames[],
                                       const String chainSlotPatternNames[], String& lastFooterLine)
{
  String footerLine = fitListRowText(
      formatGrooveboxFooter(view, chainSlotTargetPatternNames, chainSlotPatternNames));

  if (footerLine == lastFooterLine)
  {
    return;
  }

  display.drawListLine(8, footerLine, false);

  lastFooterLine = footerLine;

} //   uiGrooveboxScreenDrawFooterUpdate()
