/*** Last Changed: 2026-06-11 - 11:30 ***/
#include "sampleManager.h"
#include "appConfig.h"
#include "settingsStore.h"

#include <ArduinoJson.h>
#include <SD.h>
#include <SPI.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <math.h>
#include <string.h>

static const char* logTag = "SampleManager";

struct FmtChunk
{
  uint16_t audioFormat;
  uint16_t numChannels;
  uint32_t sampleRate;
  uint32_t byteRate;
  uint16_t blockAlign;
  uint16_t bitsPerSample;
};

static bool initSdCard();
static void buildFallbackSample(uint8_t sampleIndex);
static void loadSampleGainPercent();
static String getSampleSetDir();
static void logSdDirectoryRecursive(const char* directoryPath, uint8_t depth);
static void logSampleAllocationHeapState(const char* sampleName, size_t requiredBytes);
static uint16_t readLe16(const uint8_t* data);
static uint32_t readLe32(const uint8_t* data);
static bool parseWavLayoutFromFile(File& wavFile, FmtChunk& fmtChunk, uint32_t& dataOffset,
                                   uint32_t& dataSize);
static bool readMonoSampleFromFile(File& wavFile, uint32_t dataSize, uint32_t& consumedBytes,
                                   uint16_t numChannels, uint16_t bitsPerSample,
                                   int16_t& monoSample);
static bool loadSampleFromSdPath(uint8_t sampleIndex, const char* wavPath);

static bool sdCardReady = false;
static bool psramAvailable = false;

static SampleSlot sampleSlots[sampleCount];
static int16_t fallbackSamples[sampleCount][512];

static const char* sampleFileNames[sampleCount] = {"kick.wav", "snare.wav", "ch.wav",
                                                   "oh.wav",   "tone.wav",  "metal.wav"};

static const char* sampleNames[sampleCount] = {"kick", "snare", "ch", "oh", "tone", "metal"};

static char activeSampleSet[4] = "S1";
static uint16_t sampleGainPercent[sampleCount] = {100, 100, 100, 100, 100, 100};

//-- Get path to current sample set directory.
static String getSampleSetDir()
{
  return String("/samples/") + activeSampleSet + "/";

} //   getSampleSetDir()

//-- Load per-sample gain percentages from the active sample set.
static void loadSampleGainPercent()
{
  for (uint8_t sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++)
  {
    sampleGainPercent[sampleIndex] = 100;
  }

  String gainPath = String("/samples/") + activeSampleSet + "/setGain.json";

  if (!SD.exists(gainPath))
  {
    ESP_LOGW(logTag, "[SampleManager] Warning: setGain.json not found: %s", gainPath.c_str());
    return;
  }

  File file = SD.open(gainPath, FILE_READ);

  if (!file)
  {
    ESP_LOGW(logTag, "[SampleManager] Warning: Could not open %s", gainPath.c_str());
    return;
  }

  JsonDocument jsonDocument;
  DeserializationError error = deserializeJson(jsonDocument, file);

  file.close();

  if (error)
  {
    ESP_LOGW(logTag, "[SampleManager] Warning: Could not parse %s: %s", gainPath.c_str(),
             error.c_str());
    return;
  }

  JsonObject sampleGainPercentObject = jsonDocument["sampleGainPercent"].as<JsonObject>();

  if (sampleGainPercentObject.isNull())
  {
    ESP_LOGW(logTag, "[SampleManager] Warning: Missing sampleGainPercent in %s", gainPath.c_str());
    return;
  }

  for (uint8_t sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++)
  {
    const char* sampleName = sampleNames[sampleIndex];

    JsonVariant gainVariant = sampleGainPercentObject[sampleName];

    if (!gainVariant.is<int>())
    {
      ESP_LOGW(logTag, "[SampleManager] Warning: Missing gain for %s in %s", sampleName,
               gainPath.c_str());
      continue;
    }

    int gainPercent = gainVariant.as<int>();

    if (gainPercent < 0)
    {
      gainPercent = 0;
    }
    else if (gainPercent > 300)
    {
      gainPercent = 300;
    }

    sampleGainPercent[sampleIndex] = static_cast<uint16_t>(gainPercent);

    ESP_LOGI(logTag, "[SampleManager] Sample gain %s=%u%%", sampleName,
             static_cast<unsigned>(sampleGainPercent[sampleIndex]));
  }

} //   loadSampleGainPercent()

