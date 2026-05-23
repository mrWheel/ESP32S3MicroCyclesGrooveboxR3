/*** Last Changed: 2026-05-23 - 16:00 ***/
#include "audioEngine.h"
#include "appConfig.h"

#include <Arduino.h>
#include <driver/i2s.h>
#include <esp_log.h>
#include <math.h>

//-- Logging tag.
static const char* logTag = "AudioEngine";

//-- Audio constants.
static const int audioSampleRate = 22050;
static const int audioChannelCount = 2;
static const int audioBlockFrames = 128;
static const int audioVoiceCount = 16;
static const i2s_port_t audioI2sPort = I2S_NUM_0;

//-- One fixed mixer voice.
struct Voice
{
  bool active;
  const int16_t* sampleData;
  uint32_t frameCount;
  uint32_t position;
  uint8_t level;
};

//-- Fixed voice pool.
static Voice voices[audioVoiceCount];

//-- Interleaved stereo output buffer.
static int16_t outputBuffer[audioBlockFrames * audioChannelCount];

//-- Audio runtime statistics.
static AudioEngineStats stats;

//-- True when I2S output path was initialized successfully.
static bool audioOutputReady = false;

//-- Test tone phase.
static float sinePhase = 0.0f;

//-- Mix one mono frame from active voices.
static int16_t mixNextFrame(bool& hadVoices)
{
  int32_t mixed = 0;
  hadVoices = false;

  for (int voiceIndex = 0; voiceIndex < audioVoiceCount; voiceIndex++)
  {
    Voice& voice = voices[voiceIndex];

    if (!voice.active)
    {
      continue;
    }

    hadVoices = true;

    int32_t sampleValue = static_cast<int32_t>(voice.sampleData[voice.position]);
    mixed += (sampleValue * voice.level) / 255;

    voice.position++;

    if (voice.position >= voice.frameCount)
    {
      voice.active = false;
    }
  }

  if (!hadVoices && stats.testToneEnabled)
  {
    float sineValue = sinf(sinePhase);
    mixed = static_cast<int32_t>(sineValue * 9000.0f);
    sinePhase += 2.0f * static_cast<float>(M_PI) * 220.0f / static_cast<float>(audioSampleRate);

    if (sinePhase > 2.0f * static_cast<float>(M_PI))
    {
      sinePhase -= 2.0f * static_cast<float>(M_PI);
    }
  }

  if (mixed > 32767)
  {
    mixed = 32767;
  }
  else if (mixed < -32768)
  {
    mixed = -32768;
  }

  return static_cast<int16_t>(mixed);

} //   mixNextFrame()

