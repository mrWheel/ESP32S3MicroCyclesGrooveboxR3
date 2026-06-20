# `src/systemManager.cpp` — System Commands, WiFi Lifecycle, and Credentials Management

**Purpose:** Central system command dispatcher, WiFi manager lifecycle coordination, WiFi credential storage, and device restart/factory-reset control.

---

## Responsibilities

```
1. Initialize WiFiManagerExt wrapper at boot
2. Load stored WiFi credentials from NVS
3. Optionally disable WiFi auto-connect (for fast boot)
4. Queue and dispatch system commands (restart, erase WiFi, open portal, etc.)
5. Run periodic WiFi manager updates
6. Provide SSID/IP/MAC/portal info strings to UI
7. Monitor WiFi connection status
8. Coordinate WiFi credential save/load between UI and storage
```

---

## System Commands

**SystemCommand enum:**

```cpp
enum class SystemCommand {
  NONE = 0,
  RESTART_DEVICE,          // reboot via esp_restart()
  ERASE_WIFI_CREDENTIALS,  // clear NVS WiFi entries
  OPEN_WIFI_PORTAL,        // start captive WiFi setup
  CLOSE_WIFI_PORTAL,       // stop portal, return to STA
  DISABLE_WIFI,            // turn off WiFi entirely
  ENABLE_WIFI,             // turn WiFi back on
  SAVE_SCREEN_ROTATION,    // persist display rotation
  // ... additional commands as needed
};
```

Commands are queued and executed in SystemTask to avoid blocking audio/UI.

---

## Core State

**systemState struct:**

```cpp
struct {
  WiFiManagerExt wifiManager;
  WifiSettings wifiSettings;
  bool wifiDisabledAtBoot;
  
  StaticQueue_t commandQueueStruct;
  QueueHandle_t commandQueue;
  
  String currentSsid;
  String currentIpAddress;
  String currentMacAddress;
  String portalApSsid;
  
  uint32_t wifiConnectAttempts;
  uint32_t lastWifiCheckTime;
} systemState;
```

---

## Key Internal Functions

**ensureWifiSettingsFsMounted() → bool:**

Mounts LittleFS to allow WiFi credential storage if NVS becomes full.

**loadStoredWifiCredentials(WifiSettings& out) → bool:**

```
1. Open NVS namespace "wifi"
2. Read SSID, password, hostname
3. Return true if found, false if missing (first boot)
```

**saveWifiCredentials(const WifiSettings& in) → bool:**

```
1. Open NVS namespace "wifi"
2. Write SSID, password, hostname
3. Commit to flash
4. Return true if successful
```

**updateWifiStatus():**

```
1. Check WiFi connection status
2. Update currentIpAddress, currentMacAddress
3. Report changes to UI via status indicators
4. Handle auto-reconnect retry logic
```

**executeSystemCommand(SystemCommand cmd):**

```
switch (cmd) {
  case RESTART_DEVICE:
    esp_restart();
    break;
  
  case ERASE_WIFI_CREDENTIALS:
    NVS.erase("wifi");
    break;
  
  case OPEN_WIFI_PORTAL:
    wifiManager.startPortal();
    break;
  
  // ... other commands
}
```

---

## Public Functions

### `systemManagerInit()`

**Purpose:** Initialize system manager and WiFi.

**Actions:**

1. Create command queue
2. Load runtime settings from NVS
3. Create WiFiManagerExt instance
4. Load stored WiFi credentials
5. Call `wifiManager.begin()` (may skip if WiFi disabled at boot)
6. Initialize status polling

**Call during:** `setup()`, after sequencer and UI manager init

### `systemManagerUpdate()`

**Purpose:** Run periodic WiFi updates and dispatch queued commands.

**Actions:**

1. Check for pending system commands in queue
2. Execute each command
3. Update WiFi status (connection check, IP polling)
4. Update UI info strings (SSID, IP address)

