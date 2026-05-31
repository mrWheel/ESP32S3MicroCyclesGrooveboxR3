# Code Review - ESP32MicroCyclesGroovebox main branch

Repository:

- https://github.com/mrWheel/ESP32MicroCyclesGroovebox.git
- Branch: main
- Review date: 2026-05-30

Focus requested by user:

When the Groovebox is in RUN status and the user requests STOP:

The firmware must NOT finish the whole current chain and then stop.

Correct behavior:

1. Finish only the pattern that is currently running.
2. Then play the pattern with the highest existing sequence number.
3. Then stop.

Example:

If the chain is p01 -> p02 -> p03 -> p04 and STOP is requested while p02 is running:

- finish p02
- then play p04
- then stop

If STOP is requested while p04 is already running:

- finish p04
- stop

This applies even if the highest pattern is not explicitly referenced as the next item in the chain.

---

# Review Scope

Reviewed public main branch source views through GitHub.

Key files inspected:

- src/sequencer.cpp
- src/sampleManager.cpp
- src/audioEngine.cpp
- src/settingsStore.cpp
- src/uiManager.cpp
- src/main.cpp
- include/sampleManager.h
- include/audioEngine.h
- include/sequencer.h

Important limitation:

I did not run a local PlatformIO build because this environment could not clone GitHub directly. This is a static review.

---

# Executive Summary

The main branch has improved substantially since the previous review.

Most important improvement:

- sampleManager.cpp now contains a real `loadSampleFromSdPath()` implementation.
- WAV files are parsed from SD, decoded to mono int16, and allocated in PSRAM if available.
- fallback samples are preserved if a WAV is missing or invalid.

This means the earlier issue "samples are detected but not actually loaded" appears to be mostly resolved.

Main remaining issues:

1. STOP behavior does not match the corrected musical requirement.
2. sampleManagerSetActiveSampleSet() changes the active set name and gain JSON, but does not reload sample buffers.
3. audioEngine has a fixed 8-voice pool, but still lacks real release fades, choke groups, panning, and proper voice stealing.
4. sequencer.cpp still uses a simple chainLength-based internal memory model, while the desired storage model is Local `/patterns/p01..p99`.
5. settingsStore.cpp still contains mixed old and new pattern naming logic.
6. uiManager.cpp and settingsStore.cpp are becoming very large multi-responsibility modules.
7. Several source files are minified or collapsed into very long physical lines, making review and Copilot edits risky.

---

# Priority 1 - STOP / Chain Semantics

## Finding 1.1: Current transport toggle is immediate

File:

- src/sequencer.cpp

Observed behavior:

- `sequencerTogglePlay()` toggles `state.playing`.
- When starting, it clears `nextStepDueUs`.
- There is no explicit stop-request state.
- There is no deferred stop behavior.
- There is no "finish current pattern, then play highest pNN, then stop" behavior.

Observed code behavior:

```cpp
state.playing = !state.playing;
if (state.playing)
{
  state.nextStepDueUs = 0;
}
```

Impact:

The current transport model cannot implement the desired musical STOP behavior reliably.

Required behavior:

When STOP is requested during RUN:

- do not stop immediately
- finish the currently running pattern
- then jump to the highest existing pattern pNN
- play that highest pNN once
- stop after that pattern completes

This is different from:

- finishing the whole current chain
- stopping immediately
- waiting until p01
- simply toggling state.playing

Recommendation:

Introduce an explicit transport state.

Suggested enum:

```cpp
enum TransportState
{
  transportStopped,
  transportRunning,
  transportStopRequested,
  transportPlayingFinalPattern
};
```

Add runtime fields:

```cpp
TransportState transportState;
uint8_t stopRequestedAtPatternIndex;
uint8_t finalStopPatternIndex;
bool finalPatternStarted;
```

Better names can be used, but the state distinction is important.

---

## Finding 1.2: Need a highest-existing-pattern query

