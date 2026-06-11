/*** Last Changed: 2026-06-11 - 11:30 ***/
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
//-- Short start fade to reduce clicks on sample start/retrigger.
static const uint16_t voiceAttackFrames = 32;
static const i2s_port_t audioI2sPort = I2S_NUM_0;

//-- Fixed voice pool (Phase 4)
static Voice voices[MAX_VOICES];

//-- Runtime master gain, persisted through settingsStore/NVS.
static uint8_t masterGainPercent = 100;

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
  static constexpr int32_t fullScale = 32767;
  static constexpr uint8_t limiterThresholdPercent = 85;

  const int32_t threshold = (fullScale * limiterThresholdPercent) / 100;
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

} //   applyHeadroomLimiter()

//-- Apply runtime master gain, limiter and clamp to int16 range.
static int16_t applyMasterGainAndClamp(int32_t sampleValue)
{
  int32_t scaled = (sampleValue * static_cast<int32_t>(masterGainPercent)) / 100;
  int32_t limited = applyHeadroomLimiter(scaled);

  if (limited > 32767)
  {
    limited = 32767;
  }
  else if (limited < -32768)
  {
    limited = -32768;
  }

  return static_cast<int16_t>(limited);

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

//-- Clamp pitch to supported semitone range.
static int8_t clampPitchValue(int8_t pitch)
{
  if (pitch < -24)
  {
    return -24;
  }

  if (pitch > 24)
  {
    return 24;
  }

  return pitch;

} //   clampPitchValue()

//-- Return fixed-point phase increment for semitone pitch.
static uint32_t phaseIncrementForPitch(int8_t pitch)
{
  static const uint32_t pitchIncrementTable[49] = {
      16384,  17358,  18390,  19484,  20643,  21870,  23170,  24548,  26008,  27554,
      29193,  30929,  32768,  34716,  36781,  38968,  41285,  43740,  46341,  49097,
      52016,  55109,  58386,  61858,  65536,  69433,  73562,  77936,  82570,  87481,
      92682,  98193,  104032, 110218, 116772, 123716, 131072, 138866, 147124, 155872,
      165140, 174961, 185364, 196386, 208064, 220436, 233544, 247431, 262144};

  int8_t clampedPitch = clampPitchValue(pitch);
  uint8_t tableIndex = static_cast<uint8_t>(clampedPitch + 24);

  return pitchIncrementTable[tableIndex];

} //   phaseIncrementForPitch()

//-- Return playback frame limit for a decay percentage.
static uint32_t playbackFrameLimitForDecay(uint32_t frameCount, uint8_t decayPercent,
                                           uint32_t maxFrames)
{
  static const uint32_t minimumPlaybackFrames = 64;

  if (frameCount == 0)
  {
    return 0;
  }

  uint32_t playbackFrameLimit = (frameCount * static_cast<uint32_t>(decayPercent)) / 100U;

  if (playbackFrameLimit < minimumPlaybackFrames)
  {
    playbackFrameLimit = minimumPlaybackFrames;
  }

  if (playbackFrameLimit > frameCount)
  {
    playbackFrameLimit = frameCount;
  }

  if (maxFrames > 0 && playbackFrameLimit > maxFrames)
  {
    playbackFrameLimit = maxFrames;
  }

  return playbackFrameLimit;

} //   playbackFrameLimitForDecay()

//-- Apply very short start fade for newly triggered voices.
static int32_t applyVoiceAttackFade(int32_t sampleValue, Voice& voice)
{
  if (voice.attackCounter >= voiceAttackFrames)
  {
    return sampleValue;
  }

  int32_t fadedSample = (sampleValue * static_cast<int32_t>(voice.attackCounter)) /
                        static_cast<int32_t>(voiceAttackFrames);

  voice.attackCounter++;

  return fadedSample;

} //   applyVoiceAttackFade()

//-- Clamp pan value to supported signed range.
static int8_t clampPanValue(int8_t pan)
{
  if (pan < -64)
  {
    return -64;
  }

  if (pan > 64)
  {
    return 64;
  }

  return pan;

} //   clampPanValue()

//-- Return default stereo pan for one sample voice.
static int8_t defaultPanForSample(SampleId sampleId)
{
  if (sampleId == sampleClosedHat)
  {
    return -30;
  }

  if (sampleId == sampleOpenHat)
  {
    return -45;
  }

  if (sampleId == sampleTone)
  {
    return 30;
  }

  if (sampleId == sampleMetal)
  {
    return 45;
  }

  return 0;

} //   defaultPanForSample()

//-- Mix one mono sample into left/right accumulators using equal-power style pan.
static void mixSampleWithPan(int32_t monoSample, int8_t pan, int32_t& mixLeft, int32_t& mixRight)
{
  int8_t clampedPan = clampPanValue(pan);

  int32_t leftGain = 256;
  int32_t rightGain = 256;

  if (clampedPan < 0)
  {
    rightGain = 256 + (static_cast<int32_t>(clampedPan) * 256) / 64;
  }
  else if (clampedPan > 0)
  {
    leftGain = 256 - (static_cast<int32_t>(clampedPan) * 256) / 64;
  }

  if (leftGain < 0)
  {
    leftGain = 0;
  }

  if (rightGain < 0)
  {
    rightGain = 0;
  }

  mixLeft += (monoSample * leftGain) >> 8;
  mixRight += (monoSample * rightGain) >> 8;

} //   mixSampleWithPan()

//-- Mix one stereo frame from active voices.
static void mixNextFrame(int16_t& outLeft, int16_t& outRight, bool& hadVoices)
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

  outLeft = applyMasterGainAndClamp(mixed);
  outRight = outLeft;
#else
  int32_t mixedLeft = 0;
  int32_t mixedRight = 0;

  hadVoices = false;

  for (int voiceIndex = 0; voiceIndex < MAX_VOICES; voiceIndex++)
  {
    Voice& voice = voices[voiceIndex];

    if (!voice.active)
    {
      continue;
    }

    uint32_t samplePosition = voice.phase >> 16;

    if (samplePosition >= voice.playbackFrameLimit)
    {
      startVoiceRelease(voice);

      if (voice.playbackFrameLimit > 0)
      {
        samplePosition = voice.playbackFrameLimit - 1U;
      }
      else
      {
        samplePosition = 0;
      }

      voice.phase = samplePosition << 16;
    }

    hadVoices = true;

    uint8_t sampleIndex = static_cast<uint8_t>(voice.sampleId);
    uint16_t sampleGain = sampleManagerGetSampleGainPercent(static_cast<SampleId>(sampleIndex));

    int32_t sampleValue = static_cast<int32_t>(voice.sampleData[samplePosition]);

    //-- Apply short fade-out near playback limit.
    const uint32_t fadeFrames = 512; //-- 256;

    if (voice.playbackFrameLimit > fadeFrames &&
        samplePosition >= (voice.playbackFrameLimit - fadeFrames))
    {
      uint32_t remainingFrames = voice.playbackFrameLimit - samplePosition;

      sampleValue =
          (sampleValue * static_cast<int32_t>(remainingFrames)) / static_cast<int32_t>(fadeFrames);
    }

    uint8_t curvedVelocity = applyVelocityCurve(voice.level);
    int32_t leveledSample = (sampleValue * static_cast<int32_t>(curvedVelocity)) / 255;

    int32_t sampleSetGainedSample = (leveledSample * static_cast<int32_t>(sampleGain)) / 100;
    int32_t gainedSample = applyVoiceGain(sampleSetGainedSample, voice.gain);

    gainedSample = applyVoiceAttackFade(gainedSample, voice);

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
        voice.attackCounter = 0;
        continue;
      }
    }

    int8_t voicePan = voice.pan;

    if (voicePan == 0)
    {
      voicePan = defaultPanForSample(voice.sampleId);
    }

    mixSampleWithPan(gainedSample, voicePan, mixedLeft, mixedRight);

    if (!voice.releaseActive)
    {
      voice.phase += voice.phaseIncrement;
      voice.position = voice.phase >> 16;
    }
  }

  if (!hadVoices && stats.testToneEnabled)
  {
    float sineValue = sinf(sinePhase);
    int32_t mixed = static_cast<int32_t>(sineValue * 9000.0f);

    sinePhase += 2.0f * static_cast<float>(M_PI) * 220.0f / static_cast<float>(audioSampleRate);

    if (sinePhase > 2.0f * static_cast<float>(M_PI))
    {
      sinePhase -= 2.0f * static_cast<float>(M_PI);
    }

    mixedLeft = mixed;
    mixedRight = mixed;
  }

  outLeft = applyMasterGainAndClamp(mixedLeft);
  outRight = applyMasterGainAndClamp(mixedRight);