**Call frequency:** ~100 ms (from SystemTask)

### `systemManagerQueueCommand(SystemCommand cmd) → bool`

**Purpose:** Queue a system command for execution.

**Parameters:**

- `cmd` — command to queue

**Returns:** true if queued successfully, false if queue full

**Typical callers:**

- UI → "Restart" button → RESTART_DEVICE
- UI → "Erase WiFi" → ERASE_WIFI_CREDENTIALS
- UI → "WiFi Setup" → OPEN_WIFI_PORTAL

### `systemManagerGetSsid() → String`

**Purpose:** Return current or stored SSID display string.

**Returns:** e.g., "MyNetwork" or "(not connected)"

Used by UI settings screen.

### `systemManagerGetIpAddress() → String`

**Purpose:** Return device's current IP address.

**Returns:** e.g., "192.168.1.123" or "(no IP)"

### `systemManagerGetMacAddress() → String`

**Purpose:** Return device's MAC address.

**Returns:** e.g., "AA:BB:CC:DD:EE:FF"

### `systemManagerGetPortalApSsid() → String`

**Purpose:** Return WiFi portal AP name.

**Returns:** e.g., "Groovebox-AABB"

Used by UI to show "Connect to this network" message when portal is active.

### `systemManagerIsWifiPortalActive() → bool`

**Purpose:** Return whether WiFi portal is currently open.

**Returns:** true if captive setup page is available

### `systemManagerIsWifiConnected() → bool`

**Purpose:** Return whether device is connected to WiFi in STA mode.

**Returns:** true if WiFi connected with valid IP

---

## Command Queue

Commands are stored in a FreeRTOS queue created in `main.cpp`:

```cpp
StaticQueue_t sysCommandQueueStruct;
SystemCommand commandBuffer[16];
QueueHandle_t systemCommandQueue = xQueueCreateStatic(
  16, sizeof(SystemCommand), ...);
```

UiTask pushes commands; SystemTask consumes and executes.

---

## WiFi Credential Persistence

Credentials are stored in NVS under namespace "wifi":

| Key | Type | Example |
|-----|------|---------|
| `ssid` | String | "MyNetwork" |
| `password` | String | "secretPassword" |
| `hostname` | String | "groovebox" |

On boot, `systemManagerInit()` loads these and attempts auto-connect. User can erase them via System Settings → WiFi → "Erase Credentials".

---

## Boot Modes

**Normal boot (WiFi enabled):**

```
systemManagerInit()
  ↓
Load WiFi credentials
  ↓
wifiManager.begin() → attempt STA auto-connect
  ↓
If connected → normal operation
  ↓
If fails → wait for user to open portal
```

**Fast boot (WiFi disabled):**

```
If wifiDisabledAtBoot is set:
  Skip all WiFi init
  Device runs offline (no connectivity)
  UI allows manual enable via System Settings
```

---

## Dependencies

- `WiFiManagerExt` — WiFi management wrapper
- `settingsStore.h` — NVS/LittleFS access
- `systemManager.h` — status strings
- FreeRTOS (command queue)
- ESP32 WiFi HAL

---

## Important Implementation Notes

1. **Command queue isolation.** All system commands queued, never executed directly from UI callbacks. Prevents blocking audio or other tasks.

2. **WiFi optional.** Device works without WiFi. Audio, local sequencing, pattern editing all work offline. WiFi is convenience only.

3. **Credential storage secure.** Passwords stored in NVS (not encrypted by default; consider hardware secure storage for production).

4. **Portal non-blocking.** WiFi setup happens in SystemTask, doesn't freeze audio or UI.

5. **Status polling polls continuously.** IP address and connection status updated every `systemManagerUpdate()` call.

6. **Boot speed optimized.** If WiFi disabled at boot, device skips WiFi init entirely, reducing startup time.

---

[⬆ UP](developerBuildGuide.md#20-source-file-reference) | [📖 README](../README.md#)
