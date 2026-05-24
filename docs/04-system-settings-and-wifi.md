# 4. System Settings and WiFi

## 4.1 Opening System Settings

- Use **encoder long press** to open or close the menu.
- Menu title: `System Settings`.

## 4.2 Menu Entries

Read-only status entries:

1. `SSID: ...`
2. `IP: ...`
3. `MAC: ...`

Action entries:

4. `Erase WiFi Credentials`
5. `Start WiFi Manager`
6. `Set Theme (...)`
7. `Rotate Display (...)`
8. `Encoder Order (...)`
9. `Exit`

## 4.3 Menu Navigation

- Encoder left/right: move selection
- Encoder short/medium press: execute selected action
- KEY0 short press: close menu (or close confirmation dialog)

## 4.4 WiFi Manager Flow

`Start WiFi Manager` behavior:

1. Opens confirmation dialog (`No` / `Yes`)
2. On `Yes`, starts captive portal mode
3. UI shows waiting screen with AP name
4. After credentials are entered and station connects, credentials are saved and device restarts

Portal AP identity format:

- `<base>-xxyyzz`
- `xxyyzz` = last 3 bytes of device MAC

## 4.5 Erase WiFi Credentials

`Erase WiFi Credentials` behavior:

1. Opens confirmation dialog
2. On `Yes`, shows restart message
3. Triggers credential erase + reboot path

## 4.6 Runtime Settings Persistence

The following values are persisted in `/settings.json`:

- Display rotation
- Theme color index
- Encoder direction reversal (A-B/B-A)

Stored WiFi information is persisted in `/wifiSettings.json`.
