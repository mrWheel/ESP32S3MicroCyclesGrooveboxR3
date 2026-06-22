/*** Last Changed: 2026-06-22 - 21:09 ***/
#include "webServerManager.h"
#include "webApi.h"

#include "DisplayDriverClass.h"
#include "progVersion.h"

#include <WebServer.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <esp_log.h>

//-- Logging tag.
static const char* logTag = "WebServerManager";

//-- HTTP server instance (exposed to webApi.cpp via extern declaration in header).
WebServer webServer(80);

//-- Runtime state.
static bool webServerRunning = false;
static bool bootLogEnabled = true;
static String webServerUrl = "";

//
// Serve index.html from LittleFS, or fall back to plain text status.
//
static void handleRootRequest()
{
  if (LittleFS.exists("/index.html"))
  {
    File file = LittleFS.open("/index.html", "r");
    if (file)
    {
      webServer.streamFile(file, "text/html");
      file.close();
      return;
    }
  }

  String response = "ESP32-S3 MicroCycles Groovebox\n";
  response += "Version: ";
  response += PROG_VERSION;
  response += "\n";
  response += "Status: web server ready\n";

  webServer.send(500, "text/plain", response);

} //   handleRootRequest()

//
// Serve app.js from LittleFS.
//
static void handleAppJsRequest()
{
  if (LittleFS.exists("/app.js"))
  {
    File file = LittleFS.open("/app.js", "r");
    if (file)
    {
      webServer.streamFile(file, "application/javascript");
      file.close();
      return;
    }
  }
  webServer.send(404, "text/plain", "not found");
} //   handleAppJsRequest()

//
// Serve style.css from LittleFS.
//
static void handleStyleCssRequest()
{
  if (LittleFS.exists("/style.css"))
  {
    File file = LittleFS.open("/style.css", "r");
    if (file)
    {
      webServer.streamFile(file, "text/css");
      file.close();
      return;
    }
  }
  webServer.send(404, "text/plain", "not found");
} //   handleStyleCssRequest()

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
  webServer.on("/app.js", HTTP_GET, handleAppJsRequest);
  webServer.on("/style.css", HTTP_GET, handleStyleCssRequest);
  webServer.on("/api/status", HTTP_GET, handleStatusRequest);

  // Register all REST API routes
  webApiRegisterRoutes(webServer);

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

  if (!LittleFS.begin(true, "/littlefs", 10, "littlefs"))
  {
    ESP_LOGE(logTag, "Error: LittleFS mount failed; SPA files unavailable");
  }
  else
  {
    ESP_LOGI(logTag, "LittleFS mounted for SPA files");
  }

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
