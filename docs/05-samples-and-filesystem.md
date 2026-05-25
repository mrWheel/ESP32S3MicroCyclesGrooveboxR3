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
- Stored pattern data includes step velocity, probability, lock data, and chain settings.

## If a Sample Sounds Wrong

- First test with `TEST_TONE` to verify audio wiring path.
- Then disable `TEST_TONE` again for normal drum playback.
- Lower `AUDIO_MASTER_GAIN_PERCENT` if output is clipping or harsh.

## SD Smoke Test (Diagnostics Path)

Use `SD_SMOKE_TEST` only for SD diagnostics:

- It runs isolated SD checks at boot.
- It prints mount attempts, root listing, and required sample file presence.
- Firmware intentionally halts after diagnostics.

Always disable `SD_SMOKE_TEST` after troubleshooting and rebuild normal firmware.
