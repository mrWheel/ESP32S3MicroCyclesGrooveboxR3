/*** Last Changed: 2026-06-11 - 14:31 ***/
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

#endif