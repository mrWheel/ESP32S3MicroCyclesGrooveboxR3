/*** Last Changed: 2026-05-27 - 17:20 ***/
#include "settingsStore.h"
#include "appConfig.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <SD.h>
#include <esp_log.h>

//-- Logging tag.
static const char* logTag = "SettingsStore";

//-- Settings and pattern paths.
static const char* settingsPath = "/settings.json";
static const char* patternDirectoryPath = "/patterns";
static const char* patternFileExtension = ".json";
static const char* patternTempFileSuffix = ".tmp";
static const size_t patternSaveReserveBytes = 512;
static const char* trackJsonNames[sequencerTrackCount] = {"KICK", "SNARE", "CH", "OH", "TONE", "METAL"};
static const char* trackSampleNames[sequencerTrackCount] = {"kick", "snare", "ch", "oh", "tone", "metal"};

//-- SD card pattern directory.
static const char* sdPatternDirectoryPath = "/patterns";

//-- Ensure LittleFS is mounted for settings access.
static bool ensureSettingsFsMounted();

//-- Validate mounted LittleFS before any write/read operations.
static bool isSettingsFsUsable();

//-- Check whether LittleFS has enough free space for a safe pattern save.
static bool hasEnoughSpaceForPatternSave(size_t requiredBytes);

//-- Validate strict pattern naming format: <LETTER><DIGIT><DIGIT>.
static bool isPatternNameLetterNumberFormat(const String& patternName);

//-- Normalize one letter token to uppercase A..Z.
static char normalizePatternLetter(char patternLetter);

//-- Check whether a path exists inside /patterns without noisy VFS errors.
static bool patternPathExists(const String& fullPath);

//-- Check whether a path exists inside /patterns on SD card.
static bool sdPatternPathExists(const String& fullPath);

//-- Build one pattern JSON document without changing schema.
static void buildPatternJsonDocument(const String& normalizedName, const PatternData& patternData, JsonDocument& jsonDocument);

//-- Parse one pattern JSON document into runtime payload.
static bool parsePatternJsonDocument(const JsonDocument& jsonDocument, PatternData& patternData, const String& sourcePath);

//-- Normalize strict pattern token to uppercase <LETTER><DIGIT><DIGIT>.
static String normalizeStrictPatternToken(const String& patternName);

//-- Read chain settings from JSON document with backward-compatible keys.
static void readChainSettingsFromJson(const JsonDocument& jsonDocument, bool& outEnabled, uint8_t& outLength, String& outTarget);

//-- Write chain settings into JSON document while preserving unrelated fields.
static void writeChainSettingsToJson(JsonDocument& jsonDocument, bool enabled, uint8_t length, const String& target);

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
  settings.activePatternName = "";

  return settings;

} //   defaultRuntimeSettings()

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

//-- Require requested bytes plus reserve margin to avoid allocator edge-cases.
static bool hasEnoughSpaceForPatternSave(size_t requiredBytes)
{
  size_t totalBytes;
  size_t usedBytes;
  size_t freeBytes;

  totalBytes = LittleFS.totalBytes();
  usedBytes = LittleFS.usedBytes();

  if (totalBytes == 0 || usedBytes > totalBytes)
  {
    return true;
  }

  freeBytes = totalBytes - usedBytes;

  return freeBytes > (requiredBytes + patternSaveReserveBytes);

} //   hasEnoughSpaceForPatternSave()

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

//-- Check whether a path exists inside /patterns without noisy VFS errors.
static bool patternPathExists(const String& fullPath)
{
  File directory;
  File entry;
  String fileNameToFind;

  int slashIndex = fullPath.lastIndexOf('/');
  fileNameToFind = (slashIndex >= 0) ? fullPath.substring(slashIndex + 1) : fullPath;

  if (!ensurePatternDirectory())
  {
    return false;
  }

  directory = LittleFS.open(patternDirectoryPath, "r");

  if (!directory)
  {
    return false;
  }

  entry = directory.openNextFile();

  while (entry)
  {
    String entryPath = String(entry.name());
    int entrySlashIndex = entryPath.lastIndexOf('/');
    String entryFileName = (entrySlashIndex >= 0) ? entryPath.substring(entrySlashIndex + 1) : entryPath;

    entry.close();

    if (entryPath == fullPath || entryPath == (String(patternDirectoryPath) + "/" + fileNameToFind) || entryFileName == fileNameToFind)
    {
      directory.close();
      return true;
    }

    entry = directory.openNextFile();
  }

  directory.close();
  return false;

} //   patternPathExists()

