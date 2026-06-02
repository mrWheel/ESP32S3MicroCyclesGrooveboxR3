/*** Last Changed: 2026-06-02 - 12:16 ***/
#include "audioEngine.h"
#include "appConfig.h"

#include <Arduino.h>
#include <driver/i2s.h>
#include <esp_log.h>
#include <math.h>

//-- Logging tag.
static const char* logTag = "AudioEngine";

//-- Audio constants.

static const int audioSampleRate = 44100;
static const int audioChannelCount = 2;
static const int audioBlockFrames = 128;
//-- Phase 4: fixed voice pool size
static const int MAX_VOICES = 8;
//-- Release fade length for voices that are being stopped or stolen.
static const uint16_t voiceReleaseFrames = 256;
static const i2s_port_t audioI2sPort = I2S_NUM_0;

#ifndef AUDIO_MASTER_GAIN_PERCENT
#define AUDIO_MASTER_GAIN_PERCENT 45
#endif

#if AUDIO_MASTER_GAIN_PERCENT < 10
#define AUDIO_MASTER_GAIN_EFFECTIVE_PERCENT 10
#elif AUDIO_MASTER_GAIN_PERCENT > 150
#define AUDIO_MASTER_GAIN_EFFECTIVE_PERCENT 150
#else
#define AUDIO_MASTER_GAIN_EFFECTIVE_PERCENT AUDIO_MASTER_GAIN_PERCENT
#endif

#ifndef AUDIO_HEADROOM_LIMITER_THRESHOLD_PERCENT
#define AUDIO_HEADROOM_LIMITER_THRESHOLD_PERCENT 85
#endif

#if AUDIO_HEADROOM_LIMITER_THRESHOLD_PERCENT < 50
#define AUDIO_HEADROOM_LIMITER_EFFECTIVE_THRESHOLD_PERCENT 50
#elif AUDIO_HEADROOM_LIMITER_THRESHOLD_PERCENT > 98
#define AUDIO_HEADROOM_LIMITER_EFFECTIVE_THRESHOLD_PERCENT 98
#else
#define AUDIO_HEADROOM_LIMITER_EFFECTIVE_THRESHOLD_PERCENT AUDIO_HEADROOM_LIMITER_THRESHOLD_PERCENT
#endif

//-- Fixed voice pool (Phase 4)
Voice voices[MAX_VOICES];

//-- Interleaved stereo output buffer.
static int16_t outputBuffer[audioBlockFrames * audioChannelCount];

//-- Audio runtime statistics.
static AudioEngineStats stats;

//-- True when I2S output path was initialized successfully.
static bool audioOutputReady = false;

//-- Test tone phase.
static float sinePhase = 0.0f;

#ifdef TEST_TONE
#ifndef TEST_TONE_FREQUENCY_HZ
#define TEST_TONE_FREQUENCY_HZ 1000
#endif

static const float testToneFrequencyHz = static_cast<float>(TEST_TONE_FREQUENCY_HZ);
static const float testToneAmplitude = 9000.0f;
#endif

//-- Apply soft-knee limiter near full scale to reduce harsh clipping.
static int32_t applyHeadroomLimiter(int32_t sampleValue)
{
#ifdef AUDIO_HEADROOM_LIMITER_ENABLE
  const int32_t fullScale = 32767;
  const int32_t threshold = (fullScale * AUDIO_HEADROOM_LIMITER_EFFECTIVE_THRESHOLD_PERCENT) / 100;
  int32_t sign = 1;
  int32_t magnitude = sampleValue;

  if (magnitude < 0)
  {
    sign = -1;
    magnitude = -magnitude;
  }

  if (magnitude <= threshold)
  {
    return sampleValue;
  }

  const int32_t headroom = fullScale - threshold;
  const int32_t over = magnitude - threshold;
  int32_t limited = threshold + ((over * headroom) / (over + headroom));

  if (limited > fullScale)
  {
    limited = fullScale;
  }

  return limited * sign;
#else
  return sampleValue;
#endif

} //   applyHeadroomLimiter()