//-- Return true if sample set directory exists on SD.
static bool sampleManagerSampleSetExists(const char* sampleSetName)
{
  if (!sampleSetName || strlen(sampleSetName) != 2 || sampleSetName[0] != 'S' ||
      sampleSetName[1] < '1' || sampleSetName[1] > '9')
  {
    return false;
  }

  String sampleSetPath = String("/samples/") + sampleSetName;
  File directory = SD.open(sampleSetPath.c_str());

  if (!directory)
  {
    return false;
  }

  bool exists = directory.isDirectory();
  directory.close();

  return exists;

} //   sampleManagerSampleSetExists()

//-- List available sample sets from Card.
bool sampleManagerListSampleSets(char sampleSetNames[][4], uint8_t maxSampleSets,
                                 uint8_t* sampleSetCount)
{
  if (!sampleSetNames || !sampleSetCount)
  {
    return false;
  }

  *sampleSetCount = 0;

  if (!sdCardReady)
  {
    return false;
  }

  for (uint8_t sampleSetIndex = 1; sampleSetIndex <= 9; sampleSetIndex++)
  {
    char sampleSetName[4] = {0};

    snprintf(sampleSetName, sizeof(sampleSetName), "S%u", sampleSetIndex);

    if (sampleManagerSampleSetExists(sampleSetName))
    {
      strncpy(sampleSetNames[*sampleSetCount], sampleSetName, 3);
      sampleSetNames[*sampleSetCount][3] = '\0';
      (*sampleSetCount)++;

      if (*sampleSetCount >= maxSampleSets)
      {
        break;
      }
    }
  }

  return (*sampleSetCount > 0);

} //   sampleManagerListSampleSets()

//-- Load another sample set from SD.
bool sampleManagerLoadSampleSet(const char* sampleSetName)
{
  if (!sdCardReady)
  {
    ESP_LOGW(logTag, "Warning: Cannot load sample set, SD is not ready");
    return false;
  }

  if (!sampleManagerSampleSetExists(sampleSetName))
  {
    ESP_LOGW(logTag, "Warning: Sample set %s does not exist",
             sampleSetName ? sampleSetName : "null");
    return false;
  }

  strncpy(activeSampleSet, sampleSetName, sizeof(activeSampleSet) - 1);
  activeSampleSet[sizeof(activeSampleSet) - 1] = '\0';

  loadSampleGainPercent();

  String sampleSetDir = getSampleSetDir();

  ESP_LOGI(logTag, "Active sample set: %s", activeSampleSet);

  for (uint8_t sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++)
  {
    if (sampleSlots[sampleIndex].fromSd && sampleSlots[sampleIndex].data &&
        sampleSlots[sampleIndex].data != fallbackSamples[sampleIndex])
    {
      free(const_cast<int16_t*>(sampleSlots[sampleIndex].data));
    }

    buildFallbackSample(sampleIndex);

    sampleSlots[sampleIndex].data = fallbackSamples[sampleIndex];
    sampleSlots[sampleIndex].frameCount = 512;
    sampleSlots[sampleIndex].valid = true;
    sampleSlots[sampleIndex].fromSd = false;
    sampleSlots[sampleIndex].storedInPsram = false;

    strncpy(sampleSlots[sampleIndex].name, sampleNames[sampleIndex],
            sizeof(sampleSlots[sampleIndex].name) - 1);
    sampleSlots[sampleIndex].name[sizeof(sampleSlots[sampleIndex].name) - 1] = '\0';

    String wavPath = sampleSetDir + sampleFileNames[sampleIndex];

    ESP_LOGI(logTag, "Loading sample %s from %s", sampleNames[sampleIndex], wavPath.c_str());

    if (!loadSampleFromSdPath(sampleIndex, wavPath.c_str()))
    {
      ESP_LOGW(logTag, "Warning: Missing or invalid sample %s, using fallback", wavPath.c_str());
    }
  }

  ESP_LOGI(logTag, "Active sample set loaded: %s", activeSampleSet);

  return true;

} //   sampleManagerLoadSampleSet()

//-- Set active sample set name.
bool sampleManagerSetActiveSampleSet(const char* setName)
{
  if (!setName || strlen(setName) != 2 || setName[0] != 'S' || setName[1] < '1' || setName[1] > '9')
  {
    return false;
  }

  strncpy(activeSampleSet, setName, sizeof(activeSampleSet) - 1);
  activeSampleSet[sizeof(activeSampleSet) - 1] = '\0';

  return true;

} //   sampleManagerSetActiveSampleSet()

