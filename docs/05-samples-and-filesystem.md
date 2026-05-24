# 5. Samples and Filesystem

## 5.1 Required Sample Files

The firmware expects the following files in LittleFS:

- `/samples/kick.wav`
- `/samples/snare.wav`
- `/samples/ch.wav`
- `/samples/oh.wav`
- `/samples/tone.wav`
- `/samples/metal.wav`

Repository defaults are provided in `data/samples/`.

## 5.2 Supported WAV Input Formats

Accepted input formats per loader:

- PCM (`audioFormat == 1`)
- Channels: mono or stereo
- Bit depth: 16-bit or 24-bit
- Sample rate: 22050 or 44100 Hz

Internal playback format is mono 16-bit @ 22050 Hz.

If input is 44100 Hz, the loader downsamples by averaging frame pairs.
If input is stereo, channels are mixed to mono.

## 5.3 Fallback Sample Behavior

If a file is missing or invalid:

- A generated fallback waveform is used for that slot
- The groovebox still boots and remains playable

This prevents hard failure when sample files are incomplete.
