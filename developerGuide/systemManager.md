# `src/systemManager.cpp`

[Back](developerBuildGuide.md)

---

## Purpose

This file owns system-level commands, WiFi manager lifecycle, WiFi credentials, restart requests and network status values for the settings menu.

---

## Responsibilities

```text
initialize WiFi manager wrapper
load stored WiFi info
optionally disable auto-connect at boot
queue restart / erase WiFi / start portal commands
run WiFi manager update loop
provide SSID/IP/MAC/portal strings to UI
```

---

## Important Implementation Notes

- System commands should be queued, not executed directly from UI callbacks.
- WiFi may be disabled at boot to keep audio/UI startup fast.
- Portal screens are UI-visible but controlled by system state.

---

## Important Internal Areas

```text
ensureWifiSettingsFsMounted()
loadStoredWifiCredentials()
saveWifiCredentials()
system command queue variable
WiFiManagerExt wrapper instance
portal AP SSID reporting
WiFi disabled-at-boot behavior
```

---

## Public Functions

### `systemManagerInit()`

Initializes stored WiFi state and WiFi manager wrapper.

### `systemManagerUpdate()`

Runs queued system commands and WiFi manager updates.

### `systemManagerQueueCommand(SystemCommand)`

Queues a system command for SystemTask.

### `systemManagerGetSsid()`

Returns stored/current SSID display string.

### `systemManagerGetIpAddress()`

Returns IP display string.

### `systemManagerGetMacAddress()`

Returns MAC display string.

### `systemManagerGetPortalApSsid()`

Returns portal AP SSID.

### `systemManagerIsWifiPortalActive()`

Returns whether portal mode is active.


---

[UP](developerBuildGuide.md) | [README](../README.md)
