/*** Last Changed: 2026-05-23 - 16:00 ***/
#include "WiFiManagerExt.h"

#include "appConfig.h"

#include <WiFi.h>
#include <ctype.h>
#include <esp_log.h>
#include <esp_mac.h>

//--- Logging tag
static const char* logTag = "WiFiManagerExt";

//--- Shared project instance
WiFiManagerExt wifiManagerExt;

//--- Constructor
WiFiManagerExt::WiFiManagerExt() : portalActive(false),
                                   newCredentialsPending(false),
                                   lastRetryMs(0),
                                   retryCount(0),
                                   portalStartedPending(false),
                                   lastPortalApSsid(""),
                                   wifiManagerDisabled(false),
                                   portalSuspendCallback(nullptr),
                                   portalResumeCallback(nullptr)
{
  currentSettings.apPassword = "";

} //   WiFiManagerExt()

//--- Replace the current WiFi settings snapshot
void WiFiManagerExt::setSettings(const WifiSettings& wifiSettings)
{
  currentSettings = wifiSettings;
  currentSettings.apPassword = "";
  normalizePortalIdentity();

} //   setSettings()

//--- Initialize WiFi manager service
void WiFiManagerExt::begin(bool disabled)
{
  portalActive = false;
  newCredentialsPending = false;
  portalStartedPending = false;
  lastPortalApSsid = "";
  retryCount = 0;
  lastRetryMs = 0;

  if (disabled)
  {
    setDisabled(true);

    ESP_LOGI(logTag, "WiFi manager init skipped: disabled");

    return;
  }

  wifiManagerDisabled = false;

  if (currentSettings.staSsid.isEmpty())
  {
    startPortal();

    return;
  }

  startStationAttempt();

  ESP_LOGI(logTag, "AP started: %s", currentSettings.apSsid.c_str());

} //   begin()

//--- Set portal suspend/resume callbacks
void WiFiManagerExt::setPortalCallbacks(PortalCallback suspendCallback, PortalCallback resumeCallback)
{
  portalSuspendCallback = suspendCallback;
  portalResumeCallback = resumeCallback;

} //   setPortalCallbacks()

//--- Update WiFi manager service
void WiFiManagerExt::update()
{
  if (wifiManagerDisabled)
  {
    return;
  }

  if (portalActive)
  {
    wm.process();

    if (WiFi.status() == WL_CONNECTED)
    {
      portalActive = false;
      retryCount = 0;
      newCredentialsPending = true;

      if (portalResumeCallback != nullptr)
      {
        portalResumeCallback();
      }

      ESP_LOGI(logTag, "Portal completed, STA connected: %s", WiFi.SSID().c_str());
    }

    return;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    retryCount = 0;

    return;
  }

  if (retryCount >= maxRetriesBeforePortal)
  {
    ESP_LOGW(logTag, "STA connection failed after %u retries. Continuing without WiFi.", static_cast<unsigned>(maxRetriesBeforePortal));
    setDisabled(true);

    return;
  }

  if (millis() - lastRetryMs >= retryIntervalMs)
  {
    startStationAttempt();
  }

} //   update()

//--- Apply WiFi settings and reconnect flow
void WiFiManagerExt::applySettings(const WifiSettings& wifiSettings)
{
  setSettings(wifiSettings);
  portalActive = false;
  newCredentialsPending = false;
  retryCount = 0;

  if (wifiManagerDisabled)
  {
    return;
  }

  WiFi.disconnect(false, true);
  delay(50);

  if (currentSettings.staSsid.isEmpty())
  {
    startPortal();

    return;
  }

  startStationAttempt();

} //   applySettings()

//--- Get current WiFi settings
const WiFiManagerExt::WifiSettings& WiFiManagerExt::getSettings() const
{
  return currentSettings;

} //   getSettings()

