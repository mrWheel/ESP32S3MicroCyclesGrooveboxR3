/*** Last Changed: 2026-06-27 - 16:18 ***/
#pragma once

#include <Arduino.h>

void loadStatusClear();
void loadStatusStart(const char* title, uint8_t total);
void loadStatusUpdate(uint8_t current, const char* item);
void loadStatusFinish();
void loadStatusFail(const char* errorText);

bool loadStatusIsActive();
String loadStatusGetTitle();
String loadStatusGetItem();
String loadStatusGetMessage();
uint8_t loadStatusGetCurrent();
uint8_t loadStatusGetTotal();
bool loadStatusHasFailed();
String loadStatusGetError();
