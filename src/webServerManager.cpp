/*** Last Changed: 2026-06-21 - 12:46 ***/
#include "webServerManager.h"

#include "DisplayDriverClass.h"
#include "progVersion.h"

#include <WebServer.h>
#include <WiFi.h>
#include <esp_log.h>

//-- Logging tag.
static const char* logTag = "WebServerManager";

//-- HTTP server instance.
static WebServer webServer(80);

//-- Runtime state.
static bool webServerRunning = false;
static bool bootLogEnabled = true;
static String webServerUrl = "";

//-- Send a minimal text response for the root endpoint.
static void handleRootRequest()
{
  String response = "ESP32-S3 MicroCycles Groovebox\n";

  response += "Version: ";
  response += PROG_VERSION;
  response += "\n";
  response += "Status: web server ready\n";

  webServer.send(200, "text/plain", response);

} //   handleRootRequest()

//-- Send a minimal JSON status response for future SPA use.
static void handleStatusRequest()
{
  String response = "{";

  response += "\"device\":\"ESP32-S3 MicroCycles Groovebox\",";
  response += "\"version\":\"";
  response += PROG_VERSION;
  response += "\",";
  response += "\"ip\":\"";
  response += WiFi.localIP().toString();
  response += "\",";
  response += "\"rssi\":";
  response += String(WiFi.RSSI());
  response += "}";

  webServer.send(200, "application/json", response);

} //   handleStatusRequest()

//-- Send a simple 404 response.
static void handleNotFoundRequest()
{
  webServer.send(404, "text/plain", "Not found");

} //   handleNotFoundRequest()

//-- Start HTTP server when WiFi is connected.
static void startWebServer()
{
  IPAddress ipAddress = WiFi.localIP();

  if (webServerRunning)
  {
    return;
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    return;
  }

  if (ipAddress.toString() == "0.0.0.0")
  {
    return;
  }

  webServer.on("/", HTTP_GET, handleRootRequest);
  webServer.on("/api/status", HTTP_GET, handleStatusRequest);
  webServer.onNotFound(handleNotFoundRequest);

  webServer.begin();

  webServerUrl = String("http://") + ipAddress.toString();
  webServerRunning = true;

  ESP_LOGI(logTag, "Webserver started at %s", webServerUrl.c_str());

  if (bootLogEnabled)
  {
    displayBootLogInfo(String("Web ") + ipAddress.toString());
  }

} //   startWebServer()

//-- Stop HTTP server.
static void stopWebServer()
{
  if (!webServerRunning)
  {
    return;
  }

  webServer.stop();

  ESP_LOGW(logTag, "Webserver stopped");

  webServerRunning = false;
  webServerUrl = "";

} //   stopWebServer()

//-- Initialize web server manager state.
void webServerManagerInit()
{
  webServerRunning = false;
  webServerUrl = "";

  ESP_LOGI(logTag, "Webserver manager initialized");

} //   webServerManagerInit()

//-- Enable or disable boot display logging from web server manager.
void webServerManagerSetBootLogEnabled(bool enabled)
{
  bootLogEnabled = enabled;

} //   webServerManagerSetBootLogEnabled()

//-- Periodic web server service update.
void webServerManagerUpdate(bool wifiPortalActive)
{
  if (wifiPortalActive)
  {
    stopWebServer();
    return;
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    stopWebServer();
    return;
  }

  if (!webServerRunning)
  {
    startWebServer();
  }

  if (webServerRunning)
  {
    webServer.handleClient();
  }

} //   webServerManagerUpdate()

//-- Return true when the web server is currently running.
bool webServerManagerIsRunning()
{
  return webServerRunning;

} //   webServerManagerIsRunning()

//-- Return active web server URL or an empty string.
String webServerManagerGetUrl()
{
  return webServerUrl;

} //   webServerManagerGetUrl()
