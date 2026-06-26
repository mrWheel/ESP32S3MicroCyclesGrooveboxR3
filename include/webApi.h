/*** Last Changed: 2026-06-26 - 15:17 ***/
#ifndef WEB_API_H
#define WEB_API_H

#include <WebServer.h>

//
// Register all REST API routes onto the given WebServer instance.
// Call this from webServerManager after the webServer object is created.
//
void webApiRegisterRoutes(WebServer& server);

#endif //   WEB_API_H
