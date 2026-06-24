# `src/webServerManager.cpp` — HTTP Server Runtime Manager

**Purpose:** Own the lightweight HTTP server used as the foundation for a future SPA control surface. The current implementation exposes minimal test endpoints, starts only when WiFi is connected, and avoids conflicting with the WiFiManager captive portal.

---

## Responsibilities

```text
initialize web server manager state
start HTTP server when station WiFi has a valid IP
stop HTTP server when WiFi disconnects or WiFiManager portal is active
serve a simple root text endpoint
serve a minimal JSON status endpoint for future SPA use
handle unknown routes with a 404 response
log server start/stop events to the Serial Monitor
optionally write the server IP to the boot log before the UI opens
expose server running state and URL to other modules
```

---

## Important Runtime Behavior

The web server uses Arduino `WebServer` on port `80`.

It does **not** start while the WiFiManager portal is active. WiFiManager also uses a web server during captive portal setup, so `webServerManagerUpdate()` stops the Groovebox web server whenever `wifiPortalActive` is true.

The boot display logging is controlled with `webServerManagerSetBootLogEnabled()`. This prevents late web-server messages from redrawing the boot log after the Groovebox UI has already opened.

---

## Internal State

```cpp
static WebServer webServer(80);
static bool webServerRunning = false;
static bool bootLogEnabled = true;
static String webServerUrl = "";
```

- `webServerRunning` prevents duplicate `begin()` calls.
- `bootLogEnabled` is true during boot and disabled after the first Groovebox UI frame.
- `webServerUrl` stores the active URL, for example `http://192.168.12.83`.

---

## HTTP Endpoints

### `/`

Returns plain text with the firmware name, version and a ready message.

### `/api/status`

Returns minimal JSON for future SPA integration:

```json
{
  "device": "ESP32-S3 MicroCycles Groovebox",
  "version": "v1.4.5",
  "ip": "192.168.12.83",
  "rssi": -55
}
```

### Any unknown route

Returns HTTP `404` with `Not found`.

---

## Internal Functions

### `handleRootRequest()`

Builds and sends the plain-text root response.

### `handleStatusRequest()`

Builds and sends the JSON status response. This is the first stable API endpoint for a future SPA.

### `handleNotFoundRequest()`

Sends a short 404 response for unknown paths.

### `startWebServer()`

Starts the HTTP server only when all conditions are true:

```text
webServerRunning == false
WiFi.status() == WL_CONNECTED
WiFi.localIP() != 0.0.0.0
```

It registers the HTTP routes, calls `webServer.begin()`, stores the URL and logs:

```text
Webserver started at http://<IP>
```

If boot logging is still enabled, it also writes the IP address to the boot display.

### `stopWebServer()`

Stops the web server if it is running and clears the stored URL.

---

## Public Functions

### `webServerManagerInit()`

**Purpose:** Reset web server runtime state during boot.

**Called by:** `setup()` in `main.cpp`.

---

### `webServerManagerUpdate(bool wifiPortalActive)`

**Purpose:** Periodic service function.

**Behavior:**

```text
if WiFiManager portal is active:
  stop web server
else if WiFi is disconnected:
  stop web server
else if web server is not running:
  start web server
else:
  handle pending HTTP client requests
```

**Called by:** `systemTask()` in `main.cpp`.

---

### `webServerManagerSetBootLogEnabled(bool enabled)`

**Purpose:** Enable or disable boot-display logging from the web server manager.

`main.cpp` disables this after `uiManagerUpdate()` has drawn the first Groovebox frame. This prevents the boot log from reappearing over the normal UI.

---

### `webServerManagerIsRunning()`

**Purpose:** Return whether the HTTP server is currently active.

---

### `webServerManagerGetUrl()`

**Purpose:** Return the active URL or an empty string when the server is not running.

---

## Interaction With Other Modules


- `systemManager.cpp` connects WiFi using credentials stored in ESP32 NVS and reports portal state.
- `main.cpp` calls `webServerManagerUpdate(systemManagerIsWifiPortalActive())` from `SystemTask`.
- `DisplayDriverClass.cpp` provides `displayBootLogInfo()` for optional boot-time IP logging.
- `WiFiManagerExtClass.cpp` owns the captive portal; the web server manager backs off while the portal is active.

---

## Important Implementation Notes

1. **Do not serve SPA files yet.** This file only provides the server foundation and minimal endpoints.
2. **Do not start while WiFiManager portal is active.** Both services would otherwise compete for port 80.
3. **Do not draw to the boot log after UI opens.** Use `webServerManagerSetBootLogEnabled(false)` after the first UI frame.
4. **Keep handlers short.** Avoid SD card scans, pattern writes or long blocking work inside HTTP request handlers.
5. **Future SPA work should add API endpoints here or delegate to small API-specific modules.**

---

[⬆ UP](developerBuildGuide.md#20-source-file-reference) | [📖 README](../README.md#)
