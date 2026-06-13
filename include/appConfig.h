/*** Last Changed: 2026-06-13 - 14:54 ***/
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <Arduino.h>

//--- Build flag fallback values
#ifndef PIN_TFT_CS
#define PIN_TFT_CS 5
#endif

#ifndef PIN_TFT_DC
#define PIN_TFT_DC 15
#endif

#ifndef PIN_TFT_RST
#define PIN_TFT_RST 4
#endif

#ifndef PIN_TFT_BLK
#define PIN_TFT_BLK 2
#endif

#ifndef PIN_TFT_SCLK
#define PIN_TFT_SCLK 12
#endif

#ifndef PIN_TFT_MOSI
#define PIN_TFT_MOSI 11
#endif

#ifndef PIN_ENC_A
#define PIN_ENC_A 16
#endif

#ifndef PIN_ENC_B
#define PIN_ENC_B 17
#endif

#ifndef PIN_ENC_BTN
#define PIN_ENC_BTN 6
#endif

#ifndef PIN_KEY0
#define PIN_KEY0 1
#endif

#ifndef PIN_SD_CS
#define PIN_SD_CS 13
#endif

#ifndef PIN_SD_SCK
#define PIN_SD_SCK 14
#endif

#ifndef PIN_SD_MISO
#define PIN_SD_MISO 18
#endif

#ifndef PIN_SD_MOSI
#define PIN_SD_MOSI 21
#endif

#ifndef TFT_WIDTH
#define TFT_WIDTH 320
#endif

#ifndef TFT_HEIGHT
#define TFT_HEIGHT 240
#endif

#ifndef DEFAULT_DISPLAY_ROTATION
#define DEFAULT_DISPLAY_ROTATION 3
#endif

#ifndef ENCODER_LONG_PRESS_MS
#define ENCODER_LONG_PRESS_MS 900
#endif

#ifndef ENCODER_SHORT_PRESS_MS
#define ENCODER_SHORT_PRESS_MS 40
#endif

#ifndef ENCODER_MEDIUM_PRESS_MS
#define ENCODER_MEDIUM_PRESS_MS 450
#endif

#ifndef BUTTON_LONG_PRESS_MS
#define BUTTON_LONG_PRESS_MS 900
#endif

#ifndef BUTTON_SHORT_PRESS_MS
#define BUTTON_SHORT_PRESS_MS 40
#endif

#ifndef BUTTON_MEDIUM_PRESS_MS
#define BUTTON_MEDIUM_PRESS_MS 450
#endif

#ifndef DEFAULT_AP_SSID
#define DEFAULT_AP_SSID "UGroovebox"
#endif

#ifndef DEFAULT_AP_PASSWORD
#define DEFAULT_AP_PASSWORD ""
#endif

#ifndef DEFAULT_WIFI_HOSTNAME
#define DEFAULT_WIFI_HOSTNAME "groovebox"
#endif

#endif