//-- Get active sample set name.
const char* sampleManagerGetActiveSampleSet()
{
  return activeSampleSet;

} //   sampleManagerGetActiveSampleSet()

//-- Return gain percentage for one sample.
uint16_t sampleManagerGetSampleGainPercent(SampleId sampleId)
{
  if (sampleId >= sampleCount)
  {
    return 100;
  }

  return sampleGainPercent[sampleId];

} //   sampleManagerGetSampleGainPercent()

//-- Read little-endian 16-bit value.
static uint16_t readLe16(const uint8_t* data)
{
  return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);

} //   readLe16()

//-- Read little-endian 32-bit value.
static uint32_t readLe32(const uint8_t* data)
{
  return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);

} //   readLe32()

//-- Log current heap status for sample allocation diagnostics.
static void logSampleAllocationHeapState(const char* sampleName, size_t requiredBytes)
{
  size_t freeInternalBytes = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  size_t largestInternalBlockBytes =
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  size_t freePsramBytes = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  size_t largestPsramBlockBytes =
      heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  ESP_LOGI(logTag,
           "Sample allocation request for %s: need=%lu bytes, internalFree=%lu "
           "largestInternal=%lu, psramFree=%lu largestPsram=%lu",
           sampleName, static_cast<unsigned long>(requiredBytes),
           static_cast<unsigned long>(freeInternalBytes),
           static_cast<unsigned long>(largestInternalBlockBytes),
           static_cast<unsigned long>(freePsramBytes),
           static_cast<unsigned long>(largestPsramBlockBytes));

} //   logSampleAllocationHeapState()

//-- Build deterministic fallback drum-ish waveforms.
static void buildFallbackSample(uint8_t sampleIndex)
{
  float phase = 0.0f;
  float phaseStep = 2.0f * static_cast<float>(M_PI) * (70.0f + (sampleIndex * 40.0f)) / 22050.0f;

  for (uint32_t frame = 0; frame < 512; frame++)
  {
    float envelope = 1.0f - (static_cast<float>(frame) / 512.0f);
    float harmonic = sinf(phase) + (0.2f * sinf(phase * 2.7f));
    float noise = (static_cast<float>((frame * (sampleIndex + 3)) % 31) / 31.0f) - 0.5f;
    float mixed = (0.75f * harmonic) + (0.25f * noise);
    int32_t sampleValue = static_cast<int32_t>(mixed * envelope * 28000.0f);

    if (sampleValue > 32767)
    {
      sampleValue = 32767;
    }
    else if (sampleValue < -32768)
    {
      sampleValue = -32768;
    }

    fallbackSamples[sampleIndex][frame] = static_cast<int16_t>(sampleValue);
    phase += phaseStep;
  }

} //   buildFallbackSample()

//-- Log an SD directory recursively.
static void logSdDirectoryRecursive(const char* directoryPath, uint8_t depth)
{
  File directory = SD.open(directoryPath);

  if (!directory)
  {
    ESP_LOGW(logTag, "Warning: Could not open directory %s", directoryPath);
    return;
  }

  if (!directory.isDirectory())
  {
    ESP_LOGW(logTag, "Warning: Path is not a directory: %s", directoryPath);
    directory.close();
    return;
  }

  while (true)
  {
    File entry = directory.openNextFile();

    if (!entry)
    {
      break;
    }

    ESP_LOGI(logTag, "%s%s%s", directoryPath, entry.name(), entry.isDirectory() ? "/" : "");

    if (entry.isDirectory() && depth > 0)
    {
      String childPath = String(directoryPath);

      if (!childPath.endsWith("/"))
      {
        childPath += "/";
      }

      childPath += entry.name();
      childPath += "/";

      logSdDirectoryRecursive(childPath.c_str(), depth - 1);
    }

    entry.close();
  }

  directory.close();

} //   logSdDirectoryRecursive()