//-- Apply final software gain and clamp to int16 range.
static int16_t applyMasterGainAndClamp(int32_t sampleValue)
{
  int32_t scaled = (sampleValue * AUDIO_MASTER_GAIN_EFFECTIVE_PERCENT) / 100;

  scaled = applyHeadroomLimiter(scaled);

  if (scaled > 32767)
  {
    scaled = 32767;
  }
  else if (scaled < -32768)
  {
    scaled = -32768;
  }

  return static_cast<int16_t>(scaled);

} //   applyMasterGainAndClamp()

//-- Start release fade for one active voice.
static void startVoiceRelease(Voice& voice)
{
  if (!voice.active)
  {
    return;
  }

  if (!voice.releaseActive)
  {
    voice.releaseActive = true;
    voice.releaseCounter = 0;
  }

} //   startVoiceRelease()

//-- Release all active voices in the same choke group.
static void releaseVoicesInChokeGroup(uint8_t chokeGroup)
{
  if (chokeGroup == 0)
  {
    return;
  }

  for (int voiceIndex = 0; voiceIndex < MAX_VOICES; voiceIndex++)
  {
    Voice& voice = voices[voiceIndex];

    if (!voice.active)
    {
      continue;
    }

    if (voice.chokeGroup == chokeGroup)
    {
      startVoiceRelease(voice);
    }
  }

} //   releaseVoicesInChokeGroup()

//-- Apply musical velocity curve for drum dynamics.
static uint8_t applyVelocityCurve(uint8_t velocity)
{
  uint16_t curvedVelocity;

  if (velocity == 0)
  {
    return 0;
  }

  curvedVelocity = static_cast<uint16_t>(
      (static_cast<uint16_t>(velocity) * static_cast<uint16_t>(velocity) + 127U) / 255U);

  if (curvedVelocity < 1U)
  {
    curvedVelocity = 1U;
  }

  if (curvedVelocity > 255U)
  {
    curvedVelocity = 255U;
  }

  return static_cast<uint8_t>(curvedVelocity);

} //   applyVelocityCurve()

//-- Apply per-voice gain in 16-bit fixed-point scale.
static int32_t applyVoiceGain(int32_t sampleValue, uint16_t voiceGain)
{
  if (voiceGain == 0)
  {
    return 0;
  }

  return (sampleValue * static_cast<int32_t>(voiceGain)) / 65535L;

} //   applyVoiceGain()