The sequencer currently uses `chainLength` and memory slots, but the new behavior depends on the highest existing Local pattern.

Required function in storage layer:

```cpp
bool settingsStoreFindHighestLocalPatternIndex(uint8_t& outPatternIndex);
```

or, preferably after a storage refactor:

```cpp
bool patternStorageFindHighestLocalPatternIndex(uint8_t& outPatternIndex);
```

Definition:

- scan Local `/patterns/`
- find valid pNN files
- ignore p00
- return highest NN
- convert to zero-based runtime index if sequencer uses zero-based slots

Example:

Files:

```text
p01
p02
p04
```

Highest existing pattern is:

```text
p04
```

Even if chain metadata only references p01 -> p02, STOP after p02 must play p04.

---

## Finding 1.3: Sequencer currently advances chain by chainLength only

File:

- src/sequencer.cpp

Observed behavior:

```cpp
if (state.currentStep == 0 && state.chainEnabled && state.chainLength > 1U)
{
  state.activePatternIndex = (state.activePatternIndex + 1U) % state.chainLength;
}
```

Impact:

This assumes a simple contiguous in-memory chain from 0 to chainLength - 1.

That may be acceptable for basic chain playback, but it is not enough for the corrected STOP rule.

Recommendation:

At end-of-pattern boundary, apply this logic:

Pseudo-behavior:

1. If transportState == transportRunning:
   - advance normally according to chain rules.

2. If transportState == transportStopRequested:
   - the just-finished pattern is the pattern that was running when STOP was requested, or the current running pattern if no separate marker is needed.
   - set activePatternIndex to highest existing pattern.
   - set transportState = transportPlayingFinalPattern.
   - reset currentStep to 0.

3. If transportState == transportPlayingFinalPattern:
   - stop playback.
   - set transportState = transportStopped.
   - set playing = false.
   - reset or keep activePatternIndex according to UI preference.

Acceptance example:

- existing Local patterns: p01, p02, p03, p04
- currently running p02
- user presses STOP
- p02 finishes its 16 steps
- sequencer jumps to p04
- p04 plays one complete cycle
- transport stops

---

## Concrete Developer Task: Implement corrected STOP behavior

Give Developer/Copilot this task:

Implement deferred STOP behavior for Groovebox transport.

Current behavior is too simple: STOP toggles playback immediately.

Required behavior:

When KEY0 short is pressed in STOP:
- start playback from p01
- set status RUN
- currentStep = 0
- active pattern = p01

When KEY0 short is pressed in RUN:
- do not stop immediately
- mark stop requested
- finish only the currently running pattern
- after that pattern completes, jump to the highest existing Local pattern pNN
- play that pNN once
- then stop
- set status STOP

Important:
- Do NOT finish the entire chain before stopping.
- Do NOT stop immediately.
- Do NOT assume the final pattern is referenced by the current chain.
- The final pattern is the highest existing Local pNN.
- If the currently running pattern already is the highest pNN, finish it and stop.

Add a small transport state machine:

- STOPPED
- RUNNING
- STOP_REQUESTED
- PLAYING_FINAL_PATTERN

Add a helper in the storage layer:

- findHighestLocalPatternIndex()

This helper must scan Local /patterns/ for valid pNN patterns and return the highest NN.

Do not perform filesystem scanning inside the audio task.

Recommended implementation:
- UI/transport code requests stop.
- System/storage context determines highest pattern.
- Sequencer stores finalStopPatternIndex.
- Audio/sequencer timing only consumes the already known index.

Do not rewrite unrelated sequencer logic.

Acceptance tests:
1. Existing p01,p02,p03,p04; running p02; STOP pressed: finish p02, play p04, then stop.
2. Existing p01,p02,p04; running p02; STOP pressed: finish p02, play p04, then stop.
3. Existing p01 only; running p01; STOP pressed: finish p01, stop.
4. STOP pressed in STOP: start from p01.
5. No filesystem access occurs inside audio task.
6. UI status shows STOP only after final pattern completes.

