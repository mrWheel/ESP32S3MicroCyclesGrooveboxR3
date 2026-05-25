/*** Last Changed: 2026-05-25 - 18:06 ***/
#include "settingsStore.h"
#include "appConfig.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <esp_log.h>

//-- Logging tag.
static const char* logTag = "SettingsStore";

//-- Settings and pattern paths.
static const char* settingsPath = "/settings.json";
static const char* patternDirectoryPath = "/patterns";
static const char* patternFileExtension = ".json";
static const char* trackJsonNames[sequencerTrackCount] = {"KICK", "SNARE", "CH", "OH", "TONE", "METAL"};
static const char* trackSampleNames[sequencerTrackCount] = {"kick", "snare", "ch", "oh", "tone", "metal"};

//-- Ensure LittleFS is mounted for settings access.
static bool ensureSettingsFsMounted();

//-- Build safe pattern base name.
static String normalizePatternName(const String& patternName)
{
  String normalizedName = "";

  for (size_t charIndex = 0; charIndex < patternName.length(); charIndex++)
  {
    char currentChar = patternName[charIndex];
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
    normalizedName = "pattern";
  }

  if (normalizedName.length() > 24)
  {
    normalizedName = normalizedName.substring(0, 24);
  }

  return normalizedName;

} //   normalizePatternName()

//-- Ensure pattern directory exists.
static bool ensurePatternDirectory()
{
  if (!ensureSettingsFsMounted())
  {
    return false;
  }

  if (LittleFS.exists(patternDirectoryPath))
  {
    File patternPathEntry = LittleFS.open(patternDirectoryPath, "r");

    if (patternPathEntry && patternPathEntry.isDirectory())
    {
      patternPathEntry.close();
      return true;
    }

    patternPathEntry.close();

    ESP_LOGW(logTag, "%s exists but is not a directory; removing", patternDirectoryPath);

    if (!LittleFS.remove(patternDirectoryPath))
    {
      ESP_LOGW(logTag, "Failed to remove conflicting file %s", patternDirectoryPath);
      return false;
    }
  }

  if (!LittleFS.mkdir(patternDirectoryPath))
  {
    File patternPathEntry = LittleFS.open(patternDirectoryPath, "r");
    bool directoryExists = (patternPathEntry && patternPathEntry.isDirectory());

    if (patternPathEntry)
    {
      patternPathEntry.close();
    }

    if (directoryExists)
    {
      return true;
    }

    ESP_LOGW(logTag, "Failed to create %s", patternDirectoryPath);
    return false;
  }

  return true;

} //   ensurePatternDirectory()

//-- Build full pattern file path.
static String buildPatternPath(const String& patternName)
{
  String normalizedName = normalizePatternName(patternName);
  String patternPath = String(patternDirectoryPath) + "/" + normalizedName + patternFileExtension;

  return patternPath;

} //   buildPatternPath()

//-- Extract pattern name from absolute file path.
static String patternNameFromPath(const String& fullPath)
{
  int slashIndex = fullPath.lastIndexOf('/');
  String fileName = (slashIndex >= 0) ? fullPath.substring(slashIndex + 1) : fullPath;

  if (!fileName.endsWith(patternFileExtension))
  {
    return "";
  }

  return fileName.substring(0, fileName.length() - strlen(patternFileExtension));

} //   patternNameFromPath()

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

//-- Create pattern storage directory early during boot.
bool settingsStoreInitPatternStorage()
{
  return ensurePatternDirectory();

} //   settingsStoreInitPatternStorage()

