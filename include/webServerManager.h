/*** Last Changed: 2026-06-21 - 12:46 ***/
#ifndef WEB_SERVER_MANAGER_H
#define WEB_SERVER_MANAGER_H

#include <Arduino.h>

//-- Initialize web server manager state.
void webServerManagerInit();

//-- Periodic web server service update.
void webServerManagerUpdate(bool wifiPortalActive);

//-- Enable or disable boot display logging from web server manager.
void webServerManagerSetBootLogEnabled(bool enabled);

//-- Return true when the web server is currently running.
bool webServerManagerIsRunning();

//-- Return active web server URL or an empty string.
String webServerManagerGetUrl();

#endif //   WEB_SERVER_MANAGER_H