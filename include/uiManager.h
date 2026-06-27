/*** Last Changed: 2026-06-27 - 14:50 ***/
#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include "InputClass.h"

//-- Initialize splash and UI state.
void uiManagerInit();

//-- Periodic UI redraw/update.
void uiManagerUpdate();

//-- Route encoder events into UI state machine.
void uiManagerHandleEncoderEvent(EncoderEvent encoderEvent);

//-- Route KEY0 events into UI state machine.
void uiManagerHandleAuxButtonEvent(ButtonEvent buttonEvent);

//-- Query whether pattern group has unsaved changes.
bool uiManagerIsPatternGroupDirty();

//-- Set the pattern group dirty flag explicitly.
void uiManagerSetPatternGroupDirty(bool dirty);

//-- Get count of currently loaded patterns in active group.
uint8_t uiManagerGetLoadedPatternCount();

//-- Get pattern name string for a given slot index.
String uiManagerGetPatternNameForSlot(uint8_t slotIndex);

//-- Get pattern chain target for a given slot index.
String uiManagerGetPatternChainTargetForSlot(uint8_t slotIndex);

//-- Query whether chain is enabled for a given slot index.
bool uiManagerGetPatternChainEnabledForSlot(uint8_t slotIndex);

//-- Set chain settings for one loaded pattern slot.
void uiManagerSetPatternChainForSlot(uint8_t slotIndex, bool chainEnabled,
                                     const String& chainTarget);

//-- Load a pattern group from SD card into memory.
bool uiManagerLoadPatternGroup(const String& groupName);

//-- Save the active pattern group to SD card.
bool uiManagerSavePatternGroup();

//-- Return the physical display to the Groovebox screen after web actions.
void uiManagerReturnToGrooveboxScreen();

//-- Request a full redraw of the physical UI.
void uiManagerRequestRedraw();

#endif