//-- Parse RIFF chunks directly from file and locate fmt and data sections.
static bool parseWavLayoutFromFile(File& wavFile, FmtChunk& fmtChunk, uint32_t& dataOffset,
                                   uint32_t& dataSize)
{
  uint8_t riffHeader[12] = {0};
  uint32_t fileSize = static_cast<uint32_t>(wavFile.size());
  bool fmtFound = false;
  bool dataFound = false;

  dataOffset = 0;
  dataSize = 0;

  if (fileSize < 12)
  {
    return false;
  }

  wavFile.seek(0);

  if (wavFile.read(riffHeader, sizeof(riffHeader)) != static_cast<int>(sizeof(riffHeader)))
  {
    return false;
  }

  if (memcmp(&riffHeader[0], "RIFF", 4) != 0 || memcmp(&riffHeader[8], "WAVE", 4) != 0)
  {
    return false;
  }

  while ((static_cast<uint32_t>(wavFile.position()) + 8U) <= fileSize)
  {
    uint8_t chunkHeader[8] = {0};

    if (wavFile.read(chunkHeader, sizeof(chunkHeader)) != static_cast<int>(sizeof(chunkHeader)))
    {
      return false;
    }

    uint32_t chunkSize = readLe32(&chunkHeader[4]);
    uint32_t chunkDataOffset = static_cast<uint32_t>(wavFile.position());
    uint32_t chunkEndOffset = chunkDataOffset + chunkSize;

    if (chunkEndOffset > fileSize)
    {
      return false;
    }

    if (memcmp(chunkHeader, "fmt ", 4) == 0)
    {
      uint8_t fmtData[16] = {0};

      if (chunkSize < 16)
      {
        return false;
      }

      if (wavFile.read(fmtData, sizeof(fmtData)) != static_cast<int>(sizeof(fmtData)))
      {
        return false;
      }

      fmtChunk.audioFormat = readLe16(&fmtData[0]);
      fmtChunk.numChannels = readLe16(&fmtData[2]);
      fmtChunk.sampleRate = readLe32(&fmtData[4]);
      fmtChunk.byteRate = readLe32(&fmtData[8]);
      fmtChunk.blockAlign = readLe16(&fmtData[12]);
      fmtChunk.bitsPerSample = readLe16(&fmtData[14]);
      fmtFound = true;
    }
    else if (memcmp(chunkHeader, "data", 4) == 0)
    {
      dataOffset = chunkDataOffset;
      dataSize = chunkSize;
      dataFound = true;
    }

    wavFile.seek(chunkEndOffset);

    if ((chunkSize & 1U) != 0U)
    {
      if ((chunkEndOffset + 1U) > fileSize)
      {
        return false;
      }

      wavFile.seek(chunkEndOffset + 1U);
    }
  }

  return fmtFound && dataFound;

} //   parseWavLayoutFromFile()

//-- Read one mono sample frame directly from SD file data section.
static bool readMonoSampleFromFile(File& wavFile, uint32_t dataSize, uint32_t& consumedBytes,
                                   uint16_t numChannels, uint16_t bitsPerSample,
                                   int16_t& monoSample)
{
  int16_t channelSamples[2] = {0, 0};

  if (numChannels == 0 || numChannels > 2)
  {
    return false;
  }

  if (bitsPerSample != 16 && bitsPerSample != 24)
  {
    return false;
  }

  for (uint16_t channelIndex = 0; channelIndex < numChannels; channelIndex++)
  {
    if (bitsPerSample == 16)
    {
      uint8_t sampleBytes[2] = {0};

      if ((consumedBytes + 2U) > dataSize)
      {
        return false;
      }

      if (wavFile.read(sampleBytes, sizeof(sampleBytes)) != static_cast<int>(sizeof(sampleBytes)))
      {
        return false;
      }

      channelSamples[channelIndex] = static_cast<int16_t>(readLe16(sampleBytes));
      consumedBytes += 2U;
    }
    else
    {
      uint8_t sampleBytes[3] = {0};
      int32_t signed24 = 0;

      if ((consumedBytes + 3U) > dataSize)
      {
        return false;
      }

      if (wavFile.read(sampleBytes, sizeof(sampleBytes)) != static_cast<int>(sizeof(sampleBytes)))
      {
        return false;
      }

      signed24 = static_cast<int32_t>(sampleBytes[0]) |
                 (static_cast<int32_t>(sampleBytes[1]) << 8) |
                 (static_cast<int32_t>(sampleBytes[2]) << 16);

      if ((signed24 & 0x00800000L) != 0)
      {
        signed24 |= 0xFF000000L;
      }

      channelSamples[channelIndex] = static_cast<int16_t>(signed24 >> 8);
      consumedBytes += 3U;
    }
  }

  if (numChannels == 1)
  {
    monoSample = channelSamples[0];
    return true;
  }

  int32_t mixed = static_cast<int32_t>(channelSamples[0]) + static_cast<int32_t>(channelSamples[1]);
  monoSample = static_cast<int16_t>(mixed / 2);

  return true;

} //   readMonoSampleFromFile()

