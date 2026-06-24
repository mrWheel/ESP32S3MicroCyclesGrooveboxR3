# `src/sampleManager.cpp` — SD Card, WAV Parsing, and Sample Loading

**Purpose:** SD card initialization, sample set management, WAV file decoding, per-sample gain loading, memory allocation (PSRAM first, fallback to RAM), and fallback sample generation.

---

## Responsibilities

```
1. Initialize SD card on dedicated SPI bus
2. Monitor card-detect pin (if configured)
3. Load and persist active sample set name (NVS)
4. List available sample sets (S1–S9 directories)
5. Load WAV files from /samples/<SET>/ directory
6. Parse WAV headers (format, sample rate, bit depth, channels)
7. Convert 16/24-bit PCM to mono int16_t samples
8. Prefer PSRAM allocation; fallback to internal RAM
9. Generate procedural fallback samples (sine, sawtooth) if WAV missing/oversized
10. Load per-sample gain from setGain.json
11. Expose SampleSlot objects to audioEngine
```

---

## Core Data Structures

**SampleSlot struct:**

```cpp
struct SampleSlot {
  const int16_t* data;       // sample buffer (mono, 44.1 kHz)
  uint32_t frameCount;       // total frames (samples)
  uint8_t gainPercent;       // scale factor from setGain.json
  bool isGenerated;          // true if fallback procedural sample
  uint32_t memoryBytes;      // allocated bytes
};
```

Array of 6 slots, one per track (kick, snare, ch, oh, tone, metal).

**FmtChunk struct (WAV format):**

```cpp
struct FmtChunk {
  uint16_t audioFormat;      // 1 = PCM
  uint16_t numChannels;      // 1 = mono, 2 = stereo
  uint32_t sampleRate;       // e.g., 44100
  uint32_t byteRate;
  uint16_t blockAlign;
  uint16_t bitsPerSample;    // 16 or 24
};
```

Parsed from WAV file; must match 44.1 kHz or sample rate conversion is needed.

**Fallback samples:**

Pre-allocated procedural samples (sine wave, sawtooth) used when:
- WAV file is missing
- File is corrupted
- File is too large for available memory
- Parse error occurs

---

## Key Internal Functions

**sampleManagerInit():**

```
1. Create SPIClass(SD_SPI_HOST) for SD card
2. Initialize SD.begin(...) with pin config
3. Check card-detect pin if configured
4. Log SD status to boot log
5. Load active sample set name from NVS
6. Call loadSampleSet(activeName)
7. Boot log each sample load (success or fallback)
```

**parseWavLayoutFromFile(File handle) → FmtChunk:**

Reads WAV header:
- RIFF chunk (file type validation)
- fmt chunk (audio format)
- Validates sample rate = 44.1 kHz
- Returns format info or error status

**readMonoSampleFromFile(File handle, FmtChunk fmt) → int16_t[]:**

Decodes WAV samples:
- Handles mono and stereo input (mixes stereo to mono)
- Converts 16-bit or 24-bit PCM to int16_t
- Allocates buffer (PSRAM first via `heap_caps_malloc`)
- Normalizes 24-bit data (shifts to 16-bit range)
- Returns sample buffer or NULL on error

**loadSampleFromSdPath(const char* path) → SampleSlot:**

```
1. Open file from SD (/samples/S1/kick.wav, etc.)
2. Parse WAV layout
3. Allocate and decode samples
4. Load per-sample gain from setGain.json
5. Return SampleSlot or fallback if error
```

**buildFallbackSample() → int16_t[]:**

Generates a sine wave or other waveform in RAM. Used when:
- Sample file missing
- Memory exhausted
- Parse error

Keeps device playable even if SD card issues occur.

**loadSampleGainPercent(const char* setName) → uint8_t[6]:**

```json
{
  "setGain": {
    "kick": 100,
    "snare": 90,
    "ch": 70,
    "oh": 80,
    "tone": 100,
    "metal": 85
  }
}
```

Parses `setGain.json` with ArduinoJson. Missing entries default to 100.

---

## Public Functions

### `sampleManagerInit()`

**Purpose:** Initialize SD card and load active sample set.

**Actions:**

1. Create dedicated SPI bus: `SPIClass(SD_SPI_HOST)`
2. Call `SD.begin(PIN_SD_CS, sdSpi, ...)`
3. Check card-detect pin if `PIN_SD_DTCT` configured
4. Load active sample set name from NVS
5. Call `loadSampleSet(activeName)`
6. Log each loaded sample to boot log

**Call during:** `setup()`, immediately after display init

**Returns:** true if SD initialized; false if not ready (but device can still use fallback samples)

### `sampleManagerIsSdCardReady()`

**Purpose:** Check if SD card mount was successful.

**Returns:** true if `SD.begin()` succeeded and card is present