//-- Initialize I2S, DMA and mixer state.
bool audioEngineInit()
{
#ifdef NO_DAC_HARDWARE
  for (int voiceIndex = 0; voiceIndex < audioVoiceCount; voiceIndex++)
  {
    voices[voiceIndex].active = false;
    voices[voiceIndex].sampleData = nullptr;
    voices[voiceIndex].frameCount = 0;
    voices[voiceIndex].position = 0;
    voices[voiceIndex].level = 0;
  }

  stats.dmaWriteFailures = 0;
  stats.activeVoiceCount = 0;
  stats.testToneEnabled = true;
  audioOutputReady = false;

  ESP_LOGW(logTag, "NO_DAC_HARDWARE is enabled. I2S/DAC initialization is skipped.");

  return true;
#else
  i2s_config_t i2sConfig = {};
  i2s_pin_config_t pinConfig = {};

  i2sConfig.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
  i2sConfig.sample_rate = audioSampleRate;
  i2sConfig.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  i2sConfig.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  i2sConfig.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  i2sConfig.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  i2sConfig.dma_buf_count = 8;
  i2sConfig.dma_buf_len = audioBlockFrames;
  i2sConfig.use_apll = false;
  i2sConfig.tx_desc_auto_clear = true;
  i2sConfig.fixed_mclk = 0;

  pinConfig.bck_io_num = PIN_I2S_BCLK;
  pinConfig.ws_io_num = PIN_I2S_WS;
  pinConfig.data_out_num = PIN_I2S_DOUT;
  pinConfig.data_in_num = I2S_PIN_NO_CHANGE;
  pinConfig.mck_io_num = I2S_PIN_NO_CHANGE;

  if (i2s_driver_install(audioI2sPort, &i2sConfig, 0, nullptr) != ESP_OK)
  {
    audioOutputReady = false;
    ESP_LOGE(logTag, "Failed to install I2S driver");
    return false;
  }

  if (i2s_set_pin(audioI2sPort, &pinConfig) != ESP_OK)
  {
    audioOutputReady = false;
    i2s_driver_uninstall(audioI2sPort);
    ESP_LOGE(logTag, "Failed to set I2S pins");
    return false;
  }

  if (i2s_zero_dma_buffer(audioI2sPort) != ESP_OK)
  {
    audioOutputReady = false;
    i2s_driver_uninstall(audioI2sPort);
    ESP_LOGE(logTag, "Failed to clear I2S DMA buffer");
    return false;
  }

  for (int voiceIndex = 0; voiceIndex < audioVoiceCount; voiceIndex++)
  {
    voices[voiceIndex].active = false;
    voices[voiceIndex].sampleData = nullptr;
    voices[voiceIndex].frameCount = 0;
    voices[voiceIndex].position = 0;
    voices[voiceIndex].level = 0;
  }

  stats.dmaWriteFailures = 0;
  stats.activeVoiceCount = 0;
  stats.testToneEnabled = true;
  audioOutputReady = true;

  ESP_LOGI(logTag, "Audio engine initialized");

  return true;
#endif

} //   audioEngineInit()

//-- Return true when I2S output is available.
bool audioEngineIsOutputReady()
{
  return audioOutputReady;

} //   audioEngineIsOutputReady()

//-- Trigger sample playback on a fixed voice slot.
void audioEngineTriggerSample(SampleId sampleId, uint8_t level)
{
  const SampleSlot& sample = sampleManagerGetSample(sampleId);

  if (!sample.valid || sample.data == nullptr || sample.frameCount == 0)
  {
    return;
  }

  int selectedVoice = -1;

  for (int voiceIndex = 0; voiceIndex < audioVoiceCount; voiceIndex++)
  {
    if (!voices[voiceIndex].active)
    {
      selectedVoice = voiceIndex;
      break;
    }
  }

  if (selectedVoice < 0)
  {
    selectedVoice = 0;
  }

  voices[selectedVoice].active = true;
  voices[selectedVoice].sampleData = sample.data;
  voices[selectedVoice].frameCount = sample.frameCount;
  voices[selectedVoice].position = 0;
  voices[selectedVoice].level = level;

} //   audioEngineTriggerSample()

//-- Render one audio block and write to I2S.
void audioEngineRenderBlock()
{
  uint32_t activeVoiceCount = 0;

  for (int frameIndex = 0; frameIndex < audioBlockFrames; frameIndex++)
  {
    bool hadVoices = false;
    int16_t monoSample = mixNextFrame(hadVoices);

    outputBuffer[(frameIndex * 2)] = monoSample;
    outputBuffer[(frameIndex * 2) + 1] = monoSample;
  }

#ifndef NO_DAC_HARDWARE
  size_t bytesWritten = 0;

  if (i2s_write(audioI2sPort, outputBuffer, sizeof(outputBuffer), &bytesWritten, 0) != ESP_OK)
  {
    stats.dmaWriteFailures++;
  }
#endif

  for (int voiceIndex = 0; voiceIndex < audioVoiceCount; voiceIndex++)
  {
    if (voices[voiceIndex].active)
    {
      activeVoiceCount++;
    }
  }

  stats.activeVoiceCount = activeVoiceCount;

} //   audioEngineRenderBlock()

//-- Enable or disable sine test tone when no voices are active.
void audioEngineSetTestToneEnabled(bool enabled)
{
  stats.testToneEnabled = enabled;

} //   audioEngineSetTestToneEnabled()

//-- Copy audio statistics for status UI.
void audioEngineGetStats(AudioEngineStats& outStats)
{
  outStats = stats;

} //   audioEngineGetStats()