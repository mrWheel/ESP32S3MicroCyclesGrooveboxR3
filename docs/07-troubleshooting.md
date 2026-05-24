# 7. Troubleshooting

## 7.1 No Sound

Check:

- DAC wiring for BCLK/WS/DOUT
- DAC power and amplifier path
- `NO_DAC_HARDWARE` flag is disabled when real audio hardware is connected
- Sample files are present in LittleFS (`/samples/*.wav`)

## 7.2 Distorted or Incorrect Samples

Re-export WAV files as:

- PCM
- 16-bit or 24-bit
- 22050 or 44100 Hz
- mono preferred

Avoid unsupported compressed WAV variants.

## 7.3 WiFi Portal Not Appearing

Verify:

- Open `System Settings` and run `Start WiFi Manager`
- Confirm `Yes` in confirmation prompt
- Connect to the AP shown on screen (`<base>-xxyyzz`)
- Enter credentials and wait for auto-restart

## 7.4 Settings Not Persisting

Persistence requires LittleFS mount success.

If settings reset every boot:

- Re-upload filesystem image
- Check flash partition config (`partitions_4MB_littlefs.csv`)

## 7.5 Display Orientation Wrong

Use:

- `System Settings -> Rotate Display`

The firmware toggles between rotation 1 and 3 and saves this value.

## 7.6 Controls Feel Reversed

Use:

- `System Settings -> Encoder Order`

This toggles A-B / B-A and persists the choice.
