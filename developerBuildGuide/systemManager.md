# `src/systemManager.cpp` — System Commands, WiFi Lifecycle, and NVS Credential Reconnect

**Purpose:** Central system command dispatcher, WiFiManager lifecycle coordination, ESP32 WiFi NVS reconnect, and device restart/credential erase control.

---

## Responsibilities

```text
1. Create the system command queue
2. Try to reconnect at boot using ESP32 WiFi credentials stored in NVS
3. Configure WiFi persistent mode and auto-reconnect
4. Keep cached SSID/IP information for the settings menu
5. Start the WiFiManager portal when requested from the UI
6. Detect newly entered WiFiManager credentials
7. Restart after new WiFi credentials are accepted
8. Erase ESP32 WiFi NVS credentials when requested
9. Provide SSID/IP/MAC/portal info strings to UI
10. Report whether the WiFiManager portal is active
```

---

## System Commands

**SystemCommand enum:**

```cpp
enum class SystemCommand : uint8_t
{
  none = 0,
  startWifiManager,
  eraseWifiCredentials,
  restartNow
};
```

Commands are queued from the UI and executed in `SystemTask` to avoid blocking input and display handlers.

---

## Core State

```cpp
static StaticQueue_t commandQueueStruct;
static uint8_t commandQueueStorage[8 * sizeof(SystemCommand)];
static QueueHandle_t commandQueue = nullptr;

static String cachedConnectedSsid = "";
static String cachedConnectedIp = "";
```

The cached SSID/IP values are display information only. WiFi passwords are **not** stored by `systemManager.cpp` or `settingsStore.cpp`.

---

## Key Internal Functions

### `connectUsingStoredNvsCredentials() -> bool`

**Purpose:** Try to reconnect using credentials stored by the ESP32 WiFi stack in NVS.

**Flow:**

```text
WiFi.mode(WIFI_STA)
WiFi.persistent(true)
WiFi.setAutoReconnect(true)
WiFi.begin()
wait up to 8 seconds for WL_CONNECTED
cache WiFi.SSID() and WiFi.localIP() if connected
```

This function does not know the password and does not read a project-specific credentials file. The ESP32 WiFi stack owns the credential storage.

---

## Public Functions

### `systemManagerInit()`

**Purpose:** Initialize system manager and attempt WiFi reconnect.

**Actions:**

1. Clear cached SSID/IP.
2. Prepare default `WiFiManagerExt::WifiSettings` for the captive portal.
3. Create the static command queue.
4. Enable WiFi persistence and auto-reconnect.
5. Call `connectUsingStoredNvsCredentials()`.
6. If connected, return with WiFi active.
7. If not connected, initialize `WiFiManagerExt` in disabled mode and continue offline.

**Call during:** `setup()`, after audio engine initialization and before web server/UI initialization.

### `systemManagerUpdate()`

**Purpose:** Run periodic WiFiManager updates and dispatch queued system commands.

**Actions:**

1. Call `wifiManagerExt.update()`.
2. Consume newly entered WiFi credentials from the portal.
3. Keep WiFi persistence and auto-reconnect enabled.
4. Cache the connected SSID/IP for display.
5. Restart after portal credentials are accepted.
6. Process queued commands:
   - start WiFiManager portal
   - erase WiFi credentials from ESP32 NVS
   - restart now

**Call frequency:** from `SystemTask` in `main.cpp`.

### `systemManagerQueueCommand(SystemCommand command)`

**Purpose:** Queue a system command for execution by `SystemTask`.

**Typical callers:**

- System Settings → Start WiFiManager
- System Settings → Erase WiFi credentials
- System Settings → Restart Groovebox

### `systemManagerGetSsid()`

**Purpose:** Return the current connected SSID, cached SSID, or portal AP SSID.

### `systemManagerGetIpAddress()`

**Purpose:** Return the current station IP, cached IP, or portal address string.

### `systemManagerGetMacAddress()`

**Purpose:** Return the device MAC address.

### `systemManagerGetPortalApSsid()`

**Purpose:** Return the WiFiManager portal AP SSID.

### `systemManagerIsWifiPortalActive()`

**Purpose:** Return whether the WiFiManager portal is currently active.

This is passed to `webServerManagerUpdate()` so the Groovebox web server can stop while the portal owns port 80.

---

## Command Queue

`commandQueue` is created in `systemManagerInit()` using static storage:

```cpp
commandQueue = xQueueCreateStatic(8, sizeof(SystemCommand), commandQueueStorage,
                                  &commandQueueStruct);
```

The queue keeps UI actions non-blocking and lets `SystemTask` perform restart/WiFi operations safely.

---

## WiFi Credential Persistence

WiFi credentials are stored by the **ESP32 WiFi stack in NVS**, not by LittleFS and not by a project JSON file.

Important calls:

```cpp
WiFi.persistent(true);
WiFi.setAutoReconnect(true);
WiFi.begin();
```

- `WiFi.begin()` without SSID/password uses stored ESP32 NVS credentials.
- New credentials entered through WiFiManager are accepted by the WiFi stack and then the firmware restarts.
- Erase uses `WiFi.disconnect(true, true)`, which removes stored WiFi credentials from ESP32 NVS.

There is no `/littlefs/wifiSettings.json` credentials file in the current design.

---

## Boot Modes

**Connected boot:**

```text
systemManagerInit()
  ↓
connectUsingStoredNvsCredentials()
  ↓
WiFi connected with valid IP
  ↓
webServerManager can start from SystemTask
```

**Offline boot:**

```text
systemManagerInit()
  ↓
connectUsingStoredNvsCredentials() fails or no credentials exist
  ↓
WiFiManagerExt is initialized disabled
  ↓
Groovebox continues fully offline
  ↓
User may open WiFiManager from System Settings
```

**WiFiManager portal flow:**

```text
UI queues startWifiManager
  ↓
systemManagerUpdate() enables WiFiManagerExt and starts portal
  ↓
User enters credentials
  ↓
WiFi connects
  ↓
systemManagerUpdate() logs credentials stored in ESP32 NVS
  ↓
firmware restarts
```

---

## Dependencies

- `WiFiManagerExtClass.h` — captive portal wrapper
- `appConfig.h` — default AP SSID/password/hostname
- `WiFi.h` — ESP32 WiFi stack and NVS-backed credential handling
- `esp_system.h` — `esp_restart()`
- FreeRTOS queue API

---

## Important Implementation Notes

1. **Do not store WiFi passwords in LittleFS.** Credentials are owned by the ESP32 WiFi stack/NVS.
2. **WiFi is optional.** The Groovebox must boot and play without WiFi.
3. **System commands must remain queued.** Do not restart or start the portal directly from menu rendering code.
4. **WiFiManager portal and web server cannot both own port 80.** `webServerManagerUpdate()` receives portal state and stops the web server when needed.
5. **Boot reconnect timeout is intentionally bounded.** Do not block startup indefinitely waiting for WiFi.

---

[⬆ UP](developerBuildGuide.md#20-source-file-reference) | [📖 README](../README.md#)
