/*** Last Changed: 2026-05-23 - 17:33 ***/
#include "systemManager.h"

#include "WiFiManagerExt.h"
#include "appConfig.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_log.h>
#include <esp_system.h>

//-- Logging tag.
static const char* logTag = "SystemManager";

//-- Fixed command queue storage.
static StaticQueue_t commandQueueStruct;
static uint8_t commandQueueStorage[8 * sizeof(SystemCommand)];
static QueueHandle_t commandQueue = nullptr;

//-- Persisted WiFi credentials path.
static const char* wifiSettingsPath = "/wifiSettings.json";

//-- Cached connection info loaded from storage or updated at runtime.
static String cachedConnectedSsid = "";
static String cachedConnectedIp = "";

//-- Ensure LittleFS is mounted for WiFi settings persistence.
static bool ensureWifiSettingsFsMounted()
{
  static bool mounted = false;

  if (mounted)
  {
    return true;
  }

  if (!LittleFS.begin(true, "/littlefs", 10, "littlefs"))
  {
    ESP_LOGW(logTag, "LittleFS mount failed for WiFi settings");
    return false;
  }

  mounted = true;

  return true;

} //   ensureWifiSettingsFsMounted()

//-- Load saved STA credentials from LittleFS.
static void loadStoredWifiCredentials(String& staSsid, String& staPassword, String& lastConnectedSsid, String& lastConnectedIp)
{
  staSsid = "";
  staPassword = "";
  lastConnectedSsid = "";
  lastConnectedIp = "";

  if (!ensureWifiSettingsFsMounted())
  {
    return;
  }

  if (!LittleFS.exists(wifiSettingsPath))
  {
    return;
  }

  File file = LittleFS.open(wifiSettingsPath, "r");

  if (!file)
  {
    ESP_LOGW(logTag, "Failed to open %s", wifiSettingsPath);
    return;
  }

  JsonDocument jsonDocument;
  DeserializationError error = deserializeJson(jsonDocument, file);

  file.close();

  if (error)
  {
    ESP_LOGW(logTag, "Failed to parse %s (%s)", wifiSettingsPath, error.c_str());
    return;
  }

  staSsid = static_cast<const char*>(jsonDocument["staSsid"] | "");
  staPassword = static_cast<const char*>(jsonDocument["staPassword"] | "");
  lastConnectedSsid = static_cast<const char*>(jsonDocument["lastConnectedSsid"] | "");
  lastConnectedIp = static_cast<const char*>(jsonDocument["lastConnectedIp"] | "");

} //   loadStoredWifiCredentials()

//-- Save STA credentials to LittleFS.
static bool saveWifiCredentials(const String& staSsid, const String& staPassword, const String& lastConnectedSsid, const String& lastConnectedIp)
{
  if (!ensureWifiSettingsFsMounted())
  {
    return false;
  }

  File file = LittleFS.open(wifiSettingsPath, "w");

  if (!file)
  {
    ESP_LOGW(logTag, "Failed to open %s for write", wifiSettingsPath);
    return false;
  }

  JsonDocument jsonDocument;

  jsonDocument["staSsid"] = staSsid;
  jsonDocument["staPassword"] = staPassword;
  jsonDocument["lastConnectedSsid"] = lastConnectedSsid;
  jsonDocument["lastConnectedIp"] = lastConnectedIp;

  bool success = (serializeJson(jsonDocument, file) > 0);

  file.close();

  if (!success)
  {
    ESP_LOGW(logTag, "Failed to serialize %s", wifiSettingsPath);
  }

  return success;

} //   saveWifiCredentials()

//-- Initialize WiFi manager wrapper.
void systemManagerInit()
{
  WiFiManagerExt::WifiSettings wifiSettings;
  String storedStaSsid;
  String storedStaPassword;
  String storedConnectedSsid;
  String storedConnectedIp;

  loadStoredWifiCredentials(storedStaSsid, storedStaPassword, storedConnectedSsid, storedConnectedIp);

  cachedConnectedSsid = storedConnectedSsid;
  cachedConnectedIp = storedConnectedIp;

  wifiSettings.staSsid = storedStaSsid;
  wifiSettings.staPassword = storedStaPassword;
  wifiSettings.apSsid = DEFAULT_AP_SSID;
  wifiSettings.apPassword = DEFAULT_AP_PASSWORD;
  wifiSettings.hostName = DEFAULT_WIFI_HOSTNAME;

  commandQueue = xQueueCreateStatic(8, sizeof(SystemCommand), commandQueueStorage, &commandQueueStruct);

  wifiManagerExt.setSettings(wifiSettings);
  wifiManagerExt.begin(true);

  ESP_LOGI(logTag,
           "Stored WiFi info: SSID='%s' IP='%s'",
           cachedConnectedSsid.isEmpty() ? "-" : cachedConnectedSsid.c_str(),
           cachedConnectedIp.isEmpty() ? "-" : cachedConnectedIp.c_str());

  ESP_LOGW(logTag, "WiFi auto-connect is disabled at boot. System continues without WiFi.");
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
    String connectedSsid = WiFi.SSID();
    String connectedIp = WiFi.localIP().toString();

    if (connectedSsid.isEmpty())
    {
      connectedSsid = newSettings.staSsid;
    }

    if (connectedIp == "0.0.0.0")
    {
      connectedIp = "";
    }

    cachedConnectedSsid = connectedSsid;
    cachedConnectedIp = connectedIp;

    ESP_LOGI(logTag,
             "Connected WiFi: SSID='%s' IP='%s'",
             cachedConnectedSsid.isEmpty() ? "-" : cachedConnectedSsid.c_str(),
             cachedConnectedIp.isEmpty() ? "-" : cachedConnectedIp.c_str());

    if (saveWifiCredentials(newSettings.staSsid, newSettings.staPassword, cachedConnectedSsid, cachedConnectedIp))
    {
      ESP_LOGI(logTag, "WiFi credentials saved. Restarting...");
    }
    else
    {
      ESP_LOGW(logTag, "WiFi credentials received, but save failed. Restarting anyway...");
    }

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
      wifiManagerExt.setDisabled(false);
      wifiManagerExt.startPortal();
      ESP_LOGI(logTag, "WiFi manager portal requested from settings menu");
    }
    else if (pendingCommand == SystemCommand::eraseWifiCredentials)
    {
      ESP_LOGW(logTag, "Erasing WiFi credentials and rebooting");

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