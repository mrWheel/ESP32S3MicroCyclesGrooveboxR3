# 5. Samples and Sequences

## Samples

The Groovebox ships with built-in embedded samples:

- Kick
- Snare
- CH
- OH
- Tone
- Metal

This means:

- No manual sample file upload is required for normal playback.

## Sequence Storage

User sequences are saved in internal flash filesystem.

- Sequence folder: `/sequences`
- Naming format: `Snnn` (example: `S014`)

## If a Sample Sounds Wrong

- First test with `TEST_TONE` to verify audio wiring path.
- Then disable `TEST_TONE` again for normal drum playback.
- Lower `AUDIO_MASTER_GAIN_PERCENT` if output is clipping or harsh.
