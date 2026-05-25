# 7. Troubleshooting

## No Sound

1. Confirm firmware was uploaded for `ESP32GrooveboxR3`.
2. Check I2S wiring: BCLK, WS/LRCLK, DOUT.
3. Check SD/EN pin behavior for your DAC module.
4. Verify speaker and power wiring.

## Sound Is Very Distorted

1. Lower `AUDIO_MASTER_GAIN_PERCENT` in `platformio.ini`.
2. Confirm DAC input wiring is correct (especially DOUT).
3. Test with `TEST_TONE` to isolate wiring vs sample playback.

## I Hear Only a Sine Tone

- `TEST_TONE` is enabled.
- Disable `-D TEST_TONE` and rebuild/upload for normal drum playback.

## Wi-Fi Setup Does Not Start

1. Open System Settings.
2. Select Start WiFi Manager.
3. Confirm in the dialog.
4. Join the shown access point and submit credentials.

## Pattern Not Found

- Use Load Pattern to refresh the list.
- Create a New Pattern and Save Pattern once.
- Check that flash partition and filesystem are not corrupted.

## A Sample Falls Back On Boot

1. Check boot log for `Out of memory for SD sample` or WAV format warnings.
2. Confirm boot log shows `PSRAM: available` on WROVER builds.
3. Verify the sample file in `/samples` is valid PCM WAV (16-bit or 24-bit, 22050 or 44100 Hz).
4. Shorten or downsample very large WAV files if memory is still insufficient.