---

# Priority 2 - Sample Set Loading

## Finding 2.1: SD WAV loader is now present

File:

- src/sampleManager.cpp

Positive finding:

`loadSampleFromSdPath()` exists and now:

- opens WAV file
- parses RIFF/WAVE chunks
- validates PCM WAV
- accepts 44.1 kHz
- accepts 16/24-bit
- accepts mono/stereo
- converts to mono int16
- allocates in PSRAM first
- uses fallback on failure

This addresses the earlier critical blocker.

---

## Finding 2.2: Runtime sample-set selection still does not reload sample buffers

File:

- src/sampleManager.cpp

Observed behavior:

`sampleManagerSetActiveSampleSet()`:

- validates set name
- copies set name into `activeSampleSet`
- loads `sampleGainPercent.json`
- returns true

It does not:

- unload old sample buffers
- load new WAV files
- restart the system
- guarantee the new set is audible

Impact:

Selecting S2 at runtime may only change the name and gains, not the sample audio.

Recommendation:

Choose one of two models.

### Safer model: reboot after sample-set selection

When sample set is selected:

1. STOP transport.
2. Save selected set to NVS.
3. Show "Restarting..".
4. Call `esp_restart()`.
5. On boot, read NVS sample set and load that set.

This is simplest and safest.

### More advanced model: hot reload

When sample set is selected:

1. STOP transport.
2. Disable new triggers.
3. Clear active voices.
4. Free old SD sample buffers.
5. Load new sample set.
6. Return in STOP state.

Hot reload must never happen from audio task.

Given the current architecture, I recommend the reboot model first.

---

## Finding 2.3: Active sample set from NVS may not be connected to sampleManagerInit()

Files:

- src/settingsStore.cpp
- src/sampleManager.cpp
- src/main.cpp

Observed:

- `settingsStoreGetActiveSampleSet()` exists and returns "S1" by default.
- `sampleManager.cpp` has its own static `activeSampleSet = "S1"`.
- It is not clear from the reviewed source that main reads NVS before `sampleManagerInit()`.

Recommendation:

In startup:

1. initialize NVS/settings
2. read active sample set
3. call `sampleManagerSetActiveSampleSet(activeSet.c_str())`
4. call `sampleManagerInit()`

If that ordering already exists, document it clearly in the boot log.

Expected log:

```text
Info: Active sample set from NVS: S2
Info: Loading sample kick from /samples/S2/kick.wav
```

---

# Priority 3 - Audio Engine / Phase 4 Quality

## Finding 3.1: Voice pool exists but phase-4 behavior is incomplete

File:

- src/audioEngine.cpp

Positive finding:

- `MAX_VOICES = 8`
- fixed `Voice voices[MAX_VOICES]`
- new trigger signature includes gain, pan, chokeGroup, release fields

Missing behavior:

- pan is stored but not used
- gain field is stored but not used
- chokeGroup is stored but not used
- releaseActive and releaseCounter are stored but not used
- voice stealing still falls back to voice 0
- mixer output is still mono duplicated

Impact:

The data model started Phase 4, but the actual audio behavior remains closer to basic polyphonic mono playback.

Recommendation:

Implement in this order:

1. voice stealing policy
2. release fade
3. choke group behavior
4. pan application
5. stereo output
6. optional limiter refinement

---

## Finding 3.2: Voice stealing always uses voice 0 when all voices are busy

File:

- src/audioEngine.cpp

Observed:

```cpp
if (selectedVoice < 0)
{
  selectedVoice = 0;
}
```

Impact:

Voice 0 is hard-cut repeatedly under load.

Recommendation:

Choose oldest or quietest active voice.

Add:

```cpp
uint32_t age;
```

or:

```cpp
uint32_t triggerCounter;
```

Then select oldest.

If implementing release fade first, prefer stealing voices already in release.

---

