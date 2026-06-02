/*** Last Changed: 2026-06-02 - 10:24 ***/
#pragma once

#include "DisplayDriverClass.h"
#include "sequencer.h"

#include <Arduino.h>

//-- Build one Groovebox track row: left-aligned instrument, right-aligned 16 steps.
String uiGrooveboxScreenBuildTrackRowText(const char* trackName, const Track& track);

//-- Draw the complete Groovebox sequencer screen.
void uiGrooveboxScreenDraw(DisplayDriver& display, const SequencerView& view,
                           const char* const trackNames[], uint8_t parameterPageIndex,
                           bool tempoEditOpen, int tempoEditSelection, bool editPopupOpen,
                           int editPopupSelection, bool editPopupValueEdit,
                           uint8_t editPopupChainFocus, bool chainTargetValid,
                           const String& chainTargetPatternName,
                           const String chainSlotTargetPatternNames[],
                           const String chainSlotPatternNames[], String& lastFooterLine,
                           bool& screenDrawn);

//-- Redraw only the Edit Track popup overlay.
void uiGrooveboxScreenDrawEditPopupOverlayOnly(DisplayDriver& display, const SequencerView& view,
                                               int editPopupSelection, bool editPopupValueEdit,
                                               uint8_t editPopupChainFocus, bool chainTargetValid,
                                               const String& chainTargetPatternName);

//-- Update only the Groovebox footer row when its text changed.
void uiGrooveboxScreenDrawFooterUpdate(DisplayDriver& display, const SequencerView& view,
                                       const String chainSlotTargetPatternNames[],
                                       const String chainSlotPatternNames[],
                                       String& lastFooterLine);
