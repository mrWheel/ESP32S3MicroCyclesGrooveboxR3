/*** Last Changed: 2026-06-10 - 16:50 ***/
/*** Last Changed: 2026-05-27 - 17:20 ***/

#include "settingsStore.h"
#include "appConfig.h"
#include <nvs.h>
#include <nvs_flash.h>
#include <Preferences.h>

//-- Helper: open NVS handle
static nvs_handle_t openNvsHandle(bool write = false)
{
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, write ? NVS_READWRITE : NVS_READONLY, &handle);
  if (err != ESP_OK)
    return 0;
  return handle;
}

//-- Get active pattern group name from NVS
String settingsStoreGetActivePatternGroup()
{
  nvs_handle_t handle = openNvsHandle();
  if (!handle)
    return "DEMO";
  char buf[16] = {0};
  size_t len = sizeof(buf);
  esp_err_t err = nvs_get_str(handle, "pattern_group", buf, &len);
  nvs_close(handle);
  if (err == ESP_OK && len > 1)
    return String(buf);
  return "DEMO";
}

//-- Set active pattern group name in NVS
bool settingsStoreSetActivePatternGroup(const String& groupName)
{
  nvs_handle_t handle = openNvsHandle(true);
  if (!handle)
    return false;
  esp_err_t err = nvs_set_str(handle, "pattern_group", groupName.c_str());
  nvs_commit(handle);
  nvs_close(handle);
  return err == ESP_OK;
}

//-- Get active sample set name from NVS
String settingsStoreGetActiveSampleSet()
{
  nvs_handle_t handle = openNvsHandle();
  if (!handle)
    return "S1";
  char buf[4] = {0};
  size_t len = sizeof(buf);
  esp_err_t err = nvs_get_str(handle, "sample_set", buf, &len);
  nvs_close(handle);
  if (err == ESP_OK && len > 1)
    return String(buf);
  return "S1";
}

//-- Set active sample set name in NVS
bool settingsStoreSetActiveSampleSet(const String& setName)
{
  nvs_handle_t handle = openNvsHandle(true);
  if (!handle)
    return false;
  esp_err_t err = nvs_set_str(handle, "sample_set", setName.c_str());
  nvs_commit(handle);
  nvs_close(handle);
  return err == ESP_OK;
}

//-- Get display rotation from NVS
uint8_t settingsStoreGetDisplayRotation()
{
  nvs_handle_t handle = openNvsHandle();
  if (!handle)
    return 0;
  uint8_t val = 0;
  esp_err_t err = nvs_get_u8(handle, "display_rot", &val);
  nvs_close(handle);
  return (err == ESP_OK) ? val : 0;
}

//-- Set display rotation in NVS
bool settingsStoreSetDisplayRotation(uint8_t rotation)
{
  nvs_handle_t handle = openNvsHandle(true);
  if (!handle)
    return false;
  esp_err_t err = nvs_set_u8(handle, "display_rot", rotation);
  nvs_commit(handle);
  nvs_close(handle);
  return err == ESP_OK;
}

//-- Get encoder order from NVS
bool settingsStoreGetEncoderOrder()
{
  nvs_handle_t handle = openNvsHandle();
  if (!handle)
    return false;
  uint8_t val = 0;
  esp_err_t err = nvs_get_u8(handle, "enc_order", &val);
  nvs_close(handle);
  return (err == ESP_OK) ? (val != 0) : false;
}

//-- Set encoder order in NVS
bool settingsStoreSetEncoderOrder(bool reversed)
{
  nvs_handle_t handle = openNvsHandle(true);
  if (!handle)
    return false;
  esp_err_t err = nvs_set_u8(handle, "enc_order", reversed ? 1 : 0);
  nvs_commit(handle);
  nvs_close(handle);
  return err == ESP_OK;
}

//-- Get theme color index from NVS
int settingsStoreGetThemeColorIndex()
{
  nvs_handle_t handle = openNvsHandle();
  if (!handle)
    return 0;
  int32_t val = 0;
  esp_err_t err = nvs_get_i32(handle, "theme_color", &val);
  nvs_close(handle);
  return (err == ESP_OK) ? val : 0;
}

//-- Set theme color index in NVS
bool settingsStoreSetThemeColorIndex(int colorIndex)
{
  nvs_handle_t handle = openNvsHandle(true);
  if (!handle)
    return false;
  esp_err_t err = nvs_set_i32(handle, "theme_color", colorIndex);
  nvs_commit(handle);
  nvs_close(handle);
  return err == ESP_OK;
}

