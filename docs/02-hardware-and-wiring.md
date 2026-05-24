# 2. Hardware and Wiring

## 2.1 Fixed Pin Mapping

Default mapping from `platformio.ini`:

### Display

- TFT backlight: GPIO13
- TFT reset: GPIO14
- TFT chip select: GPIO18
- TFT SCL: GPIO21
- TFT SDA: GPIO22
- TFT D/C: GPIO23

### Input

- Encoder button: GPIO25
- KEY0: GPIO26
- Encoder A: GPIO32
- Encoder B: GPIO33

### I2S Audio

- BCLK: GPIO27
- WS/LRCLK: GPIO4
- DOUT: GPIO5

## 2.2 Display Format

- Resolution: 320x240 (`TFT_WIDTH`, `TFT_HEIGHT`)
- Supported runtime rotations in menu: 1 and 3

## 2.3 Input Timing Thresholds

Build-flag defaults used in this project:

- Short press: 50 ms
- Medium press: 1000 ms
- Long press: 2000 ms

(Separate thresholds exist for encoder push and KEY0, currently set equally.)

## 2.4 Audio Path

Internal chain:

1. Samples loaded into memory from LittleFS
2. Voices mixed to mono
3. Mono duplicated to stereo
4. 16-bit PCM written to I2S DMA

Engine defaults:

- 22050 Hz output
- Block size: 128 frames
- Voice pool: 16 fixed voices