//-- Initialize SD card on configured pins.
static bool initSdCard()
{
  static const uint32_t initFrequenciesHz[] = {400000U, 1000000U, 4000000U};

  pinMode(PIN_TFT_CS, OUTPUT);
  digitalWrite(PIN_TFT_CS, HIGH);
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);
  pinMode(PIN_SD_MISO, INPUT_PULLUP);

  SPI.end();
  SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);

  delay(20);

  SPI.beginTransaction(SPISettings(400000U, MSBFIRST, SPI_MODE0));

  for (uint8_t dummyIndex = 0; dummyIndex < 16; dummyIndex++)
  {
    (void)SPI.transfer(0xFF);
  }

  SPI.endTransaction();

  for (size_t attemptIndex = 0;
       attemptIndex < (sizeof(initFrequenciesHz) / sizeof(initFrequenciesHz[0])); attemptIndex++)
  {
    uint32_t initFrequency = initFrequenciesHz[attemptIndex];

    SD.end();

    ESP_LOGI(logTag, "SD init attempt %u at %luHz", static_cast<unsigned>(attemptIndex + 1),
             static_cast<unsigned long>(initFrequency));

    if (SD.begin(PIN_SD_CS, SPI, initFrequency))
    {
      uint8_t cardType = SD.cardType();

      if (cardType != CARD_NONE)
      {
        ESP_LOGI(logTag, "SD card ready (freq=%luHz, CS=%d SCK=%d MISO=%d MOSI=%d)",
                 static_cast<unsigned long>(initFrequency), PIN_SD_CS, PIN_SD_SCK, PIN_SD_MISO,
                 PIN_SD_MOSI);

        return true;
      }
    }

    delay(8);
  }

  ESP_LOGW(logTag, "Warning: SD mount failed (CS=%d SCK=%d MISO=%d MOSI=%d)", PIN_SD_CS, PIN_SD_SCK,
           PIN_SD_MISO, PIN_SD_MOSI);

  return false;

} //   initSdCard()