//--- Start WiFi manager config portal explicitly
void WiFiManagerExt::startPortal()
{
  if (wifiManagerDisabled)
  {
    return;
  }

  normalizePortalIdentity();

  if (portalActive)
  {
    return;
  }

  //--- Explicit portal start begins a new credential session.
  newCredentialsPending = false;

  //--- Force manual credential entry in portal.
  //--- This prevents immediate auto-connect with previously cached STA data.
  WiFi.disconnect(true, true);
  delay(50);

  WiFi.mode(WIFI_AP_STA);
  WiFi.setHostname(currentSettings.hostName.c_str());

  if (portalSuspendCallback != nullptr)
  {
    portalSuspendCallback();
  }

  //--- Trigger async scan at portal start so AP list is ready on first page visit
  wm._preloadwifiscan = true;

  wm.setConfigPortalBlocking(false);
  wm.startConfigPortal(currentSettings.apSsid.c_str());

  portalActive = true;
  portalStartedPending = true;
  lastPortalApSsid = currentSettings.apSsid;

  ESP_LOGI(logTag, "WiFi config portal started on AP: %s", currentSettings.apSsid.c_str());

} //   startPortal()

//--- Stop WiFi manager config portal explicitly
void WiFiManagerExt::stopPortal()
{
  if (!portalActive)
  {
    return;
  }

  wm.stopConfigPortal();
  portalActive = false;
  portalStartedPending = false;

  if (portalResumeCallback != nullptr)
  {
    portalResumeCallback();
  }

  ESP_LOGI(logTag, "WiFi config portal stopped");

} //   stopPortal()

//--- Enable or disable WiFi manager service runtime behavior
void WiFiManagerExt::setDisabled(bool disabled)
{
  wifiManagerDisabled = disabled;

  if (!disabled)
  {
    ESP_LOGI(logTag, "WiFi manager enabled at runtime");

    return;
  }

  stopPortal();
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);

  ESP_LOGI(logTag, "WiFi manager disabled at runtime");

} //   setDisabled()

//--- Get disabled state
bool WiFiManagerExt::isDisabled() const
{
  return wifiManagerDisabled;

} //   isDisabled()

//--- Get effective AP SSID used by portal
String WiFiManagerExt::getPortalApSsid() const
{
  return currentSettings.apSsid;

} //   getPortalApSsid()

//--- Consume portal started event for UI notification
bool WiFiManagerExt::consumePortalStartedApSsid(String& apSsid)
{
  if (!portalStartedPending)
  {
    return false;
  }

  apSsid = lastPortalApSsid;
  portalStartedPending = false;

  return true;

} //   consumePortalStartedApSsid()

//--- Scan available access points and return unique SSIDs
size_t WiFiManagerExt::scanNetworks(String ssids[], size_t maxCount)
{
  if (maxCount == 0)
  {
    return 0;
  }

  int foundCount = WiFi.scanNetworks(false, true);

  if (foundCount <= 0)
  {
    WiFi.scanDelete();

    return 0;
  }

  size_t outputCount = 0;

  for (int i = 0; i < foundCount && outputCount < maxCount; i++)
  {
    String ssid = WiFi.SSID(i);

    if (ssid.isEmpty())
    {
      continue;
    }

    bool duplicate = false;

    for (size_t j = 0; j < outputCount; j++)
    {
      if (ssids[j] == ssid)
      {
        duplicate = true;
        break;
      }
    }

    if (!duplicate)
    {
      ssids[outputCount] = ssid;
      outputCount++;
    }
  }

  WiFi.scanDelete();

  ESP_LOGI(logTag, "Scanned APs: %u", static_cast<unsigned int>(outputCount));

  return outputCount;

} //   scanNetworks()

//--- Check whether station is connected
bool WiFiManagerExt::isStaConnected() const
{
  return WiFi.status() == WL_CONNECTED;

} //   isStaConnected()

//--- Check whether config portal is active
bool WiFiManagerExt::shouldOpenPortal() const
{
  return portalActive;

} //   shouldOpenPortal()

//--- Get current address string (STA or AP)
String WiFiManagerExt::getAddressString() const
{
  if (isStaConnected())
  {
    return WiFi.localIP().toString();
  }

  return WiFi.softAPIP().toString();

} //   getAddressString()

//--- Consume newly configured STA credentials from portal
bool WiFiManagerExt::consumeNewStaCredentials(WifiSettings& wifiSettings)
{
  if (!newCredentialsPending)
  {
    return false;
  }

  wifiSettings = currentSettings;
  wifiSettings.staSsid = WiFi.SSID();
  wifiSettings.staPassword = WiFi.psk();

  currentSettings.staSsid = wifiSettings.staSsid;
  currentSettings.staPassword = wifiSettings.staPassword;
  newCredentialsPending = false;

  return true;

} //   consumeNewStaCredentials()

