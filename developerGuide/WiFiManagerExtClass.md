# `src/WiFiManagerExtClass.cpp` — WiFi Management and Configuration Portal

**Purpose:** Wrap third-party WiFiManager library with a project-specific, stable API for WiFi credential management, configuration portal, and station/AP mode control.

---

## Responsibilities

```
1. Store and persist WiFi settings (SSID, password, hostname)
2. Initialize WiFi stack (station or AP mode)
3. Start/stop configuration portal (captive login page)
4. Scan available networks
5. Attempt station connection with stored credentials
6. Monitor connection status
7. Report newly entered credentials (from portal)
8. Generate unique AP name with MAC suffix (reduce collisions)
9. Support portal suspend/resume callbacks
10. Support disabled mode (WiFi completely offline)
```

---

## Core Data Structures

**WifiSettings struct:**

```cpp
struct WifiSettings {
  String ssid;
  String password;
  String hostname;
  String portalApName;       // e.g., "Groovebox-A1B2"
  String portalApPassword;   // default or custom
  bool autoReconnectOnBoot;
};
```

Persisted in NVS via settingsStore.

**WiFiManagerExt class:**

```cpp
class WiFiManagerExt {
  WiFiManager wm;            // third-party library instance
  WifiSettings settings;
  bool portalActive;
  bool stationConnected;
  // ... internal state
};
```

---

## WiFi Modes

**Station (STA) mode:**

Connects as a client to an existing WiFi network. Requires valid SSID/password stored in NVS. Typical mode for normal operation.

**Access Point (AP) mode + Portal:**

Creates its own WiFi network ("Groovebox-XXXX"). Users connect to it and access a captive browser login page to enter their home network credentials.

**Both modes supported:**

Device starts in STA mode (if credentials stored). If connection fails or user requests, switch to AP mode + portal.

---

## Portal (Captive Login Page)

When portal is active, user's phone/laptop connects to the Groovebox's WiFi network:

```
1. Phone connects to WiFi SSID "Groovebox-A1B2"
2. Open browser → redirected to captive page
3. User enters home network SSID and password
4. Submit → WiFiManager stores credentials and attempts STA connection
5. On success → Device transitions to STA mode
6. Connection details available via consumeNewStaCredentials()
```

Portal is non-blocking; normal audio/UI operation continues in background.

---

## Key Internal Functions

**buildMacSuffix() → String:**

Reads device MAC address and generates a short suffix:

```
MAC: AA:BB:CC:DD:EE:FF  →  suffix: "-AABB"  or  "-DDEE"
```

Used for unique AP name to avoid collisions with other Grooveboxes nearby.

**stripMacSuffixIfPresent(const String& ssid) → String:**

Removes MAC suffix from AP SSID if present. Useful when reconnecting to a saved AP.

**captivePortalCallback():**

Intercepts DNS queries and redirects all traffic to the portal page. Ensures login form appears even if user tries to visit other URLs.

**portalSuspendCallback() / portalResumeCallback():**

Hooks called when portal starts/stops. Useful for pause audio or adjust UI.

---

## Public Functions

### `WiFiManagerExt::setSettings(const WifiSettings& newSettings)`

**Purpose:** Store WiFi settings for later use (SSID, password, hostname).

Does NOT immediately connect. Call `begin()` or `startStationAttempt()` to use.

### `WiFiManagerExt::begin(bool disabled = false)`

**Purpose:** Initialize WiFi manager and attempt connection.

**Actions:**