Used by UI to show SD status indicator and enable/disable card storage menus.

### `sampleManagerIsSdCardInserted()`

**Purpose:** Check if SD card is physically inserted.

**Returns:** true if card-detect pin reads high (or always true if detection not configured)

Used to prevent stale SD operation attempts.

### `sampleManagerListSampleSets() → String[]`

**Purpose:** Enumerate available sample sets on SD card.

**Returns:** Array of directory names (S1, S2, ..., S9)

Used by UI sample-set menu.

### `sampleManagerLoadSampleSet(const char* setName)`

**Purpose:** Load a new sample set at runtime (e.g., switching from S1 to S2).

**Actions:**

1. Stop audio playback (if needed)
2. Free old sample buffers
3. Load new samples from `/samples/<setName>/*`
4. Update active sample set name in NVS
5. Update `sampleGainPercent[]` from new `setGain.json`
6. Log success or fallback errors

**Non-blocking approach preferred:** May allocate large buffers; consider deferring to SystemTask if full UI blocking is unacceptable.

### `sampleManagerSetActiveSampleSet(const char* name)`

**Purpose:** Update the active sample set name persistent in NVS.

Used after successful `loadSampleSet()` to ensure next boot resumes the correct set.

### `sampleManagerGetActiveSampleSet() → const char*`

**Purpose:** Return the active sample set name.

**Returns:** Pointer to static string (e.g., "S1") or NULL if not loaded

### `sampleManagerGetSampleGainPercent(SampleId id) → uint8_t`

**Purpose:** Retrieve per-sample gain multiplier (0–150 %).

Loaded from `setGain.json`. Used by audioEngine before passing sample to voice.

### `sampleManagerGetSample(SampleId id) → const SampleSlot&`

**Purpose:** Retrieve fully decoded sample.

**Returns:** SampleSlot with data buffer, frame count, gain, and generated flag

Used by audioEngine when triggering a sample.

### `sampleManagerGetSampleForTrack(uint8_t trackIndex) → const SampleSlot&`

**Purpose:** Map track 0–5 to a sample (kick, snare, ch, oh, tone, metal).

**Returns:** SampleSlot for the mapped sample

Convenience for sequencer/UI layer.

---

## Memory Management

**PSRAM preference:**

```cpp
int16_t* buffer = (int16_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
if (!buffer) {
  buffer = (int16_t*)malloc(bytes);  // fallback to internal RAM
}
```

Large samples (e.g., 2 MB kick) use PSRAM. Small samples may fit in internal RAM.

**Fallback generation:**

If memory exhausted or WAV invalid, generate a short tone instead of NULL. Device remains playable but with reduced sample fidelity.

---

## SD Card Layout

```
/patterns
  /DEMO
    p01.json
    p02.json

/samples
  /S1
    kick.wav
    snare.wav
    ch.wav
    oh.wav
    tone.wav
    metal.wav
    setGain.json
  /S2
    ...
```

Sample sets must be named S1–S9. Missing samples are automatically fallback-generated.

---

## WAV Requirements

**Format:**

- Container: RIFF WAV
- Codec: PCM (uncompressed)
- Sample rate: 44.1 kHz (required)
- Bit depth: 16-bit or 24-bit
- Channels: mono or stereo

**Parsing:**

- Samples are converted to **mono int16_t** internally
- Stereo is mixed to mono (L/R average)
- 24-bit samples are bit-shifted to 16-bit range
- Non-44.1 kHz samples require manual PCM resampling (not currently automatic)

---

## Dependencies

- `SD` library (native ESP32 filesystem API)
- `SPI` (dedicated bus via SPIClass)
- `ArduinoJson` — setGain.json parsing
- `esp_heap_caps` — PSRAM-aware allocation
- `settingsStore.h` — NVS for active sample set name
- `DisplayDriverClass.h` — boot log output

---

## Important Implementation Notes

1. **Dedicated SPI bus:** Do not use global `SPI` object. Create a dedicated `SPIClass(SD_SPI_HOST)` to avoid contention with TFT.

2. **No SD calls from audioTask():** Sample loading is blocking. Must happen in `setup()` or SystemTask.

3. **Card-detect pin:** If `PIN_SD_DTCT` is configured, monitor it before SD operations. Prevents stale mount attempts after card removal.

4. **Fallback robustness:** Missing or corrupted samples should not crash the device. Fallback generation keeps playback alive.

5. **PSRAM configuration:** Ensure `board_build.psram = enabled` and `-DBOARD_HAS_PSRAM` in platformio.ini for large sample sets.

6. **WAV parsing strictness:** Reject files with unexpected sample rates, bit depths, or corrupted headers. Log errors clearly for diagnostics.

---

[⬆ UP](developerBuildGuide.md#20-source-file-reference) | [📖 README](../README.md#)
