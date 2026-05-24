/*** Last Changed: 2026-05-24 - 11:12 ***/
#include "settingsStore.h"
#include "appConfig.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <esp_log.h>

//-- Logging tag.
static const char* logTag = "SettingsStore";

//-- Settings file path.
static const char* settingsPath = "/settings.json";
static const char* sequenceDirectoryPath = "/sequences";
static const char* sequenceExtension = ".json";

//-- Ensure LittleFS is mounted for settings access.
static bool ensureSettingsFsMounted();

//-- Build safe sequence base name.
static String normalizeSequenceName(const String& sequenceName)
{
  String normalizedName = "";

  for (size_t charIndex = 0; charIndex < sequenceName.length(); charIndex++)
  {
    char currentChar = sequenceName[charIndex];
    bool allowedChar = ((currentChar >= 'a' && currentChar <= 'z') ||
                        (currentChar >= 'A' && currentChar <= 'Z') ||
                        (currentChar >= '0' && currentChar <= '9') ||
                        currentChar == '_' ||
                        currentChar == '-');

    if (allowedChar)
    {
      normalizedName += currentChar;
    }
  }

  if (normalizedName.length() == 0)
  {
    normalizedName = "sequence";
  }

  if (normalizedName.length() > 24)
  {
    normalizedName = normalizedName.substring(0, 24);
  }

  return normalizedName;

} //   normalizeSequenceName()

//-- Ensure sequence directory exists.
static bool ensureSequenceDirectory()
{
  if (!ensureSettingsFsMounted())
  {
    return false;
  }

  if (LittleFS.exists(sequenceDirectoryPath))
  {
    return true;
  }

  if (!LittleFS.mkdir(sequenceDirectoryPath))
  {
    ESP_LOGW(logTag, "Failed to create %s", sequenceDirectoryPath);
    return false;
  }

  return true;

} //   ensureSequenceDirectory()

//-- Build full sequence file path.
static String buildSequencePath(const String& sequenceName)
{
  String normalizedName = normalizeSequenceName(sequenceName);
  String sequencePath = String(sequenceDirectoryPath) + "/" + normalizedName + sequenceExtension;

  return sequencePath;

} //   buildSequencePath()

//-- Extract sequence name from absolute file path.
static String sequenceNameFromPath(const String& fullPath)
{
  int slashIndex = fullPath.lastIndexOf('/');
  String fileName = (slashIndex >= 0) ? fullPath.substring(slashIndex + 1) : fullPath;

  if (!fileName.endsWith(sequenceExtension))
  {
    return "";
  }

  return fileName.substring(0, fileName.length() - strlen(sequenceExtension));

} //   sequenceNameFromPath()

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

  if (!LittleFS.begin(true, "/littlefs", 10, "littlefs"))
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

//-- List available sequence names in sorted order.
bool settingsStoreListSequences(String sequenceNames[], size_t maxCount, size_t& outCount)
{
  outCount = 0;

  if (sequenceNames == nullptr || maxCount == 0)
  {
    return false;
  }

  if (!ensureSequenceDirectory())
  {
    return false;
  }

  File directory = LittleFS.open(sequenceDirectoryPath, "r");

  if (!directory)
  {
    ESP_LOGW(logTag, "Failed to open sequence directory");
    return false;
  }

  File entry = directory.openNextFile();

  while (entry && outCount < maxCount)
  {
    if (!entry.isDirectory())
    {
      String sequenceName = sequenceNameFromPath(entry.name());

      if (!sequenceName.isEmpty())
      {
        sequenceNames[outCount] = sequenceName;
        outCount++;
      }
    }

    entry.close();
    entry = directory.openNextFile();
  }

  directory.close();

  for (size_t leftIndex = 0; leftIndex < outCount; leftIndex++)
  {
    for (size_t rightIndex = leftIndex + 1; rightIndex < outCount; rightIndex++)
    {
      if (sequenceNames[rightIndex].compareTo(sequenceNames[leftIndex]) < 0)
      {
        String temporaryName = sequenceNames[leftIndex];
        sequenceNames[leftIndex] = sequenceNames[rightIndex];
        sequenceNames[rightIndex] = temporaryName;
      }
    }
  }

  return true;

} //   settingsStoreListSequences()

//-- Find next available default sequence name.
bool settingsStoreFindNextSequenceName(String& outName)
{
  if (!ensureSequenceDirectory())
  {
    return false;
  }

  for (int sequenceIndex = 1; sequenceIndex <= 999; sequenceIndex++)
  {
    char candidateName[24];

    snprintf(candidateName, sizeof(candidateName), "sequence_%03d", sequenceIndex);

    String sequencePath = buildSequencePath(String(candidateName));

    if (!LittleFS.exists(sequencePath))
    {
      outName = String(candidateName);
      return true;
    }
  }

  return false;

} //   settingsStoreFindNextSequenceName()

