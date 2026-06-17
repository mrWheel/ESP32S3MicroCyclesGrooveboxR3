# `src/sampleManager.cpp`

[Back](developerBuildGuide.md)

---

## Purpose

This file owns SD card initialization, sample set selection, WAV decoding, PSRAM/internal RAM sample allocation, fallback samples, SD card detect handling and `setGain.json` loading.

---

## Responsibilities

```text
initialize SD card on dedicated SPI bus
check SD card detect pin
load active sample set name
list available sample sets
load WAV files from /samples/Sn
convert WAV data to mono int16 samples
prefer PSRAM allocation
fall back to generated samples
load per-sample gain percentages
expose SampleSlot objects to audio engine
```

---

## Important Implementation Notes

- Do not use global `SPI`; SD uses `sdSpi`.
- Do not assume SD card is inserted; use `sampleManagerIsSdCardInserted()`.
- PSRAM must be configured correctly for large sample sets.
- Missing/invalid samples are not fatal, but should be visible in boot diagnostics.
- Keep WAV parsing strict enough to avoid undefined audio data.

---

## Important Internal Areas

```text
sdSpi(SD_SPI_HOST) dedicated bus
PIN_SD_DTCT optional card-detect guard
activeSampleSet
sampleGainPercent[]
fallbackSamples[]
parseWavLayoutFromFile()
loadSampleFromSdPath()
loadSampleGainPercent()
boot log integration for sample loading and fallback errors
```

---

## Public Functions

### `sampleManagerInit()`

Initializes SD and loads the active sample set.

### `sampleManagerIsSdCardReady()`

Returns true when SD was mounted successfully.

### `sampleManagerIsSdCardInserted()`

Returns card presence based on the optional detect pin.

### `sampleManagerListSampleSets(...)`

Lists available sample set directories such as S1, S2 and S3.

### `sampleManagerLoadSampleSet(const char*)`

Loads another sample set at runtime.

### `sampleManagerSetActiveSampleSet(const char*)`

Sets the active sample set name.

### `sampleManagerGetActiveSampleSet()`

Returns active sample set name.

### `sampleManagerGetSampleGainPercent(SampleId)`

Returns per-sample gain loaded from setGain.json.

### `sampleManagerGetSample(SampleId)`

Returns one decoded sample slot.

### `sampleManagerGetSampleForTrack(uint8_t)`

Returns the sample slot mapped to a track index.


---

[UP](developerBuildGuide.md) | [README](../README.md)