//-- Get WiFi credentials from NVS
bool settingsStoreGetWifiCredentials(String& ssid, String& password)
{
  nvs_handle_t handle = openNvsHandle();
  if (!handle)
    return false;
  char ssidBuf[33] = {0};
  char passBuf[65] = {0};
  size_t ssidLen = sizeof(ssidBuf);
  size_t passLen = sizeof(passBuf);
  esp_err_t err1 = nvs_get_str(handle, "wifi_ssid", ssidBuf, &ssidLen);
  esp_err_t err2 = nvs_get_str(handle, "wifi_pass", passBuf, &passLen);
  nvs_close(handle);
  if (err1 == ESP_OK && err2 == ESP_OK)
  {
    ssid = String(ssidBuf);
    password = String(passBuf);
    return true;
  }
  return false;
}

//-- Set WiFi credentials in NVS
bool settingsStoreSetWifiCredentials(const String& ssid, const String& password)
{
  nvs_handle_t handle = openNvsHandle(true);
  if (!handle)
    return false;
  esp_err_t err1 = nvs_set_str(handle, "wifi_ssid", ssid.c_str());
  esp_err_t err2 = nvs_set_str(handle, "wifi_pass", password.c_str());
  nvs_commit(handle);
  nvs_close(handle);
  return (err1 == ESP_OK && err2 == ESP_OK);
}

//-- Get persisted master gain percentage.
uint8_t settingsStoreGetMasterGainPercent()
{
  Preferences preferences;
  uint8_t gainPercent = 100;

  if (!preferences.begin("audio", true))
  {
    return gainPercent;
  }

  gainPercent = preferences.getUChar("masterGain", 100);
  preferences.end();

  if (gainPercent < 10)
  {
    gainPercent = 10;
  }
  else if (gainPercent > 200)
  {
    gainPercent = 200;
  }

  return gainPercent;

} //   settingsStoreGetMasterGainPercent()

//-- Persist master gain percentage.
bool settingsStoreSetMasterGainPercent(uint8_t gainPercent)
{
  Preferences preferences;
  size_t writtenBytes = 0;

  if (gainPercent < 10)
  {
    gainPercent = 10;
  }
  else if (gainPercent > 200)
  {
    gainPercent = 200;
  }

  if (!preferences.begin("audio", false))
  {
    return false;
  }

  writtenBytes = preferences.putUChar("masterGain", gainPercent);
  preferences.end();

  return writtenBytes == sizeof(uint8_t);

} //   settingsStoreSetMasterGainPercent()

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <SD.h>
#include <esp_log.h>

//-- Logging tag.
static const char* logTag = "SettingsStore";

//-- Settings and pattern paths.
static const char* settingsPath = "/settings.json";
static const char* patternTempFileSuffix = ".tmp";
static const size_t patternSaveReserveBytes = 512;
static const char* trackJsonNames[sequencerTrackCount] = {"KICK", "SNARE", "CH",
                                                          "OH",   "TONE",  "METAL"};
static const char* trackSampleNames[sequencerTrackCount] = {"kick", "snare", "ch",
                                                            "oh",   "tone",  "metal"};

//-- SD card pattern directory.
static const char* sdPatternDirectoryPath = "/patterns";

//-- Ensure LittleFS is mounted for settings access.
static bool ensureSettingsFsMounted();

//-- Validate mounted LittleFS before any write/read operations.
static bool isSettingsFsUsable();

//-- Validate strict pattern naming format: <LETTER><DIGIT><DIGIT>.
static bool isPatternNameLetterNumberFormat(const String& patternName);

//-- Normalize one letter token to uppercase A..Z.
static char normalizePatternLetter(char patternLetter);

//-- Build one pattern JSON document without changing schema.
static void buildPatternJsonDocument(const String& normalizedName, const PatternData& patternData,
                                     JsonDocument& jsonDocument);

//-- Parse one pattern JSON document into runtime payload.
static bool parsePatternJsonDocument(const JsonDocument& jsonDocument, PatternData& patternData,
                                     const String& sourcePath);

//-- Read chain settings from JSON document with backward-compatible keys.
static void readChainSettingsFromJson(const JsonDocument& jsonDocument, bool& outEnabled,
                                      uint8_t& outLength, String& outTarget);