//-- Save sequence payload to one JSON file.
bool settingsStoreSaveSequence(const String& sequenceName, const SequenceData& sequenceData)
{
  if (!ensureSequenceDirectory())
  {
    return false;
  }

  String normalizedName = normalizeSequenceName(sequenceName);
  String sequencePath = buildSequencePath(normalizedName);
  File file = LittleFS.open(sequencePath, "w");

  if (!file)
  {
    ESP_LOGW(logTag, "Failed to open %s for write", sequencePath.c_str());
    return false;
  }

  JsonDocument jsonDocument;
  JsonArray tracks = jsonDocument["tracks"].to<JsonArray>();

  jsonDocument["version"] = 1;
  jsonDocument["name"] = normalizedName;
  jsonDocument["bpm"] = sequenceData.bpm;
  jsonDocument["swingPercent"] = sequenceData.swingPercent;

  for (uint8_t trackIndex = 0; trackIndex < sequencerTrackCount; trackIndex++)
  {
    JsonObject trackObject = tracks.add<JsonObject>();
    JsonArray steps = trackObject["steps"].to<JsonArray>();

    trackObject["mute"] = sequenceData.pattern.tracks[trackIndex].mute;

    for (uint8_t stepIndex = 0; stepIndex < sequencerStepCount; stepIndex++)
    {
      JsonObject stepObject = steps.add<JsonObject>();
      const Step& step = sequenceData.pattern.tracks[trackIndex].steps[stepIndex];

      stepObject["trigger"] = step.trigger;
      stepObject["velocity"] = step.velocity;
    }
  }

  bool success = (serializeJson(jsonDocument, file) > 0);

  file.close();

  if (!success)
  {
    ESP_LOGW(logTag, "Failed to serialize %s", sequencePath.c_str());
  }

  return success;

} //   settingsStoreSaveSequence()

//-- Load one sequence JSON file into runtime payload.
bool settingsStoreLoadSequence(const String& sequenceName, SequenceData& sequenceData)
{
  String sequencePath = buildSequencePath(sequenceName);

  if (!ensureSequenceDirectory() || !LittleFS.exists(sequencePath))
  {
    return false;
  }

  File file = LittleFS.open(sequencePath, "r");

  if (!file)
  {
    ESP_LOGW(logTag, "Failed to open %s for read", sequencePath.c_str());
    return false;
  }

  JsonDocument jsonDocument;
  DeserializationError error = deserializeJson(jsonDocument, file);

  file.close();

  if (error)
  {
    ESP_LOGW(logTag, "Invalid %s (%s)", sequencePath.c_str(), error.c_str());
    return false;
  }

  sequenceData.bpm = static_cast<uint16_t>(jsonDocument["bpm"] | 120);
  sequenceData.swingPercent = static_cast<uint8_t>(jsonDocument["swingPercent"] | 8);

  JsonArray tracks = jsonDocument["tracks"].as<JsonArray>();

  if (tracks.isNull() || tracks.size() < sequencerTrackCount)
  {
    ESP_LOGW(logTag, "Missing tracks in %s", sequencePath.c_str());
    return false;
  }

  for (uint8_t trackIndex = 0; trackIndex < sequencerTrackCount; trackIndex++)
  {
    JsonObject trackObject = tracks[trackIndex].as<JsonObject>();
    JsonArray steps = trackObject["steps"].as<JsonArray>();

    sequenceData.pattern.tracks[trackIndex].mute = static_cast<bool>(trackObject["mute"] | false);

    if (steps.isNull() || steps.size() < sequencerStepCount)
    {
      ESP_LOGW(logTag, "Missing steps in %s", sequencePath.c_str());
      return false;
    }

    for (uint8_t stepIndex = 0; stepIndex < sequencerStepCount; stepIndex++)
    {
      JsonObject stepObject = steps[stepIndex].as<JsonObject>();
      Step& step = sequenceData.pattern.tracks[trackIndex].steps[stepIndex];

      step.trigger = static_cast<bool>(stepObject["trigger"] | false);
      step.velocity = static_cast<uint8_t>(stepObject["velocity"] | 255);
    }
  }

  return true;

} //   settingsStoreLoadSequence()

//-- Remove one stored sequence file.
bool settingsStoreDeleteSequence(const String& sequenceName)
{
  String sequencePath = buildSequencePath(sequenceName);

  if (!ensureSequenceDirectory() || !LittleFS.exists(sequencePath))
  {
    return false;
  }

  return LittleFS.remove(sequencePath);

} //   settingsStoreDeleteSequence()