# 2. Hardware and Wiring (R3)

## R3 Pin Mapping Used by Firmware

### Display

- Backlight: GPIO2
- Reset: GPIO4
- CS: GPIO5
- SCL: GPIO18
- SDA: GPIO23
- D/C: GPIO15

### Controls

- Encoder button: GPIO35
- KEY0: GPIO0
- Encoder A: GPIO36
- Encoder B: GPIO39

### Audio (I2S)

- BCLK: GPIO26
- WS/LRCLK: GPIO25
- DOUT: GPIO27
- SD/EN (amplifier enable): GPIO14

## Audio Wiring Checklist

1. Connect ESP32 BCLK to DAC BCLK.
2. Connect ESP32 WS/LRCLK to DAC LRC/WS.
3. Connect ESP32 DOUT to DAC DIN.
4. Connect grounds together.
5. Provide stable 3V3/5V power as required by your DAC board.

## If You Hear Nothing

- Check that SD/EN wiring matches your DAC board expectation.
- Verify speaker or amplifier connection.
- Confirm you uploaded the R3 environment firmware.
