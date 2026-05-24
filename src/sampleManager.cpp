/*** Last Changed: 2026-05-24 - 11:12 ***/
#include "sampleManager.h"

#include "sampleCh.h"
#include "sampleKick.h"
#include "sampleMetal.h"
#include "sampleOh.h"
#include "sampleSnare.h"
#include "sampleTone.h"

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

//-- Embedded WAV source descriptor.
struct SampleSource
{
  const uint8_t* wavBytes;
  size_t wavSize;
  const char* name;
};

//-- Fixed sample pool.
static SampleSlot sampleSlots[sampleCount];

//-- Synthetic fallback waveforms.
static int16_t fallbackSamples[sampleCount][512];

//-- Embedded sample mapping for required tracks.
static const SampleSource sampleSources[sampleCount] =
    {
        {sampleKickWavBytes, sampleKickWavSize, "kick"},
        {sampleSnareWavBytes, sampleSnareWavSize, "snare"},
        {sampleChWavBytes, sampleChWavSize, "ch"},
        {sampleOhWavBytes, sampleOhWavSize, "oh"},
        {sampleToneWavBytes, sampleToneWavSize, "tone"},
        {sampleMetalWavBytes, sampleMetalWavSize, "metal"}};

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

//-- Parse RIFF chunks and locate "fmt " + "data" sections.
static bool parseWavLayout(const uint8_t* wavBytes, size_t wavSize, FmtChunk& fmtChunk, const uint8_t*& pcmData, uint32_t& dataSize)
{
  size_t offset = 0;
  bool fmtFound = false;
  bool dataFound = false;

  pcmData = nullptr;
  dataSize = 0;

  if (wavBytes == nullptr || wavSize < 12)
  {
    return false;
  }

  if (memcmp(&wavBytes[0], "RIFF", 4) != 0 || memcmp(&wavBytes[8], "WAVE", 4) != 0)
  {
    return false;
  }

  offset = 12;

  while ((offset + 8) <= wavSize)
  {
    const uint8_t* chunkHeader = &wavBytes[offset];
    const char* chunkId = reinterpret_cast<const char*>(chunkHeader);
    uint32_t chunkSize = readLe32(&chunkHeader[4]);
    size_t chunkDataOffset = offset + 8;
    size_t nextChunkOffset;

    if ((chunkDataOffset + chunkSize) > wavSize)
    {
      return false;
    }

    if (memcmp(chunkId, "fmt ", 4) == 0)
    {
      const uint8_t* fmtData = &wavBytes[chunkDataOffset];

      if (chunkSize < 16)
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
      pcmData = &wavBytes[chunkDataOffset];
      dataSize = chunkSize;
      dataFound = true;
    }

    nextChunkOffset = chunkDataOffset + chunkSize;

    //-- RIFF chunks are word-aligned.
    if ((chunkSize & 1U) != 0U)
    {
      nextChunkOffset++;
    }

    offset = nextChunkOffset;
  }

  return fmtFound && dataFound;

} //   parseWavLayout()

//-- Read one mono sample from current PCM offset.
static bool readMonoSampleFromBuffer(const uint8_t* pcmData, uint32_t dataSize, size_t& pcmOffset, uint16_t numChannels, uint16_t bitsPerSample, int16_t& monoSample)
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
      if ((pcmOffset + 2) > dataSize)
      {
        return false;
      }

      channelSamples[channelIndex] = static_cast<int16_t>(readLe16(&pcmData[pcmOffset]));
      pcmOffset += 2;
    }
    else
    {
      int32_t signed24 = 0;

      if ((pcmOffset + 3) > dataSize)
      {
        return false;
      }

      signed24 = static_cast<int32_t>(pcmData[pcmOffset + 0]) |
                 (static_cast<int32_t>(pcmData[pcmOffset + 1]) << 8) |
                 (static_cast<int32_t>(pcmData[pcmOffset + 2]) << 16);

      if ((signed24 & 0x00800000L) != 0)
      {
        signed24 |= 0xFF000000L;
      }

      channelSamples[channelIndex] = static_cast<int16_t>(signed24 >> 8);
      pcmOffset += 3;
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

} //   readMonoSampleFromBuffer()