#endif

} //   mixNextFrame()

//-- Initialize I2S, DMA and mixer state.
bool audioEngineInit()
{
#ifdef NO_DAC_HARDWARE
  for (int voiceIndex = 0; voiceIndex < MAX_VOICES; voiceIndex++)
  {
    voices[voiceIndex].active = false;
    voices[voiceIndex].sampleId = sampleKick;
    voices[voiceIndex].sampleData = nullptr;
    voices[voiceIndex].frameCount = 0;
    voices[voiceIndex].playbackFrameLimit = 0;
    voices[voiceIndex].position = 0;
    voices[voiceIndex].phase = 0;
    voices[voiceIndex].phaseIncrement = 65536;
    voices[voiceIndex].level = 0;
    voices[voiceIndex].gain = 65535;
    voices[voiceIndex].pan = 0;
    voices[voiceIndex].decayPercent = 100;
    voices[voiceIndex].pitch = 0;
    voices[voiceIndex].chokeGroup = 0;
    voices[voiceIndex].releaseActive = false;
    voices[voiceIndex].releaseCounter = 0;
    voices[voiceIndex].attackCounter = 0;
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
    voices[voiceIndex].sampleId = sampleKick;
    voices[voiceIndex].sampleData = nullptr;
    voices[voiceIndex].frameCount = 0;
    voices[voiceIndex].playbackFrameLimit = 0;
    voices[voiceIndex].position = 0;
    voices[voiceIndex].phase = 0;
    voices[voiceIndex].phaseIncrement = 65536;
    voices[voiceIndex].level = 0;
    voices[voiceIndex].gain = 65535;
    voices[voiceIndex].pan = 0;
    voices[voiceIndex].decayPercent = 100;
    voices[voiceIndex].pitch = 0;
    voices[voiceIndex].chokeGroup = 0;
    voices[voiceIndex].releaseActive = false;
    voices[voiceIndex].releaseCounter = 0;
    voices[voiceIndex].attackCounter = 0;
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

  ESP_LOGI(logTag, "I2S pins BCLK=%d WS=%d DOUT=%d ", pinConfig.bck_io_num, pinConfig.ws_io_num,
           pinConfig.data_out_num);
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

//-- Stop all active voices that are playing the same sample immediately.
static void stopVoicesForSampleId(SampleId sampleId)
{
  for (int voiceIndex = 0; voiceIndex < MAX_VOICES; voiceIndex++)
  {
    Voice& voice = voices[voiceIndex];

    if (!voice.active)
    {
      continue;
    }

    if (voice.sampleId != sampleId)
    {
      continue;
    }

    voice.active = false;
    voice.sampleId = sampleKick;
    voice.sampleData = nullptr;
    voice.frameCount = 0;
    voice.playbackFrameLimit = 0;
    voice.position = 0;
    voice.phase = 0;
    voice.phaseIncrement = 65536;
    voice.level = 0;
    voice.gain = 65535;
    voice.pan = 0;
    voice.decayPercent = 100;
    voice.pitch = 0;
    voice.chokeGroup = 0;
    voice.releaseActive = false;
    voice.releaseCounter = 0;
    voice.attackCounter = 0;
  }

} //   stopVoicesForSampleId()

//-- Trigger sample playback with full voice params.
void audioEngineTriggerSample(SampleId sampleId, uint8_t level, uint16_t gain, int8_t pan,
                              uint8_t chokeGroup, uint8_t decayPercent, int8_t pitch)
{
#ifdef TEST_TONE
  (void)sampleId;
  (void)level;
  (void)gain;
  (void)pan;
  (void)chokeGroup;
  (void)decayPercent;
  (void)pitch;
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

  //-- Prevent long one-shot samples from stacking across pattern loops.
  stopVoicesForSampleId(sampleId);

  releaseVoicesInChokeGroup(chokeGroup);

  selectedVoice = selectVoiceForPlayback();

  if (selectedVoice < 0 || selectedVoice >= MAX_VOICES)
  {
    selectedVoice = 0;
  }

  uint32_t maxPlaybackFrames = audioSampleRate;

  voices[selectedVoice].active = true;
  voices[selectedVoice].sampleId = sampleId;
  voices[selectedVoice].sampleData = sample.data;
  voices[selectedVoice].frameCount = sample.frameCount;
  voices[selectedVoice].playbackFrameLimit =
      playbackFrameLimitForDecay(sample.frameCount, decayPercent, maxPlaybackFrames);
  voices[selectedVoice].position = 0;
  voices[selectedVoice].phase = 0;
  voices[selectedVoice].phaseIncrement = phaseIncrementForPitch(pitch);
  voices[selectedVoice].level = level;
  voices[selectedVoice].gain = gain;
  voices[selectedVoice].pan = pan;
  voices[selectedVoice].decayPercent = decayPercent;
  voices[selectedVoice].pitch = clampPitchValue(pitch);
  voices[selectedVoice].chokeGroup = chokeGroup;
  voices[selectedVoice].releaseActive = false;
  voices[selectedVoice].releaseCounter = 0;
  voices[selectedVoice].attackCounter = 0;
#endif

} //   audioEngineTriggerSample()

//-- Set runtime master gain percentage.
void audioEngineSetMasterGainPercent(uint8_t gainPercent)
{
  if (gainPercent < 10)
  {
    gainPercent = 10;
  }
  else if (gainPercent > 200)
  {
    gainPercent = 200;
  }

  masterGainPercent = gainPercent;

} //   audioEngineSetMasterGainPercent()

//-- Get runtime master gain percentage.
uint8_t audioEngineGetMasterGainPercent()
{
  return masterGainPercent;

} //   audioEngineGetMasterGainPercent()

//-- Render one audio block and write it to I2S.
void audioEngineRenderBlock()
{
  if (!audioOutputReady)
  {
    return;
  }

  bool hadVoices = false;

  for (size_t frameIndex = 0; frameIndex < audioBlockFrames; frameIndex++)
  {
    int16_t leftSample = 0;
    int16_t rightSample = 0;

    mixNextFrame(leftSample, rightSample, hadVoices);

    outputBuffer[(frameIndex * audioChannelCount)] = leftSample;
    outputBuffer[(frameIndex * audioChannelCount) + 1U] = rightSample;
  }

  size_t bytesWritten = 0;

  i2s_write(I2S_NUM_0, outputBuffer, sizeof(outputBuffer), &bytesWritten, portMAX_DELAY);

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