//-- Build safe pattern base name.
static String normalizePatternName(const String& patternName)
{
  String normalizedName = "";

  for (size_t charIndex = 0; charIndex < patternName.length(); charIndex++)
  {
    char currentChar = patternName[charIndex];
    bool allowedChar =
        ((currentChar >= 'a' && currentChar <= 'z') || (currentChar >= 'A' && currentChar <= 'Z') ||
         (currentChar >= '0' && currentChar <= '9') || currentChar == '_' || currentChar == '-');

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

//-- Normalize one pNN pattern token.
static String normalizePatternSlotName(const String& patternName)
{
  String normalizedName = patternName;

  if (normalizedName.endsWith(".json"))
  {
    normalizedName = normalizedName.substring(0, normalizedName.length() - 5);
  }

  if (normalizedName.length() != 3)
  {
    return "";
  }

  if (normalizedName[0] != 'p' && normalizedName[0] != 'P')
  {
    return "";
  }

  if (!isDigit(normalizedName[1]) || !isDigit(normalizedName[2]))
  {
    return "";
  }

  if (normalizedName == "p00" || normalizedName == "P00")
  {
    return "";
  }

  normalizedName[0] = 'p';

  return normalizedName;

} //   normalizePatternSlotName()

//-- Extract pattern name from absolute file path (expects /patterns/pNN)
static String patternNameFromPath(const String& fullPath)
{
  int slashIndex = fullPath.lastIndexOf('/');
  String fileName = (slashIndex >= 0) ? fullPath.substring(slashIndex + 1) : fullPath;
  // Accept pNN only
  if (fileName.length() == 3 && fileName[0] == 'p' && isDigit(fileName[1]) && isDigit(fileName[2]))
  {
    return fileName;
  }
  return "";
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
  settings.activePatternName = "";

  return settings;

} //   runtimeSettings()

//-- Ensure LittleFS is mounted for settings access.
static bool ensureSettingsFsMounted()
{
  static bool mounted = false;

  if (mounted)
  {
    if (isSettingsFsUsable())
    {
      return true;
    }

    ESP_LOGW(logTag, "LittleFS mounted but unusable; remounting");
    LittleFS.end();
    mounted = false;
  }

  if (!LittleFS.begin(true, "/littlefs", 10, "littlefs"))
  {
    ESP_LOGW(logTag, "LittleFS mount failed; using defaults");
    return false;
  }

  if (!isSettingsFsUsable())
  {
    ESP_LOGW(logTag, "LittleFS mounted with invalid capacity; using defaults");
    LittleFS.end();
    return false;
  }

  mounted = true;
  return true;

} //   ensureSettingsFsMounted()

//-- Validate runtime LittleFS capacity reported by driver.
static bool isSettingsFsUsable()
{
  size_t totalBytes = LittleFS.totalBytes();

  if (totalBytes == 0)
  {
    return false;
  }

  return true;

} //   isSettingsFsUsable()

//-- Validate strict pattern naming format: <LETTER><DIGIT><DIGIT>.
static bool isPatternNameLetterNumberFormat(const String& patternName)
{
  char nameLetter;

  if (patternName.length() != 3)
  {
    return false;
  }

  nameLetter = normalizePatternLetter(patternName[0]);

  if (nameLetter < 'A' || nameLetter > 'Z')
  {
    return false;
  }

  if (patternName[1] < '0' || patternName[1] > '9' || patternName[2] < '0' || patternName[2] > '9')
  {
    return false;
  }

  return true;

} //   isPatternNameLetterNumberFormat()

//-- Normalize one letter token to uppercase A..Z.
static char normalizePatternLetter(char patternLetter)
{
  if (patternLetter >= 'a' && patternLetter <= 'z')
  {
    return static_cast<char>(patternLetter - ('a' - 'A'));
  }

  return patternLetter;

} //   normalizePatternLetter()

//-- Build one pattern JSON document without changing schema.
static void buildPatternJsonDocument(const String& normalizedName, const PatternData& patternData,
                                     JsonDocument& jsonDocument)
{
  JsonArray tracks = jsonDocument["tracks"].to<JsonArray>();

  jsonDocument["version"] = 3;
  jsonDocument["name"] = normalizedName;
  jsonDocument["bpm"] = patternData.bpm;
  jsonDocument["swing"] = patternData.swingPercent;
  jsonDocument["chainEnabled"] = patternData.chainEnabled;
  jsonDocument["chainLength"] = patternData.chainLength;
  jsonDocument["chainTarget"] = normalizePatternSlotName(patternData.chainTarget);

  JsonObject chainObject = jsonDocument["chain"].to<JsonObject>();
  chainObject["enabled"] = patternData.chainEnabled;
  chainObject["length"] = patternData.chainLength;
  chainObject["target"] = normalizePatternSlotName(patternData.chainTarget);
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

} //   buildPatternJsonDocument()

//-- Parse one pattern JSON document into runtime payload.
static bool parsePatternJsonDocument(const JsonDocument& jsonDocument, PatternData& patternData,
                                     const String& sourcePath)
{
  JsonArrayConst tracks;

  patternData.bpm = static_cast<uint16_t>(jsonDocument["bpm"] | 120);
  patternData.swingPercent = static_cast<uint8_t>(jsonDocument["swing"] | 8);

  readChainSettingsFromJson(jsonDocument, patternData.chainEnabled, patternData.chainLength,
                            patternData.chainTarget);

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

  tracks = jsonDocument["tracks"].as<JsonArrayConst>();

  if (tracks.isNull() || tracks.size() < sequencerTrackCount)
  {
    ESP_LOGW(logTag, "Missing tracks in %s", sourcePath.c_str());
    return false;
  }

  for (uint8_t trackIndex = 0; trackIndex < sequencerTrackCount; trackIndex++)
  {
    JsonObjectConst trackObject = tracks[trackIndex].as<JsonObjectConst>();
    JsonArrayConst steps = trackObject["steps"].as<JsonArrayConst>();

    patternData.pattern.tracks[trackIndex].mute = static_cast<bool>(trackObject["mute"] | false);

    if (steps.isNull() || steps.size() < sequencerStepCount)
    {
      ESP_LOGW(logTag, "Missing steps in %s", sourcePath.c_str());
      return false;
    }

    for (uint8_t stepIndex = 0; stepIndex < sequencerStepCount; stepIndex++)
    {
      JsonObjectConst stepObject = steps[stepIndex].as<JsonObjectConst>();
      Step& step = patternData.pattern.tracks[trackIndex].steps[stepIndex];

      //-- Accept both old/new trigger field names.
      if (stepObject["trig"].is<bool>())
      {
        step.trigger = static_cast<bool>(stepObject["trig"]);
      }
      else
      {
        step.trigger = static_cast<bool>(stepObject["trigger"] | false);
      }

      step.velocity = static_cast<uint8_t>(stepObject["velocity"] | (step.trigger ? 255 : 0));
      step.probability = static_cast<uint8_t>(stepObject["probability"] | 100);

      JsonObjectConst locksObject = stepObject["locks"].as<JsonObjectConst>();

      if (locksObject.isNull())
      {
        //-- Accept flat lock fields from newer saved patterns.
        int lockPitchValue = static_cast<int>(stepObject["lockPitch"] | 0);
        int lockDecayValue = static_cast<int>(stepObject["lockDecay"] | 100);

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

        step.lockEnabled = static_cast<bool>(stepObject["lockEnabled"] | false);
        step.lockPitch = static_cast<int8_t>(lockPitchValue);
        step.lockDecay = static_cast<uint8_t>(lockDecayValue);
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

} //   parsePatternJsonDocument()

//-- Read chain settings from JSON document with backward-compatible keys.
static void readChainSettingsFromJson(const JsonDocument& jsonDocument, bool& outEnabled,
                                      uint8_t& outLength, String& outTarget)
{
  JsonObjectConst chainObject = jsonDocument["chain"].as<JsonObjectConst>();
  bool chainEnabled = false;
  uint8_t chainLength = 1;
  String chainTarget = "";

  if (!chainObject.isNull())
  {
    chainEnabled = static_cast<bool>(chainObject["enabled"] | false);
    chainLength = static_cast<uint8_t>(chainObject["length"] | 1);
    chainTarget = String(static_cast<const char*>(chainObject["target"] | ""));
  }
  else
  {
    chainEnabled = static_cast<bool>(jsonDocument["chainEnabled"] | false);
    chainLength = static_cast<uint8_t>(jsonDocument["chainLength"] | 1);
    chainTarget = String(static_cast<const char*>(jsonDocument["chainTarget"] | ""));
  }

  if (chainLength < 1)
  {
    chainLength = 1;
  }
  else if (chainLength > sequencerPatternCount)
  {
    chainLength = sequencerPatternCount;
  }

  outEnabled = chainEnabled;
  outLength = chainLength;
  outTarget = normalizePatternSlotName(chainTarget);

} //   readChainSettingsFromJson()

//-- Report LittleFS usage without creating legacy pattern directories.
bool settingsStoreGetLittleFsUsage(size_t& outUsedBytes, size_t& outTotalBytes,
                                   size_t& outFreeBytes)
{
  if (!ensureSettingsFsMounted())
  {
    return false;
  }

  outTotalBytes = LittleFS.totalBytes();
  outUsedBytes = LittleFS.usedBytes();

  if (outTotalBytes >= outUsedBytes)
  {
    outFreeBytes = outTotalBytes - outUsedBytes;
  }
  else
  {
    outFreeBytes = 0;
  }

  return true;

} //   settingsStoreGetLittleFsUsage()

//-- Return SD usage values in bytes.
bool settingsStoreGetSdUsage(size_t& outTotalBytes, size_t& outUsedBytes, size_t& outFreeBytes)
{
  if (SD.cardType() == CARD_NONE)
  {
    return false;
  }

  outTotalBytes = SD.totalBytes();
  outUsedBytes = SD.usedBytes();

  if (outUsedBytes > outTotalBytes)
  {
    outUsedBytes = outTotalBytes;
  }

  outFreeBytes = outTotalBytes - outUsedBytes;

  return true;

} //   settingsStoreGetSdUsage()

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

  settings.displayRotation =
      static_cast<uint8_t>(jsonDocument["displayRotation"] | defaults.displayRotation);
  settings.themeColorIndex =
      static_cast<int>(jsonDocument["themeColorIndex"] | defaults.themeColorIndex);
  settings.encoderDirectionReversed = static_cast<bool>(jsonDocument["encoderDirectionReversed"] |
                                                        defaults.encoderDirectionReversed);
  settings.activePatternName = static_cast<const char*>(jsonDocument["activePatternName"] |
                                                        defaults.activePatternName.c_str());

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
  jsonDocument["activePatternName"] = settings.activePatternName;

  bool success = (serializeJson(jsonDocument, file) > 0);

  file.close();

  if (!success)
  {
    ESP_LOGW(logTag, "Failed to serialize %s", settingsPath);
  }

  return success;

} //   settingsStoreSaveRuntimeSettings()

//-- Find next available SD pattern name for one letter group.
bool settingsStoreFindNextPatternNameForLetterOnCard(char patternLetter, String& outName)
{
  char normalizedLetter;
  bool usedNumbers[100] = {false};

  normalizedLetter = normalizePatternLetter(patternLetter);

  if (normalizedLetter < 'A' || normalizedLetter > 'Z' || SD.cardType() == CARD_NONE)
  {
    return false;
  }

  File directory = SD.open(sdPatternDirectoryPath, FILE_READ);

  if (directory && directory.isDirectory())
  {
    File entry = directory.openNextFile();

    while (entry)
    {
      if (!entry.isDirectory())
      {
        String patternName = patternNameFromPath(entry.name());

        if (isPatternNameLetterNumberFormat(patternName))
        {
          char entryLetter = normalizePatternLetter(patternName[0]);

          if (entryLetter == normalizedLetter)
          {
            int numberValue = (patternName[1] - '0') * 10 + (patternName[2] - '0');

            if (numberValue >= 1 && numberValue <= 99)
            {
              usedNumbers[numberValue] = true;
            }
          }
        }
      }

      entry.close();
      entry = directory.openNextFile();
    }
  }

  directory.close();

  for (int patternIndex = 1; patternIndex <= 99; patternIndex++)
  {
    if (!usedNumbers[patternIndex])
    {
      char candidateName[8];

      snprintf(candidateName, sizeof(candidateName), "%c%02d", normalizedLetter, patternIndex);

      outName = String(candidateName);
      return true;
    }
  }

  return false;

} //   settingsStoreFindNextPatternNameForLetterOnCard()

//-- Count remaining free SD pattern slots for one letter group (0..99).
bool settingsStoreCountAvailablePatternSlotsForLetterOnCard(char patternLetter, int& outFreeCount)
{
  char normalizedLetter;
  int usedCount = 0;

  outFreeCount = 0;

  normalizedLetter = normalizePatternLetter(patternLetter);

  if (normalizedLetter < 'A' || normalizedLetter > 'Z' || SD.cardType() == CARD_NONE)
  {
    return false;
  }

  File directory = SD.open(sdPatternDirectoryPath, FILE_READ);

  if (directory && directory.isDirectory())
  {
    File entry = directory.openNextFile();

    while (entry)
    {
      if (!entry.isDirectory())
      {
        String patternName = patternNameFromPath(entry.name());

        if (isPatternNameLetterNumberFormat(patternName))
        {
          char entryLetter = normalizePatternLetter(patternName[0]);

          if (entryLetter == normalizedLetter)
          {
            usedCount++;
          }
        }
      }

      entry.close();
      entry = directory.openNextFile();
    }
  }

  directory.close();

  if (usedCount < 0)
  {
    usedCount = 0;
  }
  else if (usedCount > 99)
  {
    usedCount = 99;
  }

  outFreeCount = 99 - usedCount;
  return true;

} //   settingsStoreCountAvailablePatternSlotsForLetterOnCard()

//-- Validate one pattern group name.
static bool isValidPatternGroupName(const String& groupName)
{
  if (groupName.isEmpty() || groupName.length() > 8)
  {
    return false;
  }

  for (size_t charIndex = 0; charIndex < groupName.length(); charIndex++)
  {
    char currentChar = groupName[charIndex];

    if (!((currentChar >= 'A' && currentChar <= 'Z') || (currentChar >= '0' && currentChar <= '9')))
    {
      return false;
    }
  }

  return true;

} //   isValidPatternGroupName()

//-- Save pattern payload to one JSON file on SD card in /patterns/<groupName>/pNN.json.
bool settingsStoreSavePatternToCard(const String& groupName, const String& patternName,
                                    const PatternData& patternData)
{
  String normalizedName = normalizePatternSlotName(patternName);
  String groupDir;
  String patternPath;
  String legacyPatternPath;
  String tempPatternPath;
  JsonDocument jsonDocument;

  if (SD.cardType() == CARD_NONE)
  {
    ESP_LOGW(logTag, "SD card not available for pattern save");
    return false;
  }

  if (!isValidPatternGroupName(groupName))
  {
    ESP_LOGW(logTag, "Invalid pattern group name for save: %s", groupName.c_str());
    return false;
  }

  if (normalizedName.isEmpty())
  {
    ESP_LOGW(logTag, "Invalid pattern name for save: %s", patternName.c_str());
    return false;
  }

  groupDir = String(sdPatternDirectoryPath) + "/" + groupName;

  if (!SD.exists(groupDir))
  {
    if (!SD.mkdir(groupDir))
    {
      ESP_LOGW(logTag, "Failed to create SD pattern group directory %s", groupDir.c_str());
      return false;
    }
  }

  patternPath = groupDir + "/" + normalizedName + ".json";
  legacyPatternPath = groupDir + "/" + normalizedName;
  tempPatternPath = patternPath + patternTempFileSuffix;

  buildPatternJsonDocument(normalizedName, patternData, jsonDocument);

  if (SD.exists(tempPatternPath))
  {
    SD.remove(tempPatternPath);
  }

  File file = SD.open(tempPatternPath, FILE_WRITE);

  if (!file)
  {
    ESP_LOGW(logTag, "Failed to open SD %s for write", tempPatternPath.c_str());
    return false;
  }

  bool success = (serializeJsonPretty(jsonDocument, file) > 0);

  file.close();

  if (!success)
  {
    ESP_LOGW(logTag, "Failed to serialize SD %s", tempPatternPath.c_str());
    SD.remove(tempPatternPath);
    return false;
  }

  if (SD.exists(patternPath))
  {
    SD.remove(patternPath);
  }

  if (!SD.rename(tempPatternPath, patternPath))
  {
    ESP_LOGW(logTag, "Failed to rename SD %s to %s", tempPatternPath.c_str(), patternPath.c_str());

    SD.remove(tempPatternPath);
    return false;
  }

  //-- Remove legacy extensionless file after successful .json save.
  if (SD.exists(legacyPatternPath))
  {
    SD.remove(legacyPatternPath);
  }

  return true;

} //   settingsStoreSavePatternToCard()

//-- List available pattern group names on SD card (subdirectories of /patterns)
bool settingsStoreListPatternGroupsOnCard(String groupNames[], size_t maxCount, size_t& outCount)
{
  outCount = 0;
  if (groupNames == nullptr || maxCount == 0)
  {
    return false;
  }
  if (SD.cardType() == CARD_NONE)
  {
    return false;
  }
  File directory = SD.open(sdPatternDirectoryPath, FILE_READ);
  if (!directory || !directory.isDirectory())
  {
    directory.close();
    return false;
  }
  File entry = directory.openNextFile();
  while (entry && outCount < maxCount)
  {
    if (entry.isDirectory())
    {
      String groupName = String(entry.name());
      int slashIdx = groupName.lastIndexOf('/');
      if (slashIdx >= 0)
        groupName = groupName.substring(slashIdx + 1);
      // Validate group name: max 8 chars, A-Z/0-9 only
      if (groupName.length() <= 8 && groupName.length() > 0)
      {
        bool valid = true;
        for (size_t i = 0; i < groupName.length(); i++)
        {
          char c = groupName[i];
          if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
          {
            valid = false;
            break;
          }
        }
        if (valid)
        {
          groupNames[outCount] = groupName;
          outCount++;
        }
      }
    }
    entry.close();
    entry = directory.openNextFile();
  }
  directory.close();
  return true;
} //   settingsStoreListPatternGroupsOnCard()

//-- Rename one Card pattern group directory.
bool settingsStoreRenamePatternGroupOnCard(const String& oldGroupName, const String& newGroupName)
{
  String oldPath;
  String newPath;

  if (SD.cardType() == CARD_NONE)
  {
    ESP_LOGW(logTag, "SD card not available for pattern group rename");
    return false;
  }

  if (!isValidPatternGroupName(oldGroupName) || !isValidPatternGroupName(newGroupName))
  {
    ESP_LOGW(logTag, "Invalid pattern group rename: %s -> %s", oldGroupName.c_str(),
             newGroupName.c_str());

    return false;
  }

  oldPath = String(sdPatternDirectoryPath) + "/" + oldGroupName;
  newPath = String(sdPatternDirectoryPath) + "/" + newGroupName;

  if (!SD.exists(oldPath))
  {
    ESP_LOGW(logTag, "Source pattern group does not exist: %s", oldPath.c_str());
    return false;
  }

  if (SD.exists(newPath))
  {
    ESP_LOGW(logTag, "Target pattern group already exists: %s", newPath.c_str());
    return false;
  }

  if (!SD.rename(oldPath, newPath))
  {
    ESP_LOGW(logTag, "Failed to rename pattern group %s to %s", oldPath.c_str(), newPath.c_str());

    return false;
  }

  ESP_LOGI(logTag, "Renamed pattern group %s to %s", oldGroupName.c_str(), newGroupName.c_str());

  return true;

} //   settingsStoreRenamePatternGroupOnCard()

//-- Copy one Card pattern group directory.
bool settingsStoreCopyPatternGroupOnCard(const String& sourceGroupName,
                                         const String& targetGroupName)
{
  String sourcePath;
  String targetPath;

  if (SD.cardType() == CARD_NONE)
  {
    ESP_LOGW(logTag, "SD card not available for pattern group copy");
    return false;
  }

  if (!isValidPatternGroupName(sourceGroupName) || !isValidPatternGroupName(targetGroupName))
  {
    ESP_LOGW(logTag, "Invalid pattern group copy: %s -> %s", sourceGroupName.c_str(),
             targetGroupName.c_str());

    return false;
  }

  sourcePath = String(sdPatternDirectoryPath) + "/" + sourceGroupName;
  targetPath = String(sdPatternDirectoryPath) + "/" + targetGroupName;

  if (!SD.exists(sourcePath))
  {
    ESP_LOGW(logTag, "Source pattern group does not exist: %s", sourcePath.c_str());
    return false;
  }

  if (SD.exists(targetPath))
  {
    ESP_LOGW(logTag, "Target pattern group already exists: %s", targetPath.c_str());
    return false;
  }

  if (!SD.mkdir(targetPath))
  {
    ESP_LOGW(logTag, "Failed to create target pattern group: %s", targetPath.c_str());
    return false;
  }

  File sourceDirectory = SD.open(sourcePath, FILE_READ);

  if (!sourceDirectory || !sourceDirectory.isDirectory())
  {
    if (sourceDirectory)
    {
      sourceDirectory.close();
    }

    ESP_LOGW(logTag, "Failed to open source pattern group: %s", sourcePath.c_str());
    SD.rmdir(targetPath);

    return false;
  }

  File entry = sourceDirectory.openNextFile();

  while (entry)
  {
    if (!entry.isDirectory())
    {
      String sourceFilePath = String(entry.name());
      String fileName = sourceFilePath;
      int slashIndex = fileName.lastIndexOf('/');

      if (slashIndex >= 0)
      {
        fileName = fileName.substring(slashIndex + 1);
      }

      String targetFilePath = targetPath + "/" + fileName;
      File targetFile = SD.open(targetFilePath, FILE_WRITE);

      if (!targetFile)
      {
        ESP_LOGW(logTag, "Failed to create copied pattern file: %s", targetFilePath.c_str());
        entry.close();
        sourceDirectory.close();

        return false;
      }

      uint8_t buffer[256];

      while (entry.available())
      {
        size_t bytesRead = entry.read(buffer, sizeof(buffer));

        if (bytesRead > 0)
        {
          targetFile.write(buffer, bytesRead);
        }
      }

      targetFile.close();
    }

    entry.close();
    entry = sourceDirectory.openNextFile();
  }

  sourceDirectory.close();

  ESP_LOGI(logTag, "Copied pattern group %s to %s", sourceGroupName.c_str(),
           targetGroupName.c_str());

  return true;

} //   settingsStoreCopyPatternGroupOnCard()

//-- List available pattern names in a group on SD card: /patterns/<groupName>/pNN.
bool settingsStoreListPatternsInGroupOnCard(const String& groupName, String patternNames[],
                                            size_t maxCount, size_t& outCount)
{
  outCount = 0;

  if (patternNames == nullptr || maxCount == 0)
  {
    return false;
  }

  if (SD.cardType() == CARD_NONE)
  {
    ESP_LOGW(logTag, "SD card not available while listing group %s", groupName.c_str());
    return false;
  }

  String groupDirectoryPath = String(sdPatternDirectoryPath) + "/" + groupName;
  File directory = SD.open(groupDirectoryPath, FILE_READ);

  if (!directory || !directory.isDirectory())
  {
    if (directory)
    {
      directory.close();
    }

    ESP_LOGW(logTag, "Card pattern group not found: %s", groupDirectoryPath.c_str());
    return false;
  }

  File entry = directory.openNextFile();

  while (entry && outCount < maxCount)
  {
    if (!entry.isDirectory())
    {
      String fileName = String(entry.name());
      int slashIndex = fileName.lastIndexOf('/');

      if (slashIndex >= 0)
      {
        fileName = fileName.substring(slashIndex + 1);
      }

      if (fileName.endsWith(".json"))
      {
        fileName = fileName.substring(0, fileName.length() - 5);
      }

      if (fileName.length() == 3 && fileName[0] == 'p' && isDigit(fileName[1]) &&
          isDigit(fileName[2]) && fileName != "p00")
      {
        patternNames[outCount] = fileName;
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
      int leftNumber = patternNames[leftIndex].substring(1).toInt();
      int rightNumber = patternNames[rightIndex].substring(1).toInt();

      if (rightNumber < leftNumber)
      {
        String temporaryName = patternNames[leftIndex];

        patternNames[leftIndex] = patternNames[rightIndex];
        patternNames[rightIndex] = temporaryName;
      }
    }
  }

  ESP_LOGI(logTag, "Listed %u Card patterns in group %s", static_cast<unsigned>(outCount),
           groupName.c_str());

  return true;

} //   settingsStoreListPatternsInGroupOnCard()

//-- Load pattern payload from SD card group.
bool settingsStoreLoadPatternFromCard(const String& groupName, const String& patternName,
                                      PatternData& patternData)
{
  String normalizedName = normalizePatternName(patternName);
  String patternPath = String(sdPatternDirectoryPath) + "/" + groupName + "/" + normalizedName;
  String jsonPatternPath = patternPath + ".json";
  String pathToLoad = "";

  if (SD.cardType() == CARD_NONE)
  {
    ESP_LOGW(logTag, "SD card not available while loading %s/%s", groupName.c_str(),
             normalizedName.c_str());
    return false;
  }

  if (SD.exists(patternPath))
  {
    pathToLoad = patternPath;
  }
  else if (SD.exists(jsonPatternPath))
  {
    pathToLoad = jsonPatternPath;
  }
  else
  {
    ESP_LOGW(logTag, "Card pattern file not found: %s or %s", patternPath.c_str(),
             jsonPatternPath.c_str());

    return false;
  }

  File file = SD.open(pathToLoad, FILE_READ);

  if (!file)
  {
    ESP_LOGW(logTag, "Failed to open Card pattern %s for read", pathToLoad.c_str());
    return false;
  }

  if (file.size() == 0)
  {
    file.close();

    ESP_LOGW(logTag, "Empty Card pattern file %s", pathToLoad.c_str());
    return false;
  }

  JsonDocument jsonDocument;
  DeserializationError error = deserializeJson(jsonDocument, file);

  file.close();

  if (error)
  {
    ESP_LOGW(logTag, "Invalid Card pattern %s (%s)", pathToLoad.c_str(), error.c_str());

    return false;
  }

  if (!parsePatternJsonDocument(jsonDocument, patternData, pathToLoad))
  {
    ESP_LOGW(logTag, "Failed to parse Card pattern %s", pathToLoad.c_str());
    return false;
  }

  ESP_LOGI(logTag, "Loaded Card pattern %s", pathToLoad.c_str());

  return true;

} //   settingsStoreLoadPatternFromCard()

//-- Delete one pattern from SD card group.
bool settingsStoreDeletePatternFromCard(const String& groupName, const String& patternName)
{
  String normalizedName = normalizePatternSlotName(patternName);
  String jsonPatternPath;
  String legacyPatternPath;
  bool removedAny = false;

  if (SD.cardType() == CARD_NONE)
  {
    return false;
  }

  if (!isValidPatternGroupName(groupName))
  {
    return false;
  }

  if (normalizedName.isEmpty())
  {
    return false;
  }

  jsonPatternPath =
      String(sdPatternDirectoryPath) + "/" + groupName + "/" + normalizedName + ".json";
  legacyPatternPath = String(sdPatternDirectoryPath) + "/" + groupName + "/" + normalizedName;

  if (SD.exists(jsonPatternPath))
  {
    removedAny = SD.remove(jsonPatternPath) || removedAny;
  }

  if (SD.exists(legacyPatternPath))
  {
    removedAny = SD.remove(legacyPatternPath) || removedAny;
  }

  return removedAny;

} //   settingsStoreDeletePatternFromCard()