//-- Check whether a path exists inside /patterns on SD card.
static bool sdPatternPathExists(const String& fullPath)
{
  File directory;
  File entry;
  String fileNameToFind;

  int slashIndex = fullPath.lastIndexOf('/');
  fileNameToFind = (slashIndex >= 0) ? fullPath.substring(slashIndex + 1) : fullPath;

  directory = SD.open(sdPatternDirectoryPath, FILE_READ);

  if (!directory || !directory.isDirectory())
  {
    directory.close();
    return false;
  }

  entry = directory.openNextFile();

  while (entry)
  {
    String entryPath = String(entry.name());
    int entrySlashIndex = entryPath.lastIndexOf('/');
    String entryFileName = (entrySlashIndex >= 0) ? entryPath.substring(entrySlashIndex + 1) : entryPath;

    entry.close();

    if (entryPath == fullPath || entryPath == (String(sdPatternDirectoryPath) + "/" + fileNameToFind) || entryFileName == fileNameToFind)
    {
      directory.close();
      return true;
    }

    entry = directory.openNextFile();
  }

  directory.close();
  return false;

} //   sdPatternPathExists()

//-- Build one pattern JSON document without changing schema.
static void buildPatternJsonDocument(const String& normalizedName, const PatternData& patternData, JsonDocument& jsonDocument)
{
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

} //   buildPatternJsonDocument()

//-- Parse one pattern JSON document into runtime payload.
static bool parsePatternJsonDocument(const JsonDocument& jsonDocument, PatternData& patternData, const String& sourcePath)
{
  JsonArrayConst tracks;

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

      step.trigger = static_cast<bool>(stepObject["trig"] | false);
      step.velocity = static_cast<uint8_t>(stepObject["velocity"] | 255);
      step.probability = static_cast<uint8_t>(stepObject["probability"] | 100);

      JsonObjectConst locksObject = stepObject["locks"].as<JsonObjectConst>();

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

} //   parsePatternJsonDocument()

//-- Normalize strict pattern token to uppercase <LETTER><DIGIT><DIGIT>.
static String normalizeStrictPatternToken(const String& patternName)
{
  char normalized[4];

  if (!isPatternNameLetterNumberFormat(patternName))
  {
    return "";
  }

  normalized[0] = normalizePatternLetter(patternName[0]);
  normalized[1] = patternName[1];
  normalized[2] = patternName[2];
  normalized[3] = '\0';

  return String(normalized);

} //   normalizeStrictPatternToken()

//-- Read chain settings from JSON document with backward-compatible keys.
static void readChainSettingsFromJson(const JsonDocument& jsonDocument, bool& outEnabled, uint8_t& outLength, String& outTarget)
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
  outTarget = normalizeStrictPatternToken(chainTarget);

} //   readChainSettingsFromJson()

//-- Write chain settings into JSON document while preserving unrelated fields.
static void writeChainSettingsToJson(JsonDocument& jsonDocument, bool enabled, uint8_t length, const String& target)
{
  JsonObject chainObject = jsonDocument["chain"].to<JsonObject>();

  jsonDocument["chainEnabled"] = enabled;
  jsonDocument["chainLength"] = length;
  jsonDocument["chainTarget"] = target;

  chainObject["enabled"] = enabled;
  chainObject["length"] = length;
  chainObject["target"] = target;

} //   writeChainSettingsToJson()

