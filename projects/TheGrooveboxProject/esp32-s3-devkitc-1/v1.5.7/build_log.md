# Build log for ESP32S3GrooveboxR3

Generated: 2026-08-12T14:55:13

$ pio run -e ESP32S3GrooveboxR3
Processing ESP32S3GrooveboxR3 (platform: espressif32; board: esp32-s3-devkitc-1; framework: arduino)
--------------------------------------------------------------------------------
Verbose mode can be enabled via `-v, --verbose` option
CONFIGURATION: https://docs.platformio.org/page/boards/espressif32/esp32-s3-devkitc-1.html
PLATFORM: Espressif 32 (6.13.0) > Espressif ESP32-S3-DevKitC-1-N8 (8 MB QD, No PSRAM)
HARDWARE: ESP32S3 240MHz, 320KB RAM, 7.44MB Flash
DEBUG: Current (esp-builtin) On-board (esp-builtin) External (cmsis-dap, esp-bridge, esp-prog, iot-bus-jtag, jlink, minimodule, olimex-arm-usb-ocd, olimex-arm-usb-ocd-h, olimex-arm-usb-tiny-h, olimex-jtag-tiny, tumpa)
PACKAGES: 
 - framework-arduinoespressif32 @ 3.20017.241212+sha.dcc1105b 
 - tool-esptoolpy @ 2.41100.0 (4.11.0) 
 - toolchain-riscv32-esp @ 8.4.0+2021r2-patch5 
 - toolchain-xtensa-esp32s3 @ 8.4.0+2021r2-patch5
LDF: Library Dependency Finder -> https://bit.ly/configure-pio-ldf
LDF Modes: Finder ~ chain, Compatibility ~ soft
Found 39 compatible libraries
Scanning dependencies...
Dependency Graph
|-- Adafruit GFX Library @ 1.12.6
|-- Adafruit ST7735 and ST7789 Library @ 1.11.0
|-- ArduinoJson @ 7.4.3
|-- WiFiManager @ 2.0.17
|-- SPI @ 2.0.0
|-- WiFi @ 2.0.0
|-- LittleFS @ 2.0.0
|-- SD @ 2.0.0
|-- Preferences @ 2.0.0
|-- WebServer @ 2.0.0
Building in debug mode
Retrieving maximum program size .pio.nosync/build/ESP32S3GrooveboxR3/firmware.elf
Checking size .pio.nosync/build/ESP32S3GrooveboxR3/firmware.elf
Advanced Memory Usage is available via "PlatformIO Home > Project Inspect"
RAM:   [===       ]  27.8% (used 91112 bytes from 327680 bytes)
Flash: [==        ]  19.1% (used 1487129 bytes from 7798784 bytes)
========================= [SUCCESS] Took 2.03 seconds =========================
$ pio run -e ESP32S3GrooveboxR3 -t buildfs
Processing ESP32S3GrooveboxR3 (platform: espressif32; board: esp32-s3-devkitc-1; framework: arduino)
--------------------------------------------------------------------------------
Verbose mode can be enabled via `-v, --verbose` option
CONFIGURATION: https://docs.platformio.org/page/boards/espressif32/esp32-s3-devkitc-1.html
PLATFORM: Espressif 32 (6.13.0) > Espressif ESP32-S3-DevKitC-1-N8 (8 MB QD, No PSRAM)
HARDWARE: ESP32S3 240MHz, 320KB RAM, 7.44MB Flash
DEBUG: Current (esp-builtin) On-board (esp-builtin) External (cmsis-dap, esp-bridge, esp-prog, iot-bus-jtag, jlink, minimodule, olimex-arm-usb-ocd, olimex-arm-usb-ocd-h, olimex-arm-usb-tiny-h, olimex-jtag-tiny, tumpa)
PACKAGES: 
 - framework-arduinoespressif32 @ 3.20017.241212+sha.dcc1105b 
 - tool-esptoolpy @ 2.41100.0 (4.11.0) 
 - tool-mklittlefs @ 1.203.210628 (2.3) 
 - toolchain-riscv32-esp @ 8.4.0+2021r2-patch5 
 - toolchain-xtensa-esp32s3 @ 8.4.0+2021r2-patch5
LDF: Library Dependency Finder -> https://bit.ly/configure-pio-ldf
LDF Modes: Finder ~ chain, Compatibility ~ soft
Found 39 compatible libraries
Scanning dependencies...
Dependency Graph
|-- Adafruit GFX Library @ 1.12.6
|-- Adafruit ST7735 and ST7789 Library @ 1.11.0
|-- ArduinoJson @ 7.4.3
|-- WiFiManager @ 2.0.17
|-- SPI @ 2.0.0
|-- WiFi @ 2.0.0
|-- LittleFS @ 2.0.0
|-- SD @ 2.0.0
|-- Preferences @ 2.0.0
|-- WebServer @ 2.0.0
Building in debug mode
Building FS image from 'data' directory to .pio.nosync/build/ESP32S3GrooveboxR3/littlefs.bin
/index.html
/.keep
/style.css
/app.js
========================= [SUCCESS] Took 0.63 seconds =========================
Using partitions source: /Users/willema/tmp/tmpRepos/TheGrooveboxProject/partitions_S3_N8R8.csv

Resolved buildDir: /Users/willema/tmp/tmpRepos/TheGrooveboxProject/.pio.nosync/build/ESP32S3GrooveboxR3
