/*** Last Changed: 2026-05-23 - 17:33 ***/
#include "sampleManager.h"

#include <LittleFS.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <math.h>
#include <string.h>

//-- Logging tag.
static const char* logTag = "SampleManager";

//-- RIFF/WAV container header.
struct RiffHeader
{
  char riff[4];
  uint32_t chunkSize;
  char wave[4];
};

//-- Generic RIFF chunk header.
struct ChunkHeader
{
  char id[4];
  uint32_t size;
};

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

//-- Fixed sample pool.
static SampleSlot sampleSlots[sampleCount];

//-- Synthetic fallback waveforms.
static int16_t fallbackSamples[sampleCount][512];

//-- File mapping for required tracks.
static const char* samplePaths[sampleCount] =
    {
        "/samples/kick.wav",
        "/samples/snare.wav",
        "/samples/ch.wav",
        "/samples/oh.wav",
        "/samples/tone.wav",
        "/samples/metal.wav"};

//-- Human readable slot names.
static const char* sampleNames[sampleCount] =
    {
        "kick",
        "snare",
        "ch",
        "oh",
        "tone",
        "metal"};

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
static bool parseWavLayout(File& wavFile, FmtChunk& fmtChunk, uint32_t& dataOffset, uint32_t& dataSize)
{
  RiffHeader riffHeader;

  dataOffset = 0;
  dataSize = 0;

  if (wavFile.read(reinterpret_cast<uint8_t*>(&riffHeader), sizeof(RiffHeader)) != sizeof(RiffHeader))
  {
    return false;
  }

  if (memcmp(riffHeader.riff, "RIFF", 4) != 0 || memcmp(riffHeader.wave, "WAVE", 4) != 0)
  {
    return false;
  }

  bool fmtFound = false;
  bool dataFound = false;

  while (wavFile.available() >= static_cast<int>(sizeof(ChunkHeader)))
  {
    ChunkHeader chunkHeader;
    uint32_t chunkDataOffset = static_cast<uint32_t>(wavFile.position()) + sizeof(ChunkHeader);

    if (wavFile.read(reinterpret_cast<uint8_t*>(&chunkHeader), sizeof(ChunkHeader)) != sizeof(ChunkHeader))
    {
      return false;
    }

    if (memcmp(chunkHeader.id, "fmt ", 4) == 0)
    {
      if (chunkHeader.size < 16)
      {
        return false;
      }

      if (wavFile.read(reinterpret_cast<uint8_t*>(&fmtChunk), sizeof(FmtChunk)) != sizeof(FmtChunk))
      {
        return false;
      }

      fmtFound = true;
    }
    else if (memcmp(chunkHeader.id, "data", 4) == 0)
    {
      dataOffset = chunkDataOffset;
      dataSize = chunkHeader.size;
      dataFound = true;
    }

    uint32_t nextChunkOffset = chunkDataOffset + chunkHeader.size;

    //-- RIFF chunks are word-aligned.
    if ((chunkHeader.size & 1U) != 0U)
    {
      nextChunkOffset++;
    }

    if (!wavFile.seek(nextChunkOffset, SeekSet))
    {
      return false;
    }
  }

  if (!fmtFound || !dataFound)
  {
    return false;
  }

  return true;

} //   parseWavLayout()

//-- Read one mono sample from current file position.
static bool readMonoSample(File& wavFile, uint16_t numChannels, uint16_t bitsPerSample, int16_t& monoSample)
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
      int16_t pcm16 = 0;

      if (wavFile.read(reinterpret_cast<uint8_t*>(&pcm16), sizeof(int16_t)) != sizeof(int16_t))
      {
        return false;
      }

      channelSamples[channelIndex] = pcm16;
    }
    else
    {
      uint8_t pcm24[3];
      int32_t signed24 = 0;

      if (wavFile.read(pcm24, sizeof(pcm24)) != static_cast<int>(sizeof(pcm24)))
      {
        return false;
      }

      signed24 = static_cast<int32_t>(pcm24[0]) |
                 (static_cast<int32_t>(pcm24[1]) << 8) |
                 (static_cast<int32_t>(pcm24[2]) << 16);

      if ((signed24 & 0x00800000L) != 0)
      {
        signed24 |= 0xFF000000L;
      }

      //-- Convert signed 24-bit to signed 16-bit.
      channelSamples[channelIndex] = static_cast<int16_t>(signed24 >> 8);
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

} //   readMonoSample()