//-- Mix one mono frame from active voices.
static int16_t mixNextFrame(bool& hadVoices)
{
#ifdef TEST_TONE
  hadVoices = false;

  float sineValue = sinf(sinePhase);
  int32_t mixed = static_cast<int32_t>(sineValue * testToneAmplitude);

  sinePhase +=
      2.0f * static_cast<float>(M_PI) * testToneFrequencyHz / static_cast<float>(audioSampleRate);

  if (sinePhase > 2.0f * static_cast<float>(M_PI))
  {
    sinePhase -= 2.0f * static_cast<float>(M_PI);
  }

  return applyMasterGainAndClamp(mixed);
#else
  int32_t mixed = 0;

  hadVoices = false;

  for (int voiceIndex = 0; voiceIndex < MAX_VOICES; voiceIndex++)
  {
    Voice& voice = voices[voiceIndex];

    if (!voice.active)
    {
      continue;
    }

    hadVoices = true;

    uint8_t sampleIndex = static_cast<uint8_t>(voice.sampleId);
    uint16_t sampleGain = sampleManagerGetSampleGainPercent(static_cast<SampleId>(sampleIndex));

    int32_t sampleValue = static_cast<int32_t>(voice.sampleData[voice.position]);

    uint8_t curvedVelocity = applyVelocityCurve(voice.level);
    int32_t leveledSample = (sampleValue * static_cast<int32_t>(curvedVelocity)) / 255;

    int32_t sampleSetGainedSample = (leveledSample * static_cast<int32_t>(sampleGain)) / 100;
    int32_t gainedSample = applyVoiceGain(sampleSetGainedSample, voice.gain);

    if (voice.releaseActive)
    {
      uint16_t remainingFrames = 0;

      if (voice.releaseCounter < voiceReleaseFrames)
      {
        remainingFrames = static_cast<uint16_t>(voiceReleaseFrames - voice.releaseCounter);
      }

      gainedSample = (gainedSample * static_cast<int32_t>(remainingFrames)) /
                     static_cast<int32_t>(voiceReleaseFrames);

      voice.releaseCounter++;

      if (voice.releaseCounter >= voiceReleaseFrames)
      {
        voice.active = false;
        voice.releaseActive = false;
        voice.releaseCounter = 0;
        continue;
      }
    }

    mixed += gainedSample;

    voice.position++;

    if (voice.position >= voice.frameCount)
    {
      startVoiceRelease(voice);
      voice.position = static_cast<uint32_t>(voice.frameCount - 1U);
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

  return applyMasterGainAndClamp(mixed);
#endif

} //   mixNextFrame()

//-- Initialize I2S, DMA and mixer state.
bool audioEngineInit()
{
#ifdef NO_DAC_HARDWARE
  for (int voiceIndex = 0; voiceIndex < MAX_VOICES; voiceIndex++)
  {
    voices[voiceIndex].active = false;
    voices[voiceIndex].sampleData = nullptr;
    voices[voiceIndex].frameCount = 0;
    voices[voiceIndex].position = 0;
    voices[voiceIndex].level = 0;
  }

  stats.dmaWriteFailures = 0;
  stats.activeVoiceCount = 0;
#ifdef TEST_TONE
  stats.testToneEnabled = true;
#else
  stats.testToneEnabled = false;
#endif
  audioOutputReady = false;

#ifdef TEST_TONE
  ESP_LOGI(logTag, "TEST_TONE enabled: %.1f Hz sine", static_cast<double>(testToneFrequencyHz));
#endif
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

#ifdef PIN_I2S_SD
  // Many I2S DAC/amp modules use SD as shutdown/enable. Keep it enabled.
  pinMode(PIN_I2S_SD, OUTPUT);
  digitalWrite(PIN_I2S_SD, HIGH);
#endif

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

  for (int voiceIndex = 0; voiceIndex < MAX_VOICES; voiceIndex++)
  {
    voices[voiceIndex].active = false;
    voices[voiceIndex].sampleData = nullptr;
    voices[voiceIndex].frameCount = 0;
    voices[voiceIndex].position = 0;
    voices[voiceIndex].level = 0;
  }

  stats.dmaWriteFailures = 0;
  stats.activeVoiceCount = 0;
#ifdef TEST_TONE
  stats.testToneEnabled = true;
#else
  stats.testToneEnabled = false;
#endif
  audioOutputReady = true;

#ifdef TEST_TONE
  ESP_LOGI(logTag, "TEST_TONE enabled: %.1f Hz sine", static_cast<double>(testToneFrequencyHz));
#endif
#ifdef PIN_I2S_SD
  ESP_LOGI(logTag, "I2S SD/EN pin=%d set HIGH", PIN_I2S_SD);
#endif
#ifdef AUDIO_HEADROOM_LIMITER_ENABLE
  ESP_LOGI(logTag, "Headroom limiter enabled (threshold=%d%%)",
           AUDIO_HEADROOM_LIMITER_EFFECTIVE_THRESHOLD_PERCENT);
#endif
  ESP_LOGI(logTag, "I2S pins BCLK=%d WS=%d DOUT=%d (master gain=%d%%, requested=%d%%)",
           pinConfig.bck_io_num, pinConfig.ws_io_num, pinConfig.data_out_num,
           AUDIO_MASTER_GAIN_EFFECTIVE_PERCENT, AUDIO_MASTER_GAIN_PERCENT);
  ESP_LOGI(logTag, "Audio engine initialized");

  return true;
#endif

} //   audioEngineInit()

//-- Return true when I2S output is available.
bool audioEngineIsOutputReady()
{
  return audioOutputReady;

} //   audioEngineIsOutputReady()

//-- Return absolute int32 value without using floating point.
static int32_t absInt32(int32_t value)
{
  if (value < 0)
  {
    return -value;
  }

  return value;

} //   absInt32()

//-- Estimate current voice loudness for voice stealing priority.
static int32_t estimateVoiceLevel(const Voice& voice)
{
  if (!voice.active || voice.sampleData == nullptr || voice.position >= voice.frameCount)
  {
    return 0;
  }

  int32_t sampleValue = static_cast<int32_t>(voice.sampleData[voice.position]);
  int32_t levelValue = absInt32(sampleValue);

  levelValue = (levelValue * static_cast<int32_t>(voice.level)) / 255;

  return levelValue;

} //   estimateVoiceLevel()

//-- Select best voice slot for new playback.
static int selectVoiceForPlayback()
{
  int selectedVoice = -1;
  int quietestVoice = -1;
  int oldestVoice = 0;
  int32_t quietestLevel = INT32_MAX;
  uint32_t oldestPosition = 0;

  //-- Prefer inactive voice.
  for (int voiceIndex = 0; voiceIndex < MAX_VOICES; voiceIndex++)
  {
    if (!voices[voiceIndex].active)
    {
      return voiceIndex;
    }
  }

  //-- Prefer voice already in release fade.
  for (int voiceIndex = 0; voiceIndex < MAX_VOICES; voiceIndex++)
  {
    if (voices[voiceIndex].releaseActive)
    {
      return voiceIndex;
    }
  }

  //-- Prefer quietest active voice.
  for (int voiceIndex = 0; voiceIndex < MAX_VOICES; voiceIndex++)
  {
    int32_t voiceLevel = estimateVoiceLevel(voices[voiceIndex]);

    if (voiceLevel < quietestLevel)
    {
      quietestLevel = voiceLevel;
      quietestVoice = voiceIndex;
    }
  }

  if (quietestVoice >= 0 && quietestLevel < 512)
  {
    return quietestVoice;
  }

  //-- Fallback: steal oldest/most advanced voice.
  for (int voiceIndex = 0; voiceIndex < MAX_VOICES; voiceIndex++)
  {
    if (voices[voiceIndex].position >= oldestPosition)
    {
      oldestPosition = voices[voiceIndex].position;
      oldestVoice = voiceIndex;
    }
  }

  selectedVoice = oldestVoice;

  return selectedVoice;

} //   selectVoiceForPlayback()

//-- Trigger sample playback with full voice params.
void audioEngineTriggerSample(SampleId sampleId, uint8_t level, uint16_t gain, int8_t pan,
                              uint8_t chokeGroup)
{
#ifdef TEST_TONE
  (void)sampleId;
  (void)level;
  (void)gain;
  (void)pan;
  (void)chokeGroup;

  return;
#else
  const SampleSlot& sample = sampleManagerGetSample(sampleId);
  int selectedVoice = -1;

  if (!sample.valid || sample.data == nullptr || sample.frameCount == 0)
  {
    return;
  }

  if (level == 0)
  {
    return;
  }

  //-- Start release fade for older voices in the same choke group.
  releaseVoicesInChokeGroup(chokeGroup);

  selectedVoice = selectVoiceForPlayback();

  if (selectedVoice < 0 || selectedVoice >= MAX_VOICES)
  {
    selectedVoice = 0;
  }

  voices[selectedVoice].active = true;
  voices[selectedVoice].sampleId = sampleId;
  voices[selectedVoice].sampleData = sample.data;
  voices[selectedVoice].frameCount = sample.frameCount;
  voices[selectedVoice].position = 0;
  voices[selectedVoice].level = level;
  voices[selectedVoice].gain = gain;
  voices[selectedVoice].pan = pan;
  voices[selectedVoice].chokeGroup = chokeGroup;
  voices[selectedVoice].releaseActive = false;
  voices[selectedVoice].releaseCounter = 0;
#endif

} //   audioEngineTriggerSample()

//-- Backward compatibility: old trigger function
void audioEngineTriggerSample(SampleId sampleId, uint8_t level)
{
  audioEngineTriggerSample(sampleId, level, 100, 0, 0);
}

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

  for (int voiceIndex = 0; voiceIndex < MAX_VOICES; voiceIndex++)
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
#ifdef TEST_TONE
  (void)enabled;
  stats.testToneEnabled = true;
#else
  stats.testToneEnabled = enabled;
#endif

} //   audioEngineSetTestToneEnabled()

//-- Copy audio statistics for status UI.
void audioEngineGetStats(AudioEngineStats& outStats)
{
  outStats = stats;

} //   audioEngineGetStats()
