# `src/audioEngine.cpp` — Real-Time Audio Synthesis, Voice Pool, and I2S Output Engine

**Purpose:** Sample playback with sample-accurate pitch/pan/decay control, realtime voice mixing, gain processing, and I2S stereo DMA output to external DAC.

---

---

## Responsibilities

```text
initialize I2S TX driver
manage fixed voice pool
select/steal voices
trigger sample playback
apply pitch, decay, attack/release fades and choke groups
apply sample gain and velocity curve
apply panning and master gain
render output blocks
write stereo interleaved samples to I2S
provide diagnostics and optional test tone
```

---

## Important Implementation Notes

- Never allocate memory inside `audioEngineRenderBlock()`.
- Never access SD, LittleFS, WiFi or display from the render path.
- The I2S DAC must be configured for standard I2S.
- Use `TEST_TONE` to prove hardware audio output before debugging samples.
- Choke and release behavior are central to preventing long sample stacking.

---

## Important Internal Areas

```text
fixed Voice array
outputBuffer stereo interleaved DMA buffer
TEST_TONE path
NO_DAC_HARDWARE compile path
applyVelocityCurve()
playbackFrameLimitForDecay()
release fade logic
voice stealing by estimated level
I2S pin config from build flags
```

---

## Public Functions

### `audioEngineInit()`

Initializes voices, stats and I2S output unless `NO_DAC_HARDWARE` is enabled.

### `audioEngineIsOutputReady()`

Returns whether audio output is ready.

### `audioEngineTriggerSample(...)`

Starts sample playback with level, gain, pan, choke group, decay and pitch parameters.

### `audioEngineSetMasterGainPercent(uint8_t)`

Sets runtime master gain.

### `audioEngineGetMasterGainPercent()`

Returns runtime master gain.

### `audioEngineRenderBlock()`

Renders one audio block and writes it to I2S. Called by AudioTask.

### `audioEngineSetTestToneEnabled(bool)`

Enables/disables sine test tone when compiled in.

### `audioEngineStopAllVoices()`

Immediately stops all active voices.

### `audioEngineGetStats(AudioEngineStats&)`

Copies runtime audio diagnostics.


---

[⬆ UP](developerBuildGuide.md#20-source-file-reference) | [📖 README](../README.md#)
