/*** Last Changed: 2026-06-28 - 11:38 ***/
#ifndef DEBUG_UTILS_H
#define DEBUG_UTILS_H

#include <Arduino.h>

//-- Log remaining stack space for the currently running task.
void logStackHighWaterMark(const char* tag, const char* label);

#endif // DEBUG_UTILS_H