//-- Load one sample from LittleFS into PSRAM/internal RAM.
static bool loadSampleFromFile(uint8_t sampleIndex)
{
  if (!LittleFS.exists(samplePaths[sampleIndex]))
  {
    ESP_LOGW(logTag, "Missing sample file: %s", samplePaths[sampleIndex]);
    return false;
  }

  File wavFile = LittleFS.open(samplePaths[sampleIndex], "r");

  if (!wavFile)
  {
    ESP_LOGW(logTag, "Missing sample file: %s", samplePaths[sampleIndex]);
    return false;
  }

  FmtChunk fmtChunk;
  uint32_t dataOffset = 0;
  uint32_t dataSize = 0;

  if (!parseWavLayout(wavFile, fmtChunk, dataOffset, dataSize))
  {
    ESP_LOGW(logTag, "Invalid WAV layout: %s", samplePaths[sampleIndex]);
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
             samplePaths[sampleIndex],
             static_cast<unsigned>(fmtChunk.audioFormat),
             static_cast<unsigned>(fmtChunk.numChannels),
             static_cast<unsigned>(fmtChunk.bitsPerSample),
             static_cast<unsigned long>(fmtChunk.sampleRate));
    wavFile.close();
    return false;
  }

  uint32_t bytesPerSample = static_cast<uint32_t>(fmtChunk.bitsPerSample) / 8U;
  uint32_t inputFrameCount = dataSize / (static_cast<uint32_t>(fmtChunk.numChannels) * bytesPerSample);

  if (inputFrameCount == 0)
  {
    ESP_LOGW(logTag, "Empty WAV data: %s", samplePaths[sampleIndex]);
    wavFile.close();
    return false;
  }

  uint32_t frameCount = inputFrameCount;

  if (fmtChunk.sampleRate == 44100)
  {
    frameCount = inputFrameCount / 2;

    if (frameCount == 0)
    {
      frameCount = 1;
    }
  }

  size_t sampleByteCount = static_cast<size_t>(frameCount) * sizeof(int16_t);
  int16_t* sampleData = static_cast<int16_t*>(heap_caps_malloc(sampleByteCount, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

  if (sampleData == nullptr)
  {
    sampleData = static_cast<int16_t*>(heap_caps_malloc(sampleByteCount, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  }

  if (sampleData == nullptr)
  {
    ESP_LOGE(logTag, "Out of memory for sample: %s", samplePaths[sampleIndex]);
    wavFile.close();
    return false;
  }

  if (!wavFile.seek(dataOffset, SeekSet))
  {
    ESP_LOGW(logTag, "Failed to seek WAV data: %s", samplePaths[sampleIndex]);
    free(sampleData);
    wavFile.close();
    return false;
  }

  bool decodeOk = true;

  if (fmtChunk.sampleRate == 22050 && fmtChunk.numChannels == 1 && fmtChunk.bitsPerSample == 16)
  {
    if (wavFile.read(reinterpret_cast<uint8_t*>(sampleData), sampleByteCount) != static_cast<int>(sampleByteCount))
    {
      decodeOk = false;
    }
  }
  else
  {
    for (uint32_t outputIndex = 0; outputIndex < frameCount; outputIndex++)
    {
      int16_t firstSample = 0;

      if (!readMonoSample(wavFile, fmtChunk.numChannels, fmtChunk.bitsPerSample, firstSample))
      {
        decodeOk = false;
        break;
      }

      if (fmtChunk.sampleRate == 44100)
      {
        int16_t secondSample = firstSample;

        if (!readMonoSample(wavFile, fmtChunk.numChannels, fmtChunk.bitsPerSample, secondSample))
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
    ESP_LOGW(logTag, "Failed to read WAV data: %s", samplePaths[sampleIndex]);
    free(sampleData);
    wavFile.close();
    return false;
  }

  wavFile.close();

  sampleSlots[sampleIndex].data = sampleData;
  sampleSlots[sampleIndex].frameCount = frameCount;
  sampleSlots[sampleIndex].valid = true;
  strncpy(sampleSlots[sampleIndex].name, sampleNames[sampleIndex], sizeof(sampleSlots[sampleIndex].name) - 1);
  sampleSlots[sampleIndex].name[sizeof(sampleSlots[sampleIndex].name) - 1] = '\0';

  ESP_LOGI(logTag, "Loaded sample %s (%lu frames)", sampleSlots[sampleIndex].name, static_cast<unsigned long>(frameCount));

  return true;

} //   loadSampleFromFile()

//-- Initialize sample pool and load all configured sample files.
bool sampleManagerInit()
{
  if (!LittleFS.begin(true, "/littlefs", 10, "littlefs"))
  {
    ESP_LOGE(logTag, "LittleFS mount failed");
    return false;
  }

  for (uint8_t sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++)
  {
    sampleSlots[sampleIndex].data = fallbackSamples[sampleIndex];
    sampleSlots[sampleIndex].frameCount = 512;
    sampleSlots[sampleIndex].valid = true;
    strncpy(sampleSlots[sampleIndex].name, sampleNames[sampleIndex], sizeof(sampleSlots[sampleIndex].name) - 1);
    sampleSlots[sampleIndex].name[sizeof(sampleSlots[sampleIndex].name) - 1] = '\0';
    buildFallbackSample(sampleIndex);

    if (!loadSampleFromFile(sampleIndex))
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