//-- Decode one embedded WAV source into sample slot storage.
static bool loadSampleFromInclude(uint8_t sampleIndex)
{
  const SampleSource& source = sampleSources[sampleIndex];
  FmtChunk fmtChunk = {};
  const uint8_t* pcmData = nullptr;
  uint32_t dataSize = 0;

  if (source.wavBytes == nullptr || source.wavSize < 12)
  {
    ESP_LOGW(logTag, "Embedded WAV missing: %s", source.name);
    return false;
  }

  if (!parseWavLayout(source.wavBytes, source.wavSize, fmtChunk, pcmData, dataSize))
  {
    ESP_LOGW(logTag, "Invalid embedded WAV layout: %s", source.name);
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
    return false;
  }

  uint32_t bytesPerInputSample = static_cast<uint32_t>(fmtChunk.bitsPerSample) / 8U;
  uint32_t inputFrameCount = dataSize / (static_cast<uint32_t>(fmtChunk.numChannels) * bytesPerInputSample);

  if (inputFrameCount == 0)
  {
    ESP_LOGW(logTag, "Embedded WAV is empty: %s", source.name);
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
  int16_t* sampleData = static_cast<int16_t*>(heap_caps_malloc(sampleByteCount, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  bool sampleStoredInInternalRam = (sampleData != nullptr);

  if (sampleData == nullptr)
  {
    sampleData = static_cast<int16_t*>(heap_caps_malloc(sampleByteCount, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  }

  if (sampleData == nullptr)
  {
    ESP_LOGE(logTag, "Out of memory for embedded sample: %s", source.name);
    return false;
  }

  bool decodeOk = true;

  if (fmtChunk.sampleRate == 22050 && fmtChunk.numChannels == 1 && fmtChunk.bitsPerSample == 16)
  {
    memcpy(sampleData, pcmData, sampleByteCount);
  }
  else
  {
    size_t pcmOffset = 0;

    for (uint32_t outputIndex = 0; outputIndex < outputFrameCount; outputIndex++)
    {
      int16_t firstSample = 0;

      if (!readMonoSampleFromBuffer(pcmData, dataSize, pcmOffset, fmtChunk.numChannels, fmtChunk.bitsPerSample, firstSample))
      {
        decodeOk = false;
        break;
      }

      if (fmtChunk.sampleRate == 44100)
      {
        int16_t secondSample = firstSample;

        if (!readMonoSampleFromBuffer(pcmData, dataSize, pcmOffset, fmtChunk.numChannels, fmtChunk.bitsPerSample, secondSample))
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
    ESP_LOGW(logTag, "Failed to decode embedded WAV data: %s", source.name);
    free(sampleData);
    return false;
  }

  sampleSlots[sampleIndex].data = sampleData;
  sampleSlots[sampleIndex].frameCount = outputFrameCount;
  sampleSlots[sampleIndex].valid = true;
  strncpy(sampleSlots[sampleIndex].name, source.name, sizeof(sampleSlots[sampleIndex].name) - 1);
  sampleSlots[sampleIndex].name[sizeof(sampleSlots[sampleIndex].name) - 1] = '\0';

  ESP_LOGI(logTag,
           "Loaded embedded sample %s (%lu frames, %s)",
           sampleSlots[sampleIndex].name,
           static_cast<unsigned long>(outputFrameCount),
           sampleStoredInInternalRam ? "internal RAM" : "PSRAM");

  return true;

} //   loadSampleFromInclude()

//-- Initialize sample pool and decode all embedded sample WAV headers.
bool sampleManagerInit()
{
  for (uint8_t sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++)
  {
    sampleSlots[sampleIndex].data = fallbackSamples[sampleIndex];
    sampleSlots[sampleIndex].frameCount = 512;
    sampleSlots[sampleIndex].valid = true;
    strncpy(sampleSlots[sampleIndex].name, sampleSources[sampleIndex].name, sizeof(sampleSlots[sampleIndex].name) - 1);
    sampleSlots[sampleIndex].name[sizeof(sampleSlots[sampleIndex].name) - 1] = '\0';
    buildFallbackSample(sampleIndex);

    if (!loadSampleFromInclude(sampleIndex))
    {
      ESP_LOGW(logTag, "Using fallback sample for %s", sampleSlots[sampleIndex].name);
    }
  }

  return true;

} //   sampleManagerInit()

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