## Finding 3.3: Choke groups are not applied

File:

- src/audioEngine.cpp

Observed:

- trigger accepts `chokeGroup`
- stores it in voice
- mixer does not act on it
- trigger function does not fade voices from the same group

Recommendation:

Before assigning the new voice:

```cpp
if (chokeGroup != 0)
{
  audioEngineReleaseVoicesInChokeGroup(chokeGroup);
}
```

Suggested mapping:

- CH and OH: group 1
- ride/cymbal future: group 2

Use release fade, not hard stop.

---

## Finding 3.4: Stereo pan is stored but ignored

File:

- src/audioEngine.cpp

Observed:

- `voice.pan` exists
- outputBuffer writes same mono value to L/R

Recommendation:

Change mixer from:

```cpp
int16_t monoSample = mixNextFrame(hadVoices);
left = monoSample;
right = monoSample;
```

to:

```cpp
mixNextFrame(int32_t& outLeft, int32_t& outRight, bool& hadVoices);
```

Use fixed-point pan gains.

---

# Priority 4 - Storage Model Drift

## Finding 4.1: README/current scope still mentions A01..H99 / I01..Z99 model

Repository README currently says:

- A01..H99 always Local
- I01..Z99 always SD card

But the newer design says:

- Local working patterns are `/patterns/p01..p99`
- Card groups are `/patterns/<Name>/p01..p99`
- sample sets are `/samples/S1..S9`

Impact:

Developer/Copilot may implement against the wrong model.

Recommendation:

Update developer documentation and code comments so there is only one current storage model.

---

## Finding 4.2: settingsStore.cpp contains mixed old and new pattern logic

File:

- src/settingsStore.cpp

Observed:

- `buildPatternPath()` enforces pNN.
- `settingsStoreListPatterns()` lists pNN.
- But there are still functions for letter/number pattern names and same-series logic.
- There is still SD pattern logic expecting flat names in `/patterns`.

Impact:

High risk of storage bugs.

Recommendation:

Split storage code into:

```text
settingsStore.cpp       NVS + runtime settings only
patternStorage.cpp      Local/Card pattern filesystem
patternJson.cpp         JSON parse/build
sampleSetStorage.cpp    sample-set metadata
```

Do not refactor all at once. Start by extracting pNN and group-based functions.

---

# Priority 5 - UI / Popup Infrastructure

## Finding 5.1: uiManager.cpp is too broad

File:

- src/uiManager.cpp

Observed responsibility mix:

- main groovebox UI
- system menu
- storage operations
- chain UI
- popup logic
- input handling
- pattern list logic

Impact:

Copilot is likely to damage unrelated behavior when implementing a new feature.

Recommendation:

Extract only safe boundaries first:

```text
uiFooter.cpp
uiTransport.cpp
uiStorageMenus.cpp
uiEditPopup.cpp
```

Do not change all behavior while extracting.

---

## Finding 5.2: Existing DisplayDriver popup/list helpers should be reused

Files:

- src/DisplayDriverClass.cpp
- src/uiManager.cpp

Recommendation:

Every new storage/menu/popup task should explicitly say:

- use existing DisplayDriverClass list/input/status helpers
- do not create another popup framework
- extend DisplayDriver only if a generic helper is missing

This is especially important for the storage restructure and sample set selector.

---

# Priority 6 - Formatting / Build Safety

## Finding 6.1: Source files appear minified into very long physical lines

Many source files appear with many functions on one line in raw GitHub view.

Impact:

- difficult review
- bad diffs
- higher Copilot damage risk
- hard to apply codingRules.md consistently

Recommendation:

Make a formatting-only commit before major restructuring.

Suggested:

```bash
pio run -e ESP32GrooveboxR3
clang-format -i src/*.cpp include/*.h
pio run -e ESP32GrooveboxR3
```

Only do this if `.clang-format` matches your coding style.

Commit message:

```text
Format source files only
```