//-- Return LittleFS usage values in bytes.
bool settingsStoreGetLittleFsUsage(size_t& outTotalBytes, size_t& outUsedBytes, size_t& outFreeBytes)
{
  if (!ensurePatternDirectory())
  {
    return false;
  }

  outTotalBytes = LittleFS.totalBytes();
  outUsedBytes = LittleFS.usedBytes();

  if (outUsedBytes > outTotalBytes)
  {
    outUsedBytes = outTotalBytes;
  }

  outFreeBytes = outTotalBytes - outUsedBytes;

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

  settings.displayRotation = static_cast<uint8_t>(jsonDocument["displayRotation"] | defaults.displayRotation);
  settings.themeColorIndex = static_cast<int>(jsonDocument["themeColorIndex"] | defaults.themeColorIndex);
  settings.encoderDirectionReversed = static_cast<bool>(jsonDocument["encoderDirectionReversed"] | defaults.encoderDirectionReversed);
  settings.activePatternName = static_cast<const char*>(jsonDocument["activePatternName"] | defaults.activePatternName.c_str());

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

      if (isPatternNameLetterNumberFormat(patternName))
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

//-- List available pattern names for one series letter (A..Z), sorted numerically.
bool settingsStoreListPatternsForSeries(char patternLetter, String patternNames[], size_t maxCount, size_t& outCount)
{
  String listedPatternNames[patternStoreMaxEntries];
  size_t listedCount = 0;
  char normalizedLetter;

  outCount = 0;

  if (patternNames == nullptr || maxCount == 0)
  {
    return false;
  }

  normalizedLetter = normalizePatternLetter(patternLetter);

  if (normalizedLetter < 'A' || normalizedLetter > 'Z')
  {
    return false;
  }

  if (!settingsStoreListPatterns(listedPatternNames, patternStoreMaxEntries, listedCount))
  {
    return false;
  }

  for (size_t patternIndex = 0; patternIndex < listedCount && outCount < maxCount; patternIndex++)
  {
    const String& patternName = listedPatternNames[patternIndex];

    if (patternName.length() == 3 && normalizePatternLetter(patternName[0]) == normalizedLetter)
    {
      patternNames[outCount] = normalizeStrictPatternToken(patternName);
      outCount++;
    }
  }

  for (size_t leftIndex = 0; leftIndex < outCount; leftIndex++)
  {
    for (size_t rightIndex = leftIndex + 1; rightIndex < outCount; rightIndex++)
    {
      uint8_t leftNumber = static_cast<uint8_t>(((patternNames[leftIndex][1] - '0') * 10) + (patternNames[leftIndex][2] - '0'));
      uint8_t rightNumber = static_cast<uint8_t>(((patternNames[rightIndex][1] - '0') * 10) + (patternNames[rightIndex][2] - '0'));

      if (rightNumber < leftNumber)
      {
        String temporaryName = patternNames[leftIndex];
        patternNames[leftIndex] = patternNames[rightIndex];
        patternNames[rightIndex] = temporaryName;
      }
    }
  }

  return true;

} //   settingsStoreListPatternsForSeries()

//-- Find next available default pattern name.
bool settingsStoreFindNextPatternName(String& outName)
{
  return settingsStoreFindNextPatternNameForLetter('A', outName);

} //   settingsStoreFindNextPatternName()

//-- Find next available default pattern name for one letter group.
bool settingsStoreFindNextPatternNameForLetter(char patternLetter, String& outName)
{
  char normalizedLetter;
  String listedPatternNames[patternStoreMaxEntries];
  size_t listedCount = 0;
  bool usedNumbers[100] = {false};

  if (!ensurePatternDirectory())
  {
    return false;
  }

  normalizedLetter = normalizePatternLetter(patternLetter);

  if (normalizedLetter < 'A' || normalizedLetter > 'Z')
  {
    return false;
  }

  if (!settingsStoreListPatterns(listedPatternNames, patternStoreMaxEntries, listedCount))
  {
    return false;
  }

  for (size_t patternIndex = 0; patternIndex < listedCount; patternIndex++)
  {
    const String& listedName = listedPatternNames[patternIndex];

    if (listedName.length() == 3 && listedName[0] == normalizedLetter)
    {
      int numberValue = (listedName[1] - '0') * 10 + (listedName[2] - '0');

      if (numberValue >= 1 && numberValue <= 99)
      {
        usedNumbers[numberValue] = true;
      }
    }
  }

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

} //   settingsStoreFindNextPatternNameForLetter()

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

//-- Count remaining free pattern slots for one letter group (0..99).
bool settingsStoreCountAvailablePatternSlotsForLetter(char patternLetter, int& outFreeCount)
{
  char normalizedLetter;
  int usedCount = 0;

  outFreeCount = 0;

  if (!ensurePatternDirectory())
  {
    return false;
  }

  normalizedLetter = normalizePatternLetter(patternLetter);

  if (normalizedLetter < 'A' || normalizedLetter > 'Z')
  {
    return false;
  }

  File directory = LittleFS.open(patternDirectoryPath, "r");

  if (!directory)
  {
    return false;
  }

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

} //   settingsStoreCountAvailablePatternSlotsForLetter()

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

//-- Save pattern payload to one JSON file.
bool settingsStoreSavePattern(const String& patternName, const PatternData& patternData)
{
  String normalizedName;
  String patternPath;
  String tempPatternPath;
  size_t payloadBytes;

  if (!ensurePatternDirectory())
  {
    return false;
  }

  normalizedName = normalizePatternName(patternName);
  patternPath = buildPatternPath(normalizedName);
  tempPatternPath = patternPath + patternTempFileSuffix;

  JsonDocument jsonDocument;
  buildPatternJsonDocument(normalizedName, patternData, jsonDocument);

  payloadBytes = static_cast<size_t>(measureJson(jsonDocument));

  if (!hasEnoughSpaceForPatternSave(payloadBytes))
  {
    ESP_LOGW(logTag,
             "Insufficient LittleFS space for %s (payload=%u bytes)",
             patternPath.c_str(),
             static_cast<unsigned>(payloadBytes));
    return false;
  }

  if (patternPathExists(tempPatternPath))
  {
    LittleFS.remove(tempPatternPath);
  }

  File file = LittleFS.open(tempPatternPath, "w");

  if (!file)
  {
    ESP_LOGW(logTag, "Failed to open %s for write", tempPatternPath.c_str());
    return false;
  }

  bool success = (serializeJson(jsonDocument, file) > 0);

  file.close();

  if (!success)
  {
    ESP_LOGW(logTag, "Failed to serialize %s", tempPatternPath.c_str());
    LittleFS.remove(tempPatternPath);
    return false;
  }

  File verifyFile = LittleFS.open(tempPatternPath, "r");

  if (!verifyFile)
  {
    ESP_LOGW(logTag, "Failed to verify %s", tempPatternPath.c_str());
    LittleFS.remove(tempPatternPath);
    return false;
  }

  size_t verifySize = static_cast<size_t>(verifyFile.size());
  verifyFile.close();

  if (verifySize == 0)
  {
    ESP_LOGW(logTag, "Temporary save file is empty: %s", tempPatternPath.c_str());
    LittleFS.remove(tempPatternPath);
    return false;
  }

  if (patternPathExists(patternPath))
  {
    LittleFS.remove(patternPath);
  }

  if (!LittleFS.rename(tempPatternPath, patternPath))
  {
    ESP_LOGW(logTag, "Failed to rename %s to %s", tempPatternPath.c_str(), patternPath.c_str());
    LittleFS.remove(tempPatternPath);
    return false;
  }

  return success;

} //   settingsStoreSavePattern()

//-- Save pattern payload to one JSON file on SD card.
bool settingsStoreSavePatternToCard(const String& patternName, const PatternData& patternData)
{
  String normalizedName;
  String patternPath;
  String tempPatternPath;
  JsonDocument jsonDocument;

  normalizedName = normalizePatternName(patternName);
  patternPath = String(sdPatternDirectoryPath) + "/" + normalizedName + patternFileExtension;
  tempPatternPath = patternPath + patternTempFileSuffix;

  if (SD.cardType() == CARD_NONE)
  {
    ESP_LOGW(logTag, "SD card not available for pattern save");
    return false;
  }

  File patternDirectory = SD.open(sdPatternDirectoryPath, FILE_READ);

  if (!patternDirectory || !patternDirectory.isDirectory())
  {
    patternDirectory.close();

    if (!SD.mkdir(sdPatternDirectoryPath))
    {
      ESP_LOGW(logTag, "Failed to create SD pattern directory %s", sdPatternDirectoryPath);
      return false;
    }
  }
  else
  {
    patternDirectory.close();
  }

  buildPatternJsonDocument(normalizedName, patternData, jsonDocument);

  if (sdPatternPathExists(tempPatternPath))
  {
    SD.remove(tempPatternPath);
  }

  File file = SD.open(tempPatternPath, FILE_WRITE);

  if (!file)
  {
    ESP_LOGW(logTag, "Failed to open SD %s for write", tempPatternPath.c_str());
    return false;
  }

  bool success = (serializeJson(jsonDocument, file) > 0);
  file.close();

  if (!success)
  {
    ESP_LOGW(logTag, "Failed to serialize SD %s", tempPatternPath.c_str());
    SD.remove(tempPatternPath);
    return false;
  }

  if (sdPatternPathExists(patternPath))
  {
    SD.remove(patternPath);
  }

  if (!SD.rename(tempPatternPath, patternPath))
  {
    ESP_LOGW(logTag, "Failed to rename SD %s to %s", tempPatternPath.c_str(), patternPath.c_str());
    SD.remove(tempPatternPath);
    return false;
  }

  return true;

} //   settingsStoreSavePatternToCard()

//-- List available pattern names from SD card /patterns.
bool settingsStoreListPatternsOnCard(String patternNames[], size_t maxCount, size_t& outCount)
{
  outCount = 0;

  if (patternNames == nullptr || maxCount == 0)
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
    if (!entry.isDirectory())
    {
      String patternName = patternNameFromPath(entry.name());

      if (isPatternNameLetterNumberFormat(patternName))
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

} //   settingsStoreListPatternsOnCard()

//-- Load pattern payload from SD card.
bool settingsStoreLoadPatternFromCard(const String& patternName, PatternData& patternData)
{
  String normalizedName = normalizePatternName(patternName);
  String patternPath = String(sdPatternDirectoryPath) + "/" + normalizedName + patternFileExtension;

  if (SD.cardType() == CARD_NONE || !sdPatternPathExists(patternPath))
  {
    return false;
  }

  File file = SD.open(patternPath, FILE_READ);

  if (!file)
  {
    ESP_LOGW(logTag, "Failed to open SD %s for read", patternPath.c_str());
    return false;
  }

  if (file.size() == 0)
  {
    file.close();
    ESP_LOGW(logTag, "Empty SD pattern file %s", patternPath.c_str());
    return false;
  }

  JsonDocument jsonDocument;
  DeserializationError error = deserializeJson(jsonDocument, file);

  file.close();

  if (error)
  {
    ESP_LOGW(logTag, "Invalid SD %s (%s)", patternPath.c_str(), error.c_str());
    return false;
  }

  return parsePatternJsonDocument(jsonDocument, patternData, patternPath);

} //   settingsStoreLoadPatternFromCard()

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

  if (file.size() == 0)
  {
    file.close();
    ESP_LOGW(logTag, "Empty pattern file %s; removing", patternPath.c_str());
    LittleFS.remove(patternPath);
    return false;
  }

  JsonDocument jsonDocument;
  DeserializationError error = deserializeJson(jsonDocument, file);

  file.close();

  if (error)
  {
    ESP_LOGW(logTag, "Invalid %s (%s)", patternPath.c_str(), error.c_str());

    if (error == DeserializationError::EmptyInput || error == DeserializationError::InvalidInput)
    {
      LittleFS.remove(patternPath);
    }

    return false;
  }

  return parsePatternJsonDocument(jsonDocument, patternData, patternPath);

} //   settingsStoreLoadPattern()

//-- Load chain settings from one existing pattern JSON file.
bool settingsStoreLoadPatternChainSettings(const String& patternName, bool& outEnabled, uint8_t& outLength, String& outTarget)
{
  String normalizedName;
  String patternPath;
  JsonDocument jsonDocument;

  outEnabled = false;
  outLength = 1;
  outTarget = "";

  normalizedName = normalizeStrictPatternToken(patternName);

  if (normalizedName.isEmpty())
  {
    return false;
  }

  patternPath = buildPatternPath(normalizedName);

  if (!ensurePatternDirectory() || !patternPathExists(patternPath))
  {
    return false;
  }

  File file = LittleFS.open(patternPath, "r");

  if (!file)
  {
    return false;
  }

  DeserializationError error = deserializeJson(jsonDocument, file);
  file.close();

  if (error)
  {
    return false;
  }

  readChainSettingsFromJson(jsonDocument, outEnabled, outLength, outTarget);
  return true;

} //   settingsStoreLoadPatternChainSettings()

//-- Load chain settings from one existing SD card pattern JSON file.
bool settingsStoreLoadPatternChainSettingsFromCard(const String& patternName, bool& outEnabled, uint8_t& outLength, String& outTarget)
{
  String normalizedName;
  String patternPath;
  JsonDocument jsonDocument;

  outEnabled = false;
  outLength = 1;
  outTarget = "";

  normalizedName = normalizeStrictPatternToken(patternName);

  if (normalizedName.isEmpty())
  {
    return false;
  }

  patternPath = String(sdPatternDirectoryPath) + "/" + normalizedName + patternFileExtension;

  if (SD.cardType() == CARD_NONE || !sdPatternPathExists(patternPath))
  {
    return false;
  }

  File file = SD.open(patternPath, FILE_READ);

  if (!file)
  {
    return false;
  }

  DeserializationError error = deserializeJson(jsonDocument, file);
  file.close();

  if (error)
  {
    return false;
  }

  readChainSettingsFromJson(jsonDocument, outEnabled, outLength, outTarget);
  return true;

} //   settingsStoreLoadPatternChainSettingsFromCard()

//-- Update only chain settings in one existing pattern JSON file.
bool settingsStoreSavePatternChainSettings(const String& patternName, bool chainEnabled, uint8_t chainLength, const String& chainTarget)
{
  String normalizedName;
  String normalizedTarget;
  String patternPath;
  String tempPatternPath;
  JsonDocument jsonDocument;

  normalizedName = normalizeStrictPatternToken(patternName);

  if (normalizedName.isEmpty())
  {
    return false;
  }

  if (chainLength < 1)
  {
    chainLength = 1;
  }
  else if (chainLength > sequencerPatternCount)
  {
    chainLength = sequencerPatternCount;
  }

  normalizedTarget = normalizeStrictPatternToken(chainTarget);
  patternPath = buildPatternPath(normalizedName);
  tempPatternPath = patternPath + patternTempFileSuffix;

  if (!ensurePatternDirectory() || !patternPathExists(patternPath))
  {
    return false;
  }

  File readFile = LittleFS.open(patternPath, "r");

  if (!readFile)
  {
    return false;
  }

  DeserializationError error = deserializeJson(jsonDocument, readFile);
  readFile.close();

  if (error)
  {
    ESP_LOGW(logTag, "Failed to parse %s for chain update", patternPath.c_str());
    return false;
  }

  writeChainSettingsToJson(jsonDocument, chainEnabled, chainLength, normalizedTarget);

  if (patternPathExists(tempPatternPath))
  {
    LittleFS.remove(tempPatternPath);
  }

  File writeFile = LittleFS.open(tempPatternPath, "w");

  if (!writeFile)
  {
    return false;
  }

  bool success = (serializeJson(jsonDocument, writeFile) > 0);
  writeFile.close();

  if (!success)
  {
    LittleFS.remove(tempPatternPath);
    return false;
  }

  if (patternPathExists(patternPath))
  {
    LittleFS.remove(patternPath);
  }

  if (!LittleFS.rename(tempPatternPath, patternPath))
  {
    LittleFS.remove(tempPatternPath);
    return false;
  }

  return true;

} //   settingsStoreSavePatternChainSettings()

//-- Remove one stored pattern file.
bool settingsStoreDeletePattern(const String& patternName)
{
  String patternPath = buildPatternPath(patternName);

  if (!ensurePatternDirectory() || !patternPathExists(patternPath))
  {
    return false;
  }

  return LittleFS.remove(patternPath);

} //   settingsStoreDeletePattern()

//-- Delete one SD card pattern file.
bool settingsStoreDeletePatternFromCard(const String& patternName)
{
  String normalizedName = normalizePatternName(patternName);
  String patternPath = String(sdPatternDirectoryPath) + "/" + normalizedName + patternFileExtension;

  if (SD.cardType() == CARD_NONE || !sdPatternPathExists(patternPath))
  {
    return false;
  }

  return SD.remove(patternPath);

} //   settingsStoreDeletePatternFromCard()