1. If `disabled` is true, remain offline (no WiFi)
2. Otherwise, configure WiFi stack
3. Load stored SSID/password from NVS
4. Attempt STA connection
5. If fails, prepare portal (but don't start it yet)

**Call during:** `setup()`, from systemManager

### `WiFiManagerExt::startPortal()`

**Purpose:** Start the configuration portal (AP + captive login).

**Actions:**

1. Transition to AP mode
2. Start WiFiManager autoConnect (creates portal)
3. Set `portalActive = true`
4. Call suspend callback (if registered)

**Typical trigger:** User presses button for "WiFi Setup", or connection fails N times

### `WiFiManagerExt::stopPortal()`

**Purpose:** Stop the configuration portal and return to STA mode.

**Actions:**

1. Stop WiFiManager
2. Set `portalActive = false`
3. Call resume callback
4. Attempt STA connection with stored credentials

### `WiFiManagerExt::update()`

**Purpose:** Run periodic WiFi state handling.

**Actions:**

1. Check WiFiManager status
2. Monitor STA connection state
3. Report new credentials if portal just completed
4. Handle auto-reconnect retries

**Call frequency:** ~100 ms (from SystemTask)

### `WiFiManagerExt::isStaConnected() → bool`

**Purpose:** Return whether device is currently connected to WiFi in STA mode.

**Returns:** true if WiFi is connected and has a valid IP address

### `WiFiManagerExt::shouldOpenPortal() → bool`

**Purpose:** Return whether system should automatically open portal.

Heuristic: STA connection not available OR user requested portal.

### `WiFiManagerExt::getAddressString() → String`

**Purpose:** Return device's IP address (STA mode) or portal AP name (AP mode).

**Returns:** Formatted string like "192.168.1.100" or "Groovebox-A1B2"

### `WiFiManagerExt::getPortalApSsid() → String`

**Purpose:** Return the portal AP network name.

**Returns:** Name like "Groovebox-A1B2" (includes MAC suffix)

### `WiFiManagerExt::consumePortalStartedApSsid(String& out_ssid, String& out_password) → bool`

**Purpose:** Report portal AP name only once after startup.

**Returns:** true if portal just started, false otherwise

Used by UI to notify user "Connect to Groovebox-A1B2 to configure".

### `WiFiManagerExt::consumeNewStaCredentials(String& out_ssid, String& out_password) → bool`

**Purpose:** Return newly entered STA credentials from portal.

**Returns:** true if portal just completed with new credentials

**Actions:**

1. If return true, `out_ssid` and `out_password` contain the new credentials
2. Clear the pending flag so this is reported only once
3. System should save these to NVS and attempt STA connection

### `WiFiManagerExt::scanNetworks(String ssidArray[], uint8_t maxCount) → uint8_t`

**Purpose:** Scan nearby WiFi networks.

**Returns:** Number of networks found, up to maxCount

Used by UI to offer network selection menu.

### `WiFiManagerExt::setDisabled(bool disable)`

**Purpose:** Enable/disable WiFi completely at runtime.

- `true` → WiFi off, device offline
- `false` → WiFi on, resume normal connection attempts

### `WiFiManagerExt::isDisabled() → bool`

**Purpose:** Return whether WiFi is currently disabled.

---

## Connection Flow

**Normal startup (STA mode with saved credentials):**

```
WiFiManagerExt::begin()
  ↓
Load credentials from NVS
  ↓
WiFi.begin(ssid, password)
  ↓
[Wait for connection...]
  ↓
Connected → stationConnected = true, ready for use
  ↓
OR
  ↓
Failed → prepare portal, wait for user to trigger startPortal()
```

**User initiates portal (AP mode + captive page):**

```
startPortal()
  ↓
WiFi.softAP("Groovebox-XXXX")
  ↓
WiFiManager autoConnect (creates captive page)
  ↓
[User connects phone, enters credentials...]
  ↓
Portal completes, stores credentials
  ↓
consumeNewStaCredentials() returns true
  ↓
System saves to NVS and transitions back to STA mode
```

---

## NVS Persistence

WiFi settings are saved to NVS (non-volatile storage) for power-cycle recovery:

```
NVS key "wifi_ssid"      → last known SSID
NVS key "wifi_password"  → last known password
NVS key "wifi_hostname"  → device hostname
```

---

## Dependencies

- `WiFiManager` library (third-party, via platformio.ini)
- `WiFi` (native ESP32 WiFi stack)
- `settingsStore.h` — NVS access
- `esp_system` — WiFi HAL

---

## Important Implementation Notes

1. **Keep WiFiManager internals isolated.** All WiFiManager usage stays in this file. Other modules use only the public WiFiManagerExt API.

2. **Portal is non-blocking.** Audio and UI continue during portal active. No lockups.

3. **MAC suffix uniqueness.** Multiple Grooveboxes nearby won't collide on AP name.

4. **Auto-reconnect on boot.** Device remembers last SSID/password and reconnects automatically after power-cycle (unless disabled).

5. **No blocking I/O from AudioTask.** WiFi updates only in SystemTask.

6. **Graceful offline mode.** Device works fine without WiFi (sequencing, audio, local storage all work). WiFi is optional convenience.

7. **Portal suspend/resume.** Optional callbacks allow UI to show "WiFi setup in progress" overlay.

---

[⬆ UP](developerBuildGuide.md#20-source-file-reference) | [📖 README](../README.md#)