//-- List available pattern names in sorted order.
bool settingsStoreListPatterns(String patternNames[], size_t maxCount, size_t& outCount)
{
  outCount = 0;

  if (patternNames == nullptr || maxCount == 0)
  {
    return false;
  }

  if (!ensurePatternDirectory())
  {
    return false;
  }

  File directory = LittleFS.open(patternDirectoryPath, "r");

  if (!directory)
  {
    ESP_LOGW(logTag, "Failed to open pattern directory");
    return false;
  }

  File entry = directory.openNextFile();

  while (entry && outCount < maxCount)
  {
    if (!entry.isDirectory())
    {
      String patternName = patternNameFromPath(entry.name());

      if (!patternName.isEmpty())
      {
        patternNames[outCount] = patternName;
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
      if (patternNames[rightIndex].compareTo(patternNames[leftIndex]) < 0)
      {
        String temporaryName = patternNames[leftIndex];
        patternNames[leftIndex] = patternNames[rightIndex];
        patternNames[rightIndex] = temporaryName;
      }
    }
  }

  return true;

} //   settingsStoreListPatterns()

//-- Find next available default pattern name.
bool settingsStoreFindNextPatternName(String& outName)
{
  if (!ensurePatternDirectory())
  {
    return false;
  }

  for (int patternIndex = 1; patternIndex <= 999; patternIndex++)
  {
    char candidateName[24];

    snprintf(candidateName, sizeof(candidateName), "P%03d", patternIndex);

    String patternPath = buildPatternPath(String(candidateName));

    if (!LittleFS.exists(patternPath))
    {
      outName = String(candidateName);
      return true;
    }
  }

  return false;

} //   settingsStoreFindNextPatternName()

//-- Save pattern payload to one JSON file.
bool settingsStoreSavePattern(const String& patternName, const PatternData& patternData)
{
  if (!ensurePatternDirectory())
  {
    return false;
  }

  String normalizedName = normalizePatternName(patternName);
  String patternPath = buildPatternPath(normalizedName);
  File file = LittleFS.open(patternPath, "w");

  if (!file)
  {
    ESP_LOGW(logTag, "Failed to open %s for write", patternPath.c_str());
    return false;
  }

  JsonDocument jsonDocument;
  JsonArray tracks = jsonDocument["tracks"].to<JsonArray>();

  jsonDocument["version"] = 3;
  jsonDocument["name"] = normalizedName;
  jsonDocument["bpm"] = patternData.bpm;
  jsonDocument["swing"] = patternData.swingPercent;
  jsonDocument["chainEnabled"] = patternData.chainEnabled;
  jsonDocument["chainLength"] = patternData.chainLength;
  jsonDocument["masterLevel"] = 100;

  for (uint8_t trackIndex = 0; trackIndex < sequencerTrackCount; trackIndex++)
  {
    JsonObject trackObject = tracks.add<JsonObject>();
    JsonArray steps = trackObject["steps"].to<JsonArray>();

    trackObject["name"] = trackJsonNames[trackIndex];
    trackObject["machine"] = "sample";
    trackObject["sample"] = trackSampleNames[trackIndex];
    trackObject["mute"] = patternData.pattern.tracks[trackIndex].mute;
    trackObject["solo"] = false;
    trackObject["level"] = 100;
    trackObject["pan"] = 64;

    for (uint8_t stepIndex = 0; stepIndex < sequencerStepCount; stepIndex++)
    {
      JsonObject stepObject = steps.add<JsonObject>();
      const Step& step = patternData.pattern.tracks[trackIndex].steps[stepIndex];

      stepObject["trig"] = step.trigger;
      stepObject["velocity"] = step.velocity;
      stepObject["probability"] = step.probability;

      JsonObject locksObject = stepObject["locks"].to<JsonObject>();
      locksObject["enabled"] = step.lockEnabled;
      locksObject["pitch"] = step.lockPitch;
      locksObject["decay"] = step.lockDecay;
    }
  }

  bool success = (serializeJson(jsonDocument, file) > 0);

  file.close();

  if (!success)
  {
    ESP_LOGW(logTag, "Failed to serialize %s", patternPath.c_str());
  }

  return success;

} //   settingsStoreSavePattern()

//-- Load one pattern JSON file into runtime payload.
bool settingsStoreLoadPattern(const String& patternName, PatternData& patternData)
{
  String patternPath = buildPatternPath(patternName);

  if (!ensurePatternDirectory() || !LittleFS.exists(patternPath))
  {
    return false;
  }

  File file = LittleFS.open(patternPath, "r");

  if (!file)
  {
    ESP_LOGW(logTag, "Failed to open %s for read", patternPath.c_str());
    return false;
  }

  JsonDocument jsonDocument;
  DeserializationError error = deserializeJson(jsonDocument, file);

  file.close();

  if (error)
  {
    ESP_LOGW(logTag, "Invalid %s (%s)", patternPath.c_str(), error.c_str());
    return false;
  }

  patternData.bpm = static_cast<uint16_t>(jsonDocument["bpm"] | 120);
  patternData.swingPercent = static_cast<uint8_t>(jsonDocument["swing"] | 8);
  patternData.chainEnabled = static_cast<bool>(jsonDocument["chainEnabled"] | false);
  patternData.chainLength = static_cast<uint8_t>(jsonDocument["chainLength"] | 1);

  if (patternData.chainLength < 1)
  {
    patternData.chainLength = 1;
  }
  else if (patternData.chainLength > sequencerPatternCount)
  {
    patternData.chainLength = sequencerPatternCount;
  }

  if (patternData.chainLength <= 1)
  {
    patternData.chainEnabled = false;
  }

  JsonArray tracks = jsonDocument["tracks"].as<JsonArray>();

  if (tracks.isNull() || tracks.size() < sequencerTrackCount)
  {
    ESP_LOGW(logTag, "Missing tracks in %s", patternPath.c_str());
    return false;
  }

  for (uint8_t trackIndex = 0; trackIndex < sequencerTrackCount; trackIndex++)
  {
    JsonObject trackObject = tracks[trackIndex].as<JsonObject>();
    JsonArray steps = trackObject["steps"].as<JsonArray>();

    patternData.pattern.tracks[trackIndex].mute = static_cast<bool>(trackObject["mute"] | false);

    if (steps.isNull() || steps.size() < sequencerStepCount)
    {
      ESP_LOGW(logTag, "Missing steps in %s", patternPath.c_str());
      return false;
    }

    for (uint8_t stepIndex = 0; stepIndex < sequencerStepCount; stepIndex++)
    {
      JsonObject stepObject = steps[stepIndex].as<JsonObject>();
      Step& step = patternData.pattern.tracks[trackIndex].steps[stepIndex];

      step.trigger = static_cast<bool>(stepObject["trig"] | false);
      step.velocity = static_cast<uint8_t>(stepObject["velocity"] | 255);
      step.probability = static_cast<uint8_t>(stepObject["probability"] | 100);

      JsonObject locksObject = stepObject["locks"].as<JsonObject>();

      if (locksObject.isNull())
      {
        step.lockEnabled = false;
        step.lockPitch = 0;
        step.lockDecay = 100;
      }
      else
      {
        int lockPitchValue = static_cast<int>(locksObject["pitch"] | 0);
        int lockDecayValue = static_cast<int>(locksObject["decay"] | 100);

        if (lockPitchValue < -24)
        {
          lockPitchValue = -24;
        }
        else if (lockPitchValue > 24)
        {
          lockPitchValue = 24;
        }

        if (lockDecayValue < 10)
        {
          lockDecayValue = 10;
        }
        else if (lockDecayValue > 200)
        {
          lockDecayValue = 200;
        }

        step.lockEnabled = static_cast<bool>(locksObject["enabled"] | false);
        step.lockPitch = static_cast<int8_t>(lockPitchValue);
        step.lockDecay = static_cast<uint8_t>(lockDecayValue);
      }
    }
  }

  return true;

} //   settingsStoreLoadPattern()

//-- Remove one stored pattern file.
bool settingsStoreDeletePattern(const String& patternName)
{
  String patternPath = buildPatternPath(patternName);

  if (!ensurePatternDirectory() || !LittleFS.exists(patternPath))
  {
    return false;
  }

  return LittleFS.remove(patternPath);

} //   settingsStoreDeletePattern()
