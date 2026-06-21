/*** Last Changed: 2026-06-21 - 12:46 ***/
#include "systemManager.h"

#include "WiFiManagerExtClass.h"
#include "appConfig.h"

#include <WiFi.h>
#include <esp_log.h>
#include <esp_system.h>

//-- Logging tag.
static const char* logTag = "SystemManager";

//-- Fixed command queue storage.
static StaticQueue_t commandQueueStruct;
static uint8_t commandQueueStorage[8 * sizeof(SystemCommand)];
static QueueHandle_t commandQueue = nullptr;

//-- Cached connection info loaded from storage or updated at runtime.
static String cachedConnectedSsid = "";
static String cachedConnectedIp = "";

//-- Try to connect using WiFi credentials stored by the ESP32 WiFi stack in NVS.
static bool connectUsingStoredNvsCredentials()
{
  WiFi.mode(WIFI_STA);
  WiFi.persistent(true);
  WiFi.setAutoReconnect(true);

  ESP_LOGI(logTag, "Connecting using WiFi credentials stored in NVS");

  WiFi.begin();

  unsigned long connectStartMs = millis();

  while (WiFi.status() != WL_CONNECTED && (millis() - connectStartMs) < 8000UL)
  {
    delay(100);
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    ESP_LOGW(logTag, "Warning: No usable WiFi credentials in NVS or connect failed");
    return false;
  }

  cachedConnectedSsid = WiFi.SSID();
  cachedConnectedIp = WiFi.localIP().toString();

  ESP_LOGI(logTag, "Connected WiFi: SSID='%s' IP='%s'", cachedConnectedSsid.c_str(),
           cachedConnectedIp.c_str());

  return true;

} //   connectUsingStoredNvsCredentials()

//-- Initialize WiFi manager wrapper.
void systemManagerInit()
{
  WiFiManagerExt::WifiSettings wifiSettings;

  cachedConnectedSsid = "";
  cachedConnectedIp = "";

  wifiSettings.staSsid = "";
  wifiSettings.staPassword = "";
  wifiSettings.apSsid = DEFAULT_AP_SSID;
  wifiSettings.apPassword = DEFAULT_AP_PASSWORD;
  wifiSettings.hostName = DEFAULT_WIFI_HOSTNAME;

  commandQueue =
      xQueueCreateStatic(8, sizeof(SystemCommand), commandQueueStorage, &commandQueueStruct);

  WiFi.persistent(true);
  WiFi.setAutoReconnect(true);

  if (connectUsingStoredNvsCredentials())
  {
    ESP_LOGI(logTag, "System manager initialized with WiFi connection");
    return;
  }

  wifiManagerExt.setSettings(wifiSettings);
  wifiManagerExt.setDisabled(true);
  wifiManagerExt.begin(true);

  ESP_LOGI(logTag, "Stored WiFi info: SSID='-' IP='-'");
  ESP_LOGW(logTag, "Warning: WiFi not connected at boot. System continues without WiFi.");
  ESP_LOGI(logTag, "Use [System Settings] -> Start WiFi Manager to open the portal.");
  ESP_LOGI(logTag, "System manager initialized");

} //   systemManagerInit()

//-- Periodic system service update.
void systemManagerUpdate()
{
  SystemCommand pendingCommand = SystemCommand::none;
  WiFiManagerExt::WifiSettings newSettings;

  wifiManagerExt.update();

  if (wifiManagerExt.consumeNewStaCredentials(newSettings))
  {
    WiFi.persistent(true);
    WiFi.setAutoReconnect(true);

    cachedConnectedSsid = WiFi.SSID();
    cachedConnectedIp = WiFi.localIP().toString();

    if (cachedConnectedSsid.isEmpty())
    {
      cachedConnectedSsid = newSettings.staSsid;
    }

    ESP_LOGI(logTag, "Connected WiFi: SSID='%s' IP='%s'",
             cachedConnectedSsid.isEmpty() ? "-" : cachedConnectedSsid.c_str(),
             cachedConnectedIp.isEmpty() ? "-" : cachedConnectedIp.c_str());

    ESP_LOGI(logTag, "WiFi credentials stored in ESP32 NVS. Restarting...");

    delay(150);
    esp_restart();
  }

  if (commandQueue == nullptr)
  {
    return;
  }

  while (xQueueReceive(commandQueue, &pendingCommand, 0) == pdTRUE)
  {
    if (pendingCommand == SystemCommand::startWifiManager)
    {
      WiFi.persistent(true);
      WiFi.setAutoReconnect(true);

      wifiManagerExt.setDisabled(false);
      wifiManagerExt.startPortal();

      ESP_LOGI(logTag, "WiFi manager portal requested from settings menu");
    }
    else if (pendingCommand == SystemCommand::eraseWifiCredentials)
    {
      ESP_LOGW(logTag, "Erasing WiFi credentials from ESP32 NVS and rebooting");
      WiFi.disconnect(true, true);
      delay(50);
      esp_restart();
    }
    else if (pendingCommand == SystemCommand::restartNow)
    {
      ESP_LOGW(logTag, "Immediate restart requested");
      delay(20);
      esp_restart();
    }
  }

} //   systemManagerUpdate()

//-- Queue asynchronous command for SystemTask.
void systemManagerQueueCommand(SystemCommand command)
{
  if (commandQueue == nullptr)
  {
    return;
  }

  (void)xQueueSend(commandQueue, &command, 0);

} //   systemManagerQueueCommand()

//-- Return current connected SSID or AP SSID.
String systemManagerGetSsid()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return WiFi.SSID();
  }

  if (!cachedConnectedSsid.isEmpty())
  {
    return cachedConnectedSsid;
  }

  return wifiManagerExt.getPortalApSsid();

} //   systemManagerGetSsid()

//-- Return active station or AP address.
String systemManagerGetIpAddress()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return WiFi.localIP().toString();
  }

  if (!cachedConnectedIp.isEmpty())
  {
    return cachedConnectedIp;
  }

  return wifiManagerExt.getAddressString();

} //   systemManagerGetIpAddress()

//-- Return local interface MAC address.
String systemManagerGetMacAddress()
{
  return WiFi.macAddress();

} //   systemManagerGetMacAddress()

//-- Return portal AP SSID used by WiFiManager.
String systemManagerGetPortalApSsid()
{
  return wifiManagerExt.getPortalApSsid();

} //   systemManagerGetPortalApSsid()

//-- Return whether the portal is currently active.
bool systemManagerIsWifiPortalActive()
{
  return wifiManagerExt.shouldOpenPortal();

} //   systemManagerIsWifiPortalActive()