# 5. Samples and Patterns

## Samples

The Groovebox reads samples from SD card files:

- Kick
- Snare
- CH
- OH
- Tone
- Metal

Required paths on SD card:

- `/samples/kick.wav`
- `/samples/snare.wav`
- `/samples/ch.wav`
- `/samples/oh.wav`
- `/samples/tone.wav`
- `/samples/metal.wav`

At startup, firmware logs a full SD root listing before sample loading.

If a sample cannot be loaded (missing file, unsupported WAV format, or insufficient memory), firmware uses a fallback waveform for that track.

## Pattern Storage

User patterns are saved in internal flash filesystem.

- Pattern folder: `/patterns`
- Naming format: `Pnnn` (example: `P014`)

## If a Sample Sounds Wrong

- First test with `TEST_TONE` to verify audio wiring path.
- Then disable `TEST_TONE` again for normal drum playback.
- Lower `AUDIO_MASTER_GAIN_PERCENT` if output is clipping or harsh.
