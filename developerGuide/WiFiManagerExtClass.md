# `src/WiFiManagerExtClass.cpp`

[Back](developerBuildGuide.md)

---

## Purpose

This file wraps the third-party WiFiManager library so the rest of the firmware has a stable, project-specific WiFi API.

---

## Responsibilities

```text
store WiFi settings
start/stop configuration portal
scan visible networks
connect as station
track portal state
normalize portal identity and hostname
communicate portal suspend/resume callbacks
report newly entered station credentials
```

---

## Important Implementation Notes

- Keep third-party WiFiManager details here.
- Do not let UI code depend directly on WiFiManager internals.
- Portal identity includes a MAC suffix to reduce AP name collisions.

---

## Important Internal Areas

```text
WiFiManager instance wrapping
portal callbacks
station attempt logic
MAC suffix identity generation
consumePortalStartedApSsid()
consumeNewStaCredentials()
disabled mode support
```

---

## Public Functions

### `WiFiManagerExt::setSettings(...)`

Stores WiFi settings for later use.

### `WiFiManagerExt::begin(bool disabled)`

Initializes WiFi manager behavior.

### `WiFiManagerExt::setPortalCallbacks(...)`

Registers suspend/resume callbacks.

### `WiFiManagerExt::update()`

Runs periodic WiFi manager state handling.

### `WiFiManagerExt::applySettings(...)`

Applies a new settings object.

### `WiFiManagerExt::getSettings()`

Returns current settings.

### `WiFiManagerExt::startPortal()`

Starts the WiFi configuration portal.

### `WiFiManagerExt::stopPortal()`

Stops the portal.

### `WiFiManagerExt::setDisabled(bool)`

Enables/disables WiFi manager runtime behavior.

### `WiFiManagerExt::isDisabled()`

Returns disabled state.

### `WiFiManagerExt::getPortalApSsid()`

Returns portal AP name.

### `WiFiManagerExt::consumePortalStartedApSsid(...)`

Reports portal AP name once.

### `WiFiManagerExt::scanNetworks(...)`

Scans SSIDs into caller buffer.

### `WiFiManagerExt::isStaConnected()`

Returns station connection state.

### `WiFiManagerExt::shouldOpenPortal()`

Returns whether portal should be opened.

### `WiFiManagerExt::getAddressString()`

Returns IP address string.

### `WiFiManagerExt::consumeNewStaCredentials(...)`

Returns newly entered credentials once.


---

[UP](developerBuildGuide.md) | [README](../README.md)
