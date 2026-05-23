/*** Last Changed: 2026-05-23 - 16:00 ***/
#ifndef WIFI_MANAGER_EXT_H
#define WIFI_MANAGER_EXT_H

#include <Arduino.h>
#include <WiFiManager.h>

//--- WiFi manager service class
class WiFiManagerExt
{
public:
  //--- WiFi-related settings owned by the manager
  struct WifiSettings
  {
    String staSsid;
    String staPassword;
    String apSsid;
    String apPassword;
    String hostName;
  };

  using PortalCallback = void (*)();

  WiFiManagerExt();

  void setSettings(const WifiSettings& wifiSettings);
  const WifiSettings& getSettings() const;

  void begin(bool disabled = false);
  void update();
  void applySettings(const WifiSettings& wifiSettings);

  void startPortal();
  void stopPortal();
  void setDisabled(bool disabled);
  bool isDisabled() const;

  String getPortalApSsid() const;
  bool consumePortalStartedApSsid(String& apSsid);
  size_t scanNetworks(String ssids[], size_t maxCount);

  bool isStaConnected() const;
  bool shouldOpenPortal() const;
  String getAddressString() const;
  bool consumeNewStaCredentials(WifiSettings& wifiSettings);

  void setPortalCallbacks(PortalCallback suspendCallback, PortalCallback resumeCallback);

private:
  void startStationAttempt();
  void normalizePortalIdentity();
  String buildMacSuffix() const;
  String stripMacSuffixIfPresent(const String& value) const;
  String buildIdentityWithMacSuffix(const String& baseValue, const String& fallbackValue) const;

  WiFiManager wm;
  WifiSettings currentSettings;
  bool portalActive;
  bool newCredentialsPending;
  uint32_t lastRetryMs;
  uint8_t retryCount;
  bool portalStartedPending;
  String lastPortalApSsid;
  bool wifiManagerDisabled;
  PortalCallback portalSuspendCallback;
  PortalCallback portalResumeCallback;

  static const uint8_t maxRetriesBeforePortal = 2;
  static const uint32_t retryIntervalMs = 15000UL;
};

//--- Shared project instance
extern WiFiManagerExt wifiManagerExt;

#endif
