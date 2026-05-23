/*** Last Changed: 2026-05-23 - 16:00 ***/
#include "settingsStore.h"
#include "appConfig.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <esp_log.h>

//-- Logging tag.
static const char* logTag = "SettingsStore";

//-- Settings file path.
static const char* settingsPath = "/settings.json";

//-- Default runtime settings.
static RuntimeSettings defaultRuntimeSettings()
{
  RuntimeSettings settings;
  int defaultThemeIndex = DEFAULT_THEME_COLOR - 1;

  if (defaultThemeIndex < 0)
  {
    defaultThemeIndex = 0;
  }

  settings.displayRotation = static_cast<uint8_t>(DEFAULT_DISPLAY_ROTATION);
  settings.themeColorIndex = defaultThemeIndex;
  settings.encoderDirectionReversed = false;

  return settings;

} //   defaultRuntimeSettings()

//-- Ensure LittleFS is mounted for settings access.
static bool ensureSettingsFsMounted()
{
  static bool mounted = false;

  if (mounted)
  {
    return true;
  }

  if (!LittleFS.begin(true))
  {
    ESP_LOGW(logTag, "LittleFS mount failed; using defaults");
    return false;
  }

  mounted = true;
  return true;

} //   ensureSettingsFsMounted()

//-- Return default display rotation until persistent settings are added.
uint8_t settingsStoreLoadDisplayRotation()
{
  RuntimeSettings settings;

  settingsStoreLoadRuntimeSettings(settings);
  return settings.displayRotation;

} //   settingsStoreLoadDisplayRotation()

//-- Load runtime settings from settings.json.
void settingsStoreLoadRuntimeSettings(RuntimeSettings& settings)
{
  RuntimeSettings defaults = defaultRuntimeSettings();

  settings = defaults;

  if (!ensureSettingsFsMounted())
  {
    return;
  }

  if (!LittleFS.exists(settingsPath))
  {
    return;
  }

  File file = LittleFS.open(settingsPath, "r");

  if (!file)
  {
    ESP_LOGW(logTag, "Failed to open %s for read", settingsPath);
    return;
  }

  JsonDocument jsonDocument;
  DeserializationError error = deserializeJson(jsonDocument, file);

  file.close();

  if (error)
  {
    ESP_LOGW(logTag, "Invalid %s (%s); using defaults", settingsPath, error.c_str());
    return;
  }

  settings.displayRotation = static_cast<uint8_t>(jsonDocument["displayRotation"] | defaults.displayRotation);
  settings.themeColorIndex = static_cast<int>(jsonDocument["themeColorIndex"] | defaults.themeColorIndex);
  settings.encoderDirectionReversed = static_cast<bool>(jsonDocument["encoderDirectionReversed"] | defaults.encoderDirectionReversed);

} //   settingsStoreLoadRuntimeSettings()

//-- Save runtime settings to settings.json.
bool settingsStoreSaveRuntimeSettings(const RuntimeSettings& settings)
{
  if (!ensureSettingsFsMounted())
  {
    return false;
  }

  File file = LittleFS.open(settingsPath, "w");

  if (!file)
  {
    ESP_LOGW(logTag, "Failed to open %s for write", settingsPath);
    return false;
  }

  JsonDocument jsonDocument;

  jsonDocument["displayRotation"] = settings.displayRotation;
  jsonDocument["themeColorIndex"] = settings.themeColorIndex;
  jsonDocument["encoderDirectionReversed"] = settings.encoderDirectionReversed;

  bool success = (serializeJson(jsonDocument, file) > 0);

  file.close();

  if (!success)
  {
    ESP_LOGW(logTag, "Failed to serialize %s", settingsPath);
  }

  return success;

} //   settingsStoreSaveRuntimeSettings()