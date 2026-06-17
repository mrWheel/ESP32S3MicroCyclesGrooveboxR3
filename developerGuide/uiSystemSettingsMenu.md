# `src/uiSystemSettingsMenu.cpp`

[Back](developerBuildGuide.md)

---

## Purpose

This file draws the System Settings menu list. It does not execute menu actions; action execution lives in `uiManager.cpp`.

---

## Responsibilities

```text
build visible menu item strings
show SSID/IP/MAC informational rows
show Save Group dirty marker
show Card Storage, sample set and WiFi actions
show theme/rotation/encoder settings
manage first visible index for scrolling
```

---

## Important Implementation Notes

- Keep item order synchronized with `executeMenuAction()` in `uiManager.cpp`.
- Informational rows should remain disabled.
- If a new menu entry is inserted, update both the entry count and action mapping.

---

## Important Internal Areas

```text
systemSettingsEntryCount
fitSystemSettingsRowText()
updateSystemSettingsFirstVisibleIndex()
disabled informational rows
Save Group * dirty indicator
```

---

## Public Functions

### `uiSystemSettingsMenuDraw(...)`

Draws the menu with current network, display, encoder and dirty-group state.


---

[UP](developerBuildGuide.md) | [README](../README.md)
