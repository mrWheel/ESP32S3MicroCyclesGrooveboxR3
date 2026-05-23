/*** Last Changed: 2026-05-23 - 16:00 ***/
#include "sampleManager.h"

#include <LittleFS.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <math.h>
#include <string.h>

//-- Logging tag.
static const char* logTag = "SampleManager";

//-- Minimal PCM WAV header.
struct WavHeader
{
  char riff[4];
  uint32_t chunkSize;
  char wave[4];
  char fmt[4];
  uint32_t fmtSize;
  uint16_t audioFormat;
  uint16_t numChannels;
  uint32_t sampleRate;
  uint32_t byteRate;
  uint16_t blockAlign;
  uint16_t bitsPerSample;
  char data[4];
  uint32_t dataSize;
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

//-- Validate WAV header for required realtime format.
static bool isValidWavHeader(const WavHeader& header)
{
  if (memcmp(header.riff, "RIFF", 4) != 0)
  {
    return false;
  }

  if (memcmp(header.wave, "WAVE", 4) != 0)
  {
    return false;
  }

  if (memcmp(header.fmt, "fmt ", 4) != 0)
  {
    return false;
  }

  if (memcmp(header.data, "data", 4) != 0)
  {
    return false;
  }

  if (header.audioFormat != 1)
  {
    return false;
  }

  if (header.numChannels != 1)
  {
    return false;
  }

  if (header.bitsPerSample != 16)
  {
    return false;
  }

  if (header.sampleRate != 22050)
  {
    return false;
  }

  return true;

} //   isValidWavHeader()

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

  if (wavFile.size() < static_cast<int>(sizeof(WavHeader)))
  {
    ESP_LOGW(logTag, "Invalid WAV file size: %s", samplePaths[sampleIndex]);
    wavFile.close();
    return false;
  }

  WavHeader header;

  if (wavFile.read(reinterpret_cast<uint8_t*>(&header), sizeof(WavHeader)) != sizeof(WavHeader))
  {
    ESP_LOGW(logTag, "Failed to read WAV header: %s", samplePaths[sampleIndex]);
    wavFile.close();
    return false;
  }

  if (!isValidWavHeader(header))
  {
    ESP_LOGW(logTag, "Unsupported WAV format: %s", samplePaths[sampleIndex]);
    wavFile.close();
    return false;
  }

  uint32_t frameCount = header.dataSize / sizeof(int16_t);
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

  if (wavFile.read(reinterpret_cast<uint8_t*>(sampleData), sampleByteCount) != static_cast<int>(sampleByteCount))
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
  if (!LittleFS.begin(true))
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