Then continue feature work.

---

# Recommended Work Order

## Sprint 1: Correct transport behavior

1. Add transport state machine.
2. Add deferred STOP request.
3. Add highest-existing Local pNN lookup.
4. Implement "finish current pattern, play highest pNN, stop".
5. Add logs for transport transitions.

## Sprint 2: Finish sample-set behavior

1. Ensure boot loads active sample set from NVS.
2. Make runtime sample-set selection either reboot safely or hot-reload safely.
3. Log selected sample set clearly.
4. Add acceptance tests.

## Sprint 3: Make Phase 4 audio real

1. voice stealing
2. release fades
3. choke groups
4. stereo panning
5. optional limiter tuning

## Sprint 4: Storage cleanup

1. Decide final storage model.
2. Remove/retire old A01/Z99 assumptions.
3. Extract patternStorage.
4. Update docs.

---

# Developer Task - Correct STOP Behavior

Use this direct task for Copilot/Developer:

Implement corrected Groovebox STOP behavior.

Current behavior toggles transport immediately. This is wrong.

Required behavior:

When KEY0 short is pressed while STOPPED:
- start playback from p01
- currentStep = 0
- status = RUN

When KEY0 short is pressed while RUN:
- do not stop immediately
- do not finish the entire chain
- request stop
- finish only the pattern currently running
- then play the highest existing Local pattern pNN once
- stop after that pNN finishes
- status becomes STOP only after final pNN completes

Examples:

Existing Local patterns:
p01, p02, p03, p04

If STOP is requested while p02 runs:
- finish p02
- play p04
- stop

If STOP is requested while p04 runs:
- finish p04
- stop

Existing Local patterns:
p01 only

If STOP is requested while p01 runs:
- finish p01
- stop

Implementation notes:

- Add explicit transport states:
  - STOPPED
  - RUNNING
  - STOP_REQUESTED
  - PLAYING_FINAL_PATTERN
- Do not scan filesystem in AudioTask.
- Determine highest pNN from UI/system/storage context.
- Pass the final pattern index into sequencer state before audio timing needs it.
- Do not change I2S/audio engine timing.
- Do not rewrite unrelated UI or storage code.

Acceptance tests:
1. STOP in RUN during p02 with p04 highest: finish p02, play p04, stop.
2. STOP in RUN during p04 with p04 highest: finish p04, stop.
3. STOP in STOP starts p01.
4. Status remains RUN until final pattern finishes.
5. Status becomes STOP only after final pattern completion.
6. No filesystem access from AudioTask.
7. Chain playback still works normally when no stop is requested.

---

# Developer Task - Finish Sample Set Boot/Runtime Loading

Use this direct task for Copilot/Developer:

Ensure active sample set is actually loaded on boot and when selected.

Required behavior:

On boot:
- read active sample set from NVS
- default to S1
- call sampleManagerSetActiveSampleSet()
- then sampleManagerInit()
- load samples from /samples/<activeSet>/*.wav

When user selects a new sample set:
- stop transport
- save new sample set to NVS
- either hot-reload samples safely OR show Restarting.. and call esp_restart()
- after reboot, the selected sample set must be audible

Acceptance tests:
1. NVS sample_set=S2 loads /samples/S2/*.wav on boot.
2. Selecting S3 stores S3 in NVS.
3. Reboot loads /samples/S3/*.wav.
4. Missing one sample falls back only for that slot.
5. sampleGainPercent.json for selected set is loaded.
6. No sample reload happens in AudioTask.

---

# Overall Assessment

The main branch is progressing well and now contains important parts of Phase 4:

- real SD WAV loading
- PSRAM sample buffers
- 8-voice pool
- soft limiter scaffold
- sample gain percent support

The biggest current mismatch is now transport behavior.

Correcting the STOP behavior is important because it affects musical usability directly.

After that, finish sample-set selection and make the voice pool truly musical with release fades, choke groups and stereo panning.
