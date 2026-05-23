/*** Last Changed: 2026-05-23 - 16:00 ***/
#ifndef SYSTEM_MANAGER_H
#define SYSTEM_MANAGER_H

#include <Arduino.h>

//-- Actions queued from UI to system task.
enum class SystemCommand : uint8_t
{
  none = 0,
  startWifiManager,
  eraseWifiCredentials,
  restartNow
};

//-- Initialize WiFi manager wrapper.
void systemManagerInit();

//-- Periodic system service update.
void systemManagerUpdate();

//-- Queue asynchronous command for SystemTask.
void systemManagerQueueCommand(SystemCommand command);

//-- Read-only network information for settings menu.
String systemManagerGetSsid();
String systemManagerGetIpAddress();
String systemManagerGetMacAddress();
String systemManagerGetPortalApSsid();
bool systemManagerIsWifiPortalActive();

#endif