//-- Load and decode a WAV file from SD into the sample slot.
static bool loadSampleFromSdPath(uint8_t sampleIndex, const char* wavPath)
{
  if (!wavPath || sampleIndex >= sampleCount)
  {
    return false;
  }

  File wavFile = SD.open(wavPath, FILE_READ);

  if (!wavFile)
  {
    return false;
  }

  FmtChunk fmt = {};
  uint32_t dataOffset = 0;
  uint32_t dataSize = 0;

  if (!parseWavLayoutFromFile(wavFile, fmt, dataOffset, dataSize))
  {
    wavFile.close();
    return false;
  }

  if (fmt.audioFormat != 1 || fmt.sampleRate != 44100 ||
      (fmt.bitsPerSample != 16 && fmt.bitsPerSample != 24) ||
      (fmt.numChannels != 1 && fmt.numChannels != 2))
  {
    wavFile.close();
    return false;
  }

  uint32_t frameCount = dataSize / (fmt.numChannels * (fmt.bitsPerSample / 8));

  if (frameCount == 0 || frameCount > (44100 * 10))
  {
    wavFile.close();
    return false;
  }

  size_t allocBytes = frameCount * sizeof(int16_t);

  logSampleAllocationHeapState(sampleNames[sampleIndex], allocBytes);

  int16_t* buffer = nullptr;
  bool usedPsram = false;

  if (psramAvailable)
  {
    buffer =
        static_cast<int16_t*>(heap_caps_malloc(allocBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    usedPsram = (buffer != nullptr);
  }

  if (!buffer)
  {
    buffer =
        static_cast<int16_t*>(heap_caps_malloc(allocBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    usedPsram = false;
  }

  if (!buffer)
  {
    ESP_LOGE(logTag, "Error: Sample %s memory allocation failed (%lu bytes)",
             sampleNames[sampleIndex], static_cast<unsigned long>(allocBytes));

    wavFile.close();
    return false;
  }

  wavFile.seek(dataOffset);

  uint32_t consumedBytes = 0;

  for (uint32_t frame = 0; frame < frameCount; frame++)
  {
    int16_t monoSample = 0;

    if (!readMonoSampleFromFile(wavFile, dataSize, consumedBytes, fmt.numChannels,
                                fmt.bitsPerSample, monoSample))
    {
      ESP_LOGE(logTag, "Error: Sample %s decode error at frame %lu", sampleNames[sampleIndex],
               static_cast<unsigned long>(frame));

      free(buffer);
      wavFile.close();

      return false;
    }

    buffer[frame] = monoSample;
  }

  wavFile.close();

  if (sampleSlots[sampleIndex].fromSd && sampleSlots[sampleIndex].data &&
      sampleSlots[sampleIndex].data != fallbackSamples[sampleIndex])
  {
    free(const_cast<int16_t*>(sampleSlots[sampleIndex].data));
  }

  sampleSlots[sampleIndex].data = buffer;
  sampleSlots[sampleIndex].frameCount = frameCount;
  sampleSlots[sampleIndex].valid = true;
  sampleSlots[sampleIndex].fromSd = true;
  sampleSlots[sampleIndex].storedInPsram = usedPsram;

  strncpy(sampleSlots[sampleIndex].name, sampleNames[sampleIndex],
          sizeof(sampleSlots[sampleIndex].name) - 1);
  sampleSlots[sampleIndex].name[sizeof(sampleSlots[sampleIndex].name) - 1] = '\0';

  return true;

} //   loadSampleFromSdPath()

//-- Initialize sample pool and load WAV files from SD.
bool sampleManagerInit()
{
  sdCardReady = initSdCard();
  psramAvailable = (heap_caps_get_total_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) > 0);

  ESP_LOGI(logTag, "PSRAM: %s", psramAvailable ? "available" : "not available");

  if (!sdCardReady)
  {
    ESP_LOGW(logTag, "Warning: SD unavailable, all tracks use fallback waveforms");
  }

  String storedSampleSet = settingsStoreGetActiveSampleSet();

  if (storedSampleSet.length() == 2)
  {
    sampleManagerSetActiveSampleSet(storedSampleSet.c_str());
  }

  ESP_LOGI(logTag, "Active sample set: %s", activeSampleSet);

  if (sdCardReady)
  {
#ifdef SD_VERBOSE_DIRECTORY_LISTING
    ESP_LOGI(logTag, "SD root listing before sample load:");
    logSdDirectoryRecursive("/", 1);
#endif

    loadSampleGainPercent();
  }
  String sampleSetDir = getSampleSetDir();

  for (uint8_t sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++)
  {
    buildFallbackSample(sampleIndex);

    sampleSlots[sampleIndex].data = fallbackSamples[sampleIndex];
    sampleSlots[sampleIndex].frameCount = 512;
    sampleSlots[sampleIndex].valid = true;
    sampleSlots[sampleIndex].fromSd = false;
    sampleSlots[sampleIndex].storedInPsram = false;

    strncpy(sampleSlots[sampleIndex].name, sampleNames[sampleIndex],
            sizeof(sampleSlots[sampleIndex].name) - 1);
    sampleSlots[sampleIndex].name[sizeof(sampleSlots[sampleIndex].name) - 1] = '\0';

    if (sdCardReady)
    {
      String wavPath = sampleSetDir + sampleFileNames[sampleIndex];

      ESP_LOGI(logTag, "Loading sample %s from %s", sampleNames[sampleIndex], wavPath.c_str());

      if (loadSampleFromSdPath(sampleIndex, wavPath.c_str()))
      {
        ESP_LOGI(logTag, "Loaded SD sample %s: %lu frames, %s", sampleNames[sampleIndex],
                 static_cast<unsigned long>(sampleSlots[sampleIndex].frameCount),
                 sampleSlots[sampleIndex].storedInPsram ? "PSRAM" : "RAM");
      }
      else
      {
        ESP_LOGW(logTag, "Warning: Missing or invalid sample %s, using fallback", wavPath.c_str());
      }
    }
  }

  return true;

} //   sampleManagerInit()

//-- True when SD card is mounted and usable.
bool sampleManagerIsSdCardReady()
{
  return sdCardReady;

} //   sampleManagerIsSdCardReady()

//-- Return one sample slot by fixed identifier.
const SampleSlot& sampleManagerGetSample(SampleId sampleId)
{
  return sampleSlots[sampleId];

} //   sampleManagerGetSample()

//-- Return one sample slot by track index mapping.
const SampleSlot& sampleManagerGetSampleForTrack(uint8_t trackIndex)
{
  if (trackIndex >= sampleCount)
  {
    return sampleSlots[0];
  }

  return sampleSlots[trackIndex];

} //   sampleManagerGetSampleForTrack()