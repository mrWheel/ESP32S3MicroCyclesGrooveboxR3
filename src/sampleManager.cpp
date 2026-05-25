/*** Last Changed: 2026-05-25 - 18:06 ***/
#include "sampleManager.h"
#include "appConfig.h"

#include <SD.h>
#include <SPI.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <math.h>
#include <string.h>

//-- Logging tag.
static const char* logTag = "SampleManager";

//-- Minimal PCM format payload from the "fmt " chunk.
struct FmtChunk
{
  uint16_t audioFormat;
  uint16_t numChannels;
  uint32_t sampleRate;
  uint32_t byteRate;
  uint16_t blockAlign;
  uint16_t bitsPerSample;
};

//-- SD sample source descriptor.
struct SampleSource
{
  const char* path;
  const char* name;
};

//-- Fixed sample pool.
static SampleSlot sampleSlots[sampleCount];

//-- Synthetic fallback waveforms.
static int16_t fallbackSamples[sampleCount][512];

//-- SD sample mapping for required tracks.
static const SampleSource sampleSources[sampleCount] =
    {
        {"/samples/kick.wav", "kick"},
        {"/samples/snare.wav", "snare"},
        {"/samples/ch.wav", "ch"},
        {"/samples/oh.wav", "oh"},
        {"/samples/tone.wav", "tone"},
        {"/samples/metal.wav", "metal"}};

//-- True when SD card mount succeeded.
static bool sdCardReady = false;

//-- True when SPIRAM is available at runtime.
static bool psramAvailable = false;

//-- Build absolute child path from directory and entry name.
static String buildSdChildPath(const char* parentPath, const char* entryName)
{
  String childPath = String(entryName);

  if (childPath.startsWith("/"))
  {
    return childPath;
  }

  if (strcmp(parentPath, "/") == 0)
  {
    return String("/") + childPath;
  }

  return String(parentPath) + "/" + childPath;

} //   buildSdChildPath()

//-- Log one SD directory recursively.
static void logSdDirectoryRecursive(const char* directoryPath, uint8_t depth)
{
  File directory = SD.open(directoryPath, FILE_READ);

  if (!directory)
  {
    ESP_LOGW(logTag, "Cannot open SD directory: %s", directoryPath);
    return;
  }

  if (!directory.isDirectory())
  {
    ESP_LOGW(logTag, "SD path is not a directory: %s", directoryPath);
    directory.close();
    return;
  }

  while (true)
  {
    File entry = directory.openNextFile();
    String entryPath;

    if (!entry)
    {
      break;
    }

    entryPath = buildSdChildPath(directoryPath, entry.name());

    String indent = "";

    for (uint8_t level = 0; level < depth; level++)
    {
      indent += "  ";
    }

    ESP_LOGI(logTag,
             "%s%s (%s, %lu bytes)",
             indent.c_str(),
             entryPath.c_str(),
             entry.isDirectory() ? "dir" : "file",
             static_cast<unsigned long>(entry.size()));

    if (entry.isDirectory())
    {
      logSdDirectoryRecursive(entryPath.c_str(), static_cast<uint8_t>(depth + 1));
    }

    entry.close();
  }

  directory.close();

} //   logSdDirectoryRecursive()

//-- Log current heap status for sample allocation diagnostics.
static void logSampleAllocationHeapState(const char* sampleName, size_t requiredBytes)
{
  size_t freeInternalBytes = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  size_t largestInternalBlockBytes = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  size_t freePsramBytes = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  size_t largestPsramBlockBytes = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  ESP_LOGI(logTag,
           "Sample allocation request for %s: need=%lu bytes, internalFree=%lu largestInternal=%lu, psramFree=%lu largestPsram=%lu",
           sampleName,
           static_cast<unsigned long>(requiredBytes),
           static_cast<unsigned long>(freeInternalBytes),
           static_cast<unsigned long>(largestInternalBlockBytes),
           static_cast<unsigned long>(freePsramBytes),
           static_cast<unsigned long>(largestPsramBlockBytes));

} //   logSampleAllocationHeapState()

//-- Read little-endian 16-bit value.
static uint16_t readLe16(const uint8_t* data)
{
  return static_cast<uint16_t>(data[0]) |
         (static_cast<uint16_t>(data[1]) << 8);

} //   readLe16()

//-- Read little-endian 32-bit value.
static uint32_t readLe32(const uint8_t* data)
{
  return static_cast<uint32_t>(data[0]) |
         (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) |
         (static_cast<uint32_t>(data[3]) << 24);

} //   readLe32()

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

//-- Parse RIFF chunks directly from file and locate "fmt " + "data" sections.
static bool parseWavLayoutFromFile(File& wavFile, FmtChunk& fmtChunk, uint32_t& dataOffset, uint32_t& dataSize)
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
    const char* chunkId;
    uint32_t chunkSize;
    uint32_t chunkDataOffset;
    uint32_t chunkEndOffset;

    if (wavFile.read(chunkHeader, sizeof(chunkHeader)) != static_cast<int>(sizeof(chunkHeader)))
    {
      return false;
    }

    chunkId = reinterpret_cast<const char*>(chunkHeader);
    chunkSize = readLe32(&chunkHeader[4]);
    chunkDataOffset = static_cast<uint32_t>(wavFile.position());

    if ((chunkDataOffset + chunkSize) > fileSize)
    {
      return false;
    }

    chunkEndOffset = chunkDataOffset + chunkSize;

    if (memcmp(chunkId, "fmt ", 4) == 0)
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
    else if (memcmp(chunkId, "data", 4) == 0)
    {
      dataOffset = chunkDataOffset;
      dataSize = chunkSize;
      dataFound = true;
    }

    wavFile.seek(chunkEndOffset);

    //-- RIFF chunks are word-aligned.
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
static bool readMonoSampleFromFile(File& wavFile, uint32_t dataSize, uint32_t& consumedBytes, uint16_t numChannels, uint16_t bitsPerSample, int16_t& monoSample)
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
      int32_t signed24;

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

  // Shared SPI bus with TFT: deselect both devices before mounting SD.
  pinMode(PIN_TFT_CS, OUTPUT);
  digitalWrite(PIN_TFT_CS, HIGH);
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);
  pinMode(PIN_SD_MISO, INPUT_PULLUP);

  SPI.end();
  SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);

  // Let board-level pull-ups and CS lines settle.
  delay(20);

  // Send dummy clocks with CS high so cards reliably enter SPI mode.
  SPI.beginTransaction(SPISettings(400000U, MSBFIRST, SPI_MODE0));

  for (uint8_t dummyIndex = 0; dummyIndex < 16; dummyIndex++)
  {
    (void)SPI.transfer(0xFF);
  }

  SPI.endTransaction();

  for (size_t attemptIndex = 0; attemptIndex < (sizeof(initFrequenciesHz) / sizeof(initFrequenciesHz[0])); attemptIndex++)
  {
    uint32_t initFrequency = initFrequenciesHz[attemptIndex];

    SD.end();

    ESP_LOGI(logTag,
             "SD init attempt %u at %luHz",
             static_cast<unsigned>(attemptIndex + 1),
             static_cast<unsigned long>(initFrequency));

    if (SD.begin(PIN_SD_CS, SPI, initFrequency))
    {
      uint8_t cardType = SD.cardType();

      if (cardType != CARD_NONE)
      {
        ESP_LOGI(logTag,
                 "SD card ready (freq=%luHz, CS=%d SCK=%d MISO=%d MOSI=%d)",
                 static_cast<unsigned long>(initFrequency),
                 PIN_SD_CS,
                 PIN_SD_SCK,
                 PIN_SD_MISO,
                 PIN_SD_MOSI);
        return true;
      }
    }

    delay(8);
  }

  ESP_LOGW(logTag,
           "SD mount failed (CS=%d SCK=%d MISO=%d MOSI=%d)",
           PIN_SD_CS,
           PIN_SD_SCK,
           PIN_SD_MISO,
           PIN_SD_MOSI);
  return false;

} //   initSdCard()

//-- Decode one WAV file from SD into sample slot storage.
static bool loadSampleFromSd(uint8_t sampleIndex)
{
  const SampleSource& source = sampleSources[sampleIndex];
  FmtChunk fmtChunk = {};
  uint32_t dataOffset = 0;
  uint32_t dataSize = 0;
  File wavFile;

  wavFile = SD.open(source.path, FILE_READ);

  if (!wavFile)
  {
    ESP_LOGW(logTag, "Missing SD sample: %s (%s)", source.name, source.path);
    return false;
  }

  size_t wavSize = static_cast<size_t>(wavFile.size());

  if (wavSize < 12)
  {
    ESP_LOGW(logTag, "Invalid/empty SD WAV: %s (%s)", source.name, source.path);
    wavFile.close();
    return false;
  }

  if (!parseWavLayoutFromFile(wavFile, fmtChunk, dataOffset, dataSize))
  {
    ESP_LOGW(logTag, "Invalid SD WAV layout: %s", source.name);
    wavFile.close();
    return false;
  }

  bool supportedFormat = (fmtChunk.audioFormat == 1) &&
                         ((fmtChunk.bitsPerSample == 16) || (fmtChunk.bitsPerSample == 24)) &&
                         (fmtChunk.numChannels >= 1) &&
                         (fmtChunk.numChannels <= 2) &&
                         ((fmtChunk.sampleRate == 22050) || (fmtChunk.sampleRate == 44100));

  if (!supportedFormat)
  {
    ESP_LOGW(logTag,
             "Unsupported WAV format: %s (fmt=%u ch=%u bits=%u rate=%lu)",
             source.name,
             static_cast<unsigned>(fmtChunk.audioFormat),
             static_cast<unsigned>(fmtChunk.numChannels),
             static_cast<unsigned>(fmtChunk.bitsPerSample),
             static_cast<unsigned long>(fmtChunk.sampleRate));
    wavFile.close();
    return false;
  }

  uint32_t bytesPerInputSample = static_cast<uint32_t>(fmtChunk.bitsPerSample) / 8U;
  uint32_t inputFrameCount = dataSize / (static_cast<uint32_t>(fmtChunk.numChannels) * bytesPerInputSample);

  if (inputFrameCount == 0)
  {
    ESP_LOGW(logTag, "SD WAV has no audio frames: %s", source.name);
    wavFile.close();
    return false;
  }

  uint32_t outputFrameCount = inputFrameCount;

  if (fmtChunk.sampleRate == 44100)
  {
    outputFrameCount = inputFrameCount / 2;

    if (outputFrameCount == 0)
    {
      outputFrameCount = 1;
    }
  }

  size_t sampleByteCount = static_cast<size_t>(outputFrameCount) * sizeof(int16_t);

  logSampleAllocationHeapState(source.name, sampleByteCount);

  int16_t* sampleData = static_cast<int16_t*>(heap_caps_malloc(sampleByteCount, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  bool sampleStoredInInternalRam = (sampleData != nullptr);

  if (sampleData == nullptr)
  {
    sampleData = static_cast<int16_t*>(heap_caps_malloc(sampleByteCount, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  }

  if (sampleData == nullptr)
  {
    ESP_LOGE(logTag, "Out of memory for SD sample: %s (need=%lu bytes)", source.name, static_cast<unsigned long>(sampleByteCount));
    logSampleAllocationHeapState(source.name, sampleByteCount);
    wavFile.close();
    return false;
  }

  bool decodeOk = true;

  if (fmtChunk.sampleRate == 22050 && fmtChunk.numChannels == 1 && fmtChunk.bitsPerSample == 16)
  {
    if (!wavFile.seek(dataOffset))
    {
      ESP_LOGW(logTag, "Failed to seek SD WAV payload: %s", source.name);
      free(sampleData);
      wavFile.close();
      return false;
    }

    size_t bytesRead = static_cast<size_t>(wavFile.read(reinterpret_cast<uint8_t*>(sampleData), sampleByteCount));

    if (bytesRead != sampleByteCount)
    {
      ESP_LOGW(logTag, "Short PCM read for SD WAV: %s", source.name);
      free(sampleData);
      wavFile.close();
      return false;
    }
  }
  else
  {
    uint32_t consumedBytes = 0;

    if (!wavFile.seek(dataOffset))
    {
      ESP_LOGW(logTag, "Failed to seek SD WAV payload: %s", source.name);
      free(sampleData);
      wavFile.close();
      return false;
    }

    for (uint32_t outputIndex = 0; outputIndex < outputFrameCount; outputIndex++)
    {
      int16_t firstSample = 0;

      if (!readMonoSampleFromFile(wavFile, dataSize, consumedBytes, fmtChunk.numChannels, fmtChunk.bitsPerSample, firstSample))
      {
        decodeOk = false;
        break;
      }

      if (fmtChunk.sampleRate == 44100)
      {
        int16_t secondSample = firstSample;

        if (!readMonoSampleFromFile(wavFile, dataSize, consumedBytes, fmtChunk.numChannels, fmtChunk.bitsPerSample, secondSample))
        {
          decodeOk = false;
          break;
        }

        int32_t mixed = static_cast<int32_t>(firstSample) + static_cast<int32_t>(secondSample);
        sampleData[outputIndex] = static_cast<int16_t>(mixed / 2);
      }
      else
      {
        sampleData[outputIndex] = firstSample;
      }
    }
  }

  if (!decodeOk)
  {
    ESP_LOGW(logTag, "Failed to decode SD WAV data: %s", source.name);
    free(sampleData);
    wavFile.close();
    return false;
  }

  wavFile.close();

  sampleSlots[sampleIndex].data = sampleData;
  sampleSlots[sampleIndex].frameCount = outputFrameCount;
  sampleSlots[sampleIndex].valid = true;
  sampleSlots[sampleIndex].fromSd = true;
  sampleSlots[sampleIndex].storedInPsram = !sampleStoredInInternalRam;
  strncpy(sampleSlots[sampleIndex].name, source.name, sizeof(sampleSlots[sampleIndex].name) - 1);
  sampleSlots[sampleIndex].name[sizeof(sampleSlots[sampleIndex].name) - 1] = '\0';

  ESP_LOGI(logTag,
           "Loaded SD sample %s (%lu frames, %s)",
           sampleSlots[sampleIndex].name,
           static_cast<unsigned long>(outputFrameCount),
           sampleStoredInInternalRam ? "internal RAM" : "PSRAM");

  return true;

} //   loadSampleFromSd()

//-- Initialize sample pool and load WAV files from SD.
bool sampleManagerInit()
{
  sdCardReady = initSdCard();
  psramAvailable = (heap_caps_get_total_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) > 0);

  ESP_LOGI(logTag,
           "PSRAM: %s",
           psramAvailable ? "available" : "not available");

  if (!sdCardReady)
  {
    ESP_LOGW(logTag, "SD unavailable, all tracks use fallback waveforms");
  }
  else
  {
    ESP_LOGI(logTag, "SD root listing before sample load:");
    logSdDirectoryRecursive("/", 1);
  }

  for (uint8_t sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++)
  {
    sampleSlots[sampleIndex].data = fallbackSamples[sampleIndex];
    sampleSlots[sampleIndex].frameCount = 512;
    sampleSlots[sampleIndex].valid = true;
    sampleSlots[sampleIndex].fromSd = false;
    sampleSlots[sampleIndex].storedInPsram = false;
    strncpy(sampleSlots[sampleIndex].name, sampleSources[sampleIndex].name, sizeof(sampleSlots[sampleIndex].name) - 1);
    sampleSlots[sampleIndex].name[sizeof(sampleSlots[sampleIndex].name) - 1] = '\0';
    buildFallbackSample(sampleIndex);

    if (sdCardReady && !loadSampleFromSd(sampleIndex))
    {
      ESP_LOGW(logTag, "Using fallback sample for %s", sampleSlots[sampleIndex].name);
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