//--- Start station connection attempt
void WiFiManagerExt::startStationAttempt()
{
  if (currentSettings.staSsid.isEmpty())
  {
    return;
  }

  WiFi.mode(WIFI_AP_STA);
  WiFi.setHostname(currentSettings.hostName.c_str());
  WiFi.begin(currentSettings.staSsid.c_str(), currentSettings.staPassword.c_str());

  retryCount++;
  lastRetryMs = millis();

  ESP_LOGI(logTag, "Connecting to STA SSID: %s", currentSettings.staSsid.c_str());

} //   startStationAttempt()

//--- Build MAC suffix using last three bytes (xxyyzz)
String WiFiManagerExt::buildMacSuffix() const
{
  uint8_t macAddress[6];
  char suffixBuffer[8];

  if (esp_read_mac(macAddress, ESP_MAC_WIFI_STA) != ESP_OK)
  {
    return "000000";
  }

  snprintf(suffixBuffer, sizeof(suffixBuffer), "%02x%02x%02x", macAddress[3], macAddress[4], macAddress[5]);

  return String(suffixBuffer);

} //   buildMacSuffix()

//--- Remove trailing identity MAC suffix if present
String WiFiManagerExt::stripMacSuffixIfPresent(const String& value) const
{
  //--- Current format: <base>-xxyyzz
  if (value.length() >= 8)
  {
    int suffixStart = static_cast<int>(value.length()) - 7;

    if (value.charAt(suffixStart) == '-')
    {
      String suffix = value.substring(suffixStart + 1);
      bool validSuffix = (suffix.length() == 6);

      for (int index = 0; validSuffix && index < static_cast<int>(suffix.length()); index++)
      {
        if (!isxdigit(suffix.charAt(index)))
        {
          validSuffix = false;
        }
      }

      if (validSuffix)
      {
        return value.substring(0, suffixStart);
      }
    }
  }

  //--- New format: <base>-aabbccddeeff
  if (value.length() >= 14)
  {
    int suffixStart = static_cast<int>(value.length()) - 13;

    if (value.charAt(suffixStart) == '-')
    {
      String suffix = value.substring(suffixStart + 1);
      bool validSuffix = (suffix.length() == 12);

      for (int index = 0; validSuffix && index < static_cast<int>(suffix.length()); index++)
      {
        if (!isxdigit(suffix.charAt(index)))
        {
          validSuffix = false;
        }
      }

      if (validSuffix)
      {
        return value.substring(0, suffixStart);
      }
    }
  }

  //--- Legacy format: <base>-ab-cd-ef
  if (value.length() < 10)
  {
    return value;
  }

  int suffixStart = static_cast<int>(value.length()) - 9;

  if (value.charAt(suffixStart) != '-' ||
      value.charAt(suffixStart + 3) != '-' ||
      value.charAt(suffixStart + 6) != '-')
  {
    return value;
  }

  String suffix = value.substring(suffixStart + 1);

  for (int index = 0; index < static_cast<int>(suffix.length()); index++)
  {
    char character = suffix.charAt(index);

    if (character == '-')
    {
      continue;
    }

    if (!isxdigit(character))
    {
      return value;
    }
  }

  return value.substring(0, suffixStart);

} //   stripMacSuffixIfPresent()

//--- Build identity text: base + "-" + MAC suffix
String WiFiManagerExt::buildIdentityWithMacSuffix(const String& baseValue, const String& fallbackValue) const
{
  String base = stripMacSuffixIfPresent(baseValue);

  if (base.isEmpty())
  {
    base = fallbackValue;
  }

  return base + "-" + buildMacSuffix();

} //   buildIdentityWithMacSuffix()

//--- Normalize current AP SSID and hostname for portal identity
void WiFiManagerExt::normalizePortalIdentity()
{
  currentSettings.apSsid = buildIdentityWithMacSuffix(currentSettings.apSsid, DEFAULT_AP_SSID);
  currentSettings.hostName = buildIdentityWithMacSuffix(currentSettings.hostName, DEFAULT_WIFI_HOSTNAME);

} //   normalizePortalIdentity()
