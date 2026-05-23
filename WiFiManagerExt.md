# WiFiManagerExt

[Back to README](README.md)

## Purpose

WiFiManagerExt is the reusable WiFi connection and captive-portal controller for this project.
It owns its own runtime state and uses tzapu/WiFiManager directly.

The class handles:
- STA connection attempts
- fallback captive portal startup
- portal stop/start transitions
- WiFi connection status queries
- portal-start notifications
- new STA credential capture after portal completion
- AP SSID normalization using the current device MAC address
- optional portal suspend/resume hooks for applications that also run a web server

## What Changed (Current Code)

Recent behavior updates in the class implementation:

- AP/host identity suffix format is now `<name>-xxyyzz`.
  - Uses the last 3 MAC bytes (6 hex chars).
  - Older suffix formats are still stripped when normalizing names.
- Explicit `startPortal()` now clears active/cached STA connection state before opening the portal.
  - Prevents immediate portal completion caused by stale remembered station credentials.
- `setDisabled(false)` now only enables runtime behavior.
  - It no longer auto-starts station attempts directly.
  - Portal or STA flow should be started explicitly by caller intent.

## WiFiManager Dependency

This class uses tzapu/WiFiManager and keeps that dependency intact.

It does not wrap another WiFi manager abstraction.
The class implements the control flow itself and calls WiFiManager directly where needed.

## Public API

`WifiSettings` is owned by [include/WiFiManagerExt.h](include/WiFiManagerExt.h) as `WiFiManagerExt::WifiSettings`.
The standalone [include/WifiSettings.h](include/WifiSettings.h) header remains as a compatibility alias.

### Constructors

- `WiFiManagerExt()`
  - Creates an instance with empty/default runtime state.
  - The class does not start WiFi on construction.

### Lifecycle

- `setSettings(const WifiSettings& wifiSettings)`
  - Replaces the current WiFi settings snapshot.
  - Normalizes AP SSID and hostname identity strings.

- `begin(bool disabled = false)`
  - Starts STA connection attempts or opens the portal using the currently stored settings.
  - If `disabled` is true, WiFi is turned off and the class stays inactive.

- `update()`
  - Advances portal processing or reconnect retry handling.
  - Must be called regularly from the main loop or a fast task.

- `applySettings(const WifiSettings& wifiSettings)`
  - Replaces the active WiFi settings and restarts the connection flow.
  - Clears pending credential state.

### Portal Control

- `startPortal()`
  - Opens the non-blocking configuration portal.

- `stopPortal()`
  - Stops the configuration portal and restores the normal application flow.

- `setDisabled(bool disabled)`
  - Enables or disables the WiFi manager runtime behavior.
  - Disabling stops the portal, disconnects STA, and turns WiFi off.

- `isDisabled() const`
  - Returns whether runtime behavior is disabled.

### Settings and Status

- `getSettings() const`
  - Returns the current cached `WifiSettings`.

- `getPortalApSsid() const`
  - Returns the AP SSID used by the portal.

- `getAddressString() const`
  - Returns the current STA address when connected, otherwise the AP address.

- `isStaConnected() const`
  - Returns whether the station is currently connected.

- `shouldOpenPortal() const`
  - Returns whether the portal is currently active.

### Events

- `consumePortalStartedApSsid(String& apSsid)`
  - Returns the AP SSID for a newly started portal once, then clears the pending flag.

- `consumeNewStaCredentials(WifiSettings& wifiSettings)`
  - Returns the newly entered STA credentials after portal completion.
  - Also clears the pending credential flag.

### Scanning

- `scanNetworks(String ssids[], size_t maxCount)`
  - Scans nearby APs and returns unique SSIDs.

### Portal Hooks

- `setPortalCallbacks(PortalCallback suspendCallback, PortalCallback resumeCallback)`
  - Registers optional callbacks used when the portal needs the application web server to suspend or resume.
  - The callbacks are optional and may be `nullptr`.

## Internal Behavior

### STA Connection Flow

- When STA credentials are present, the class starts connection attempts with `WiFi.begin()`.
- If the connection fails repeatedly, it falls back to the portal.
- Retry timing is handled internally.

### Portal Flow

- The portal is started in non-blocking mode.
- If a callback is registered, the class calls the suspend callback before the portal takes over port 80.
- When the portal completes and STA connects, the class marks new credentials as pending and calls the resume callback.

### Identity Normalization

- AP SSID and hostname are normalized with a MAC suffix.
- The class strips older suffix formats before appending the current MAC-based suffix.
- The fallback values are `DEFAULT_AP_SSID` and `DEFAULT_WIFI_HOSTNAME`.

Current suffix detail:

- Effective format: `<base>-xxyyzz`
- Source: last 3 bytes of station MAC address

## Integration in This Repository

The project uses a shared global instance:
- `wifiManagerExt`

The application currently registers the web UI suspend/resume hooks and then calls `begin()` during startup.

The main loop calls `update()` regularly.

## Reuse Notes

To reuse this class in another project:
1. Keep the same WiFiManager dependency.
2. Provide `WifiSettings` values from your own persistence layer.
3. Call `begin()` once at startup.
4. Call `update()` frequently.
5. If your app runs a web server on port 80, register suspend/resume callbacks with `setPortalCallbacks()`.
6. Use `consumeNewStaCredentials()` to handle newly entered credentials after portal completion.

[Back to README](README.md)
