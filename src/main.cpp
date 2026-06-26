/*** Last Changed: 2026-06-26 - 16:38 ***/
#include <Arduino.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <LittleFS.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>

#include "DisplayDriverClass.h"
#include "InputClass.h"
#include "audioEngine.h"
#include "sampleManager.h"
#include "sequencer.h"
#include "settingsStore.h"
#include "systemManager.h"
#include "uiManager.h"
#include "webServerManager.h"
#include "appConfig.h"
#include "progVersion.h"

//-- PROG_VERSION.
const char* PROG_VERSION = "v1.5.3";

//-- Logging tag.
static const char* logTag = "Groovebox";

//-- Event message from InputTask to UiTask.
struct InputEventMessage
{
  bool encoderEvent;
  int eventValue;
};

//-- Static input queue resources.
static StaticQueue_t inputQueueStruct;
static uint8_t inputQueueStorage[24 * sizeof(InputEventMessage)];
static QueueHandle_t inputQueue = nullptr;

//-- Task startup status flags.
static bool audioTaskStarted = false;
static bool uiTaskStarted = false;
static bool inputTaskStarted = false;
static bool systemTaskStarted = false;

//-- Log remaining stack space for the currently running task.
static void logCurrentTaskStackHighWaterMark(const char* taskName)
{
  UBaseType_t freeStackWords = uxTaskGetStackHighWaterMark(nullptr);
  size_t freeStackBytes = static_cast<size_t>(freeStackWords) * sizeof(StackType_t);

  ESP_LOGI(logTag, "[Stack] %s free=%lu bytes", taskName,
           static_cast<unsigned long>(freeStackBytes));

} //   logCurrentTaskStackHighWaterMark()

//-- Build absolute child path for recursive filesystem traversal.
static String buildFilesystemChildPath(const char* parentPath, const char* entryName)
{
  String childPath = String(entryName);

  if (childPath.startsWith("/"))
  {
    return childPath;
  }

  if (strcmp(parentPath, "/") == 0)
  {
    return String("/") + childPath;
  }

  return String(parentPath) + "/" + childPath;

} //   buildFilesystemChildPath()

//-- Recursively log directory contents for a filesystem.
static void logFilesystemDirectoryRecursive(fs::FS& filesystem, const char* filesystemName,
                                            const char* directoryPath, uint8_t depth)
{
  File directory = filesystem.open(directoryPath, "r");

  if (!directory)
  {
    ESP_LOGW(logTag, "%s open failed for %s", filesystemName, directoryPath);
    return;
  }

  if (!directory.isDirectory())
  {
    ESP_LOGW(logTag, "%s path is not a directory: %s", filesystemName, directoryPath);
    directory.close();
    return;
  }

  while (true)
  {
    File entry = directory.openNextFile();

    if (!entry)
    {
      break;
    }

    String entryPath = buildFilesystemChildPath(directoryPath, entry.name());
    String indent = "";

    for (uint8_t level = 0; level < depth; level++)
    {
      indent += "  ";
    }

    ESP_LOGI(logTag, "%s%s (%s, %lu bytes)", indent.c_str(), entryPath.c_str(),
             entry.isDirectory() ? "dir" : "file", static_cast<unsigned long>(entry.size()));

    if (entry.isDirectory())
    {
      logFilesystemDirectoryRecursive(filesystem, filesystemName, entryPath.c_str(),
                                      static_cast<uint8_t>(depth + 1));
    }

    entry.close();
  }

  directory.close();

} //   logFilesystemDirectoryRecursive()

//-- Run isolated SD smoke test and stop firmware startup.
#ifdef SD_SMOKE_TEST
static void runSdSmokeTestAndHalt()
{
  static const uint32_t initFrequenciesHz[] = {400000U, 1000000U, 4000000U};
  static SPIClass smokeSdSpi(SD_SPI_HOST);

  ESP_LOGI(logTag, "SD smoke test pins: CS=%d SCK=%d MISO=%d MOSI=%d", PIN_SD_CS, PIN_SD_SCK,
           PIN_SD_MISO, PIN_SD_MOSI);

  pinMode(PIN_TFT_CS, OUTPUT);
  digitalWrite(PIN_TFT_CS, HIGH);

  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);

  pinMode(PIN_SD_MISO, INPUT_PULLUP);

  smokeSdSpi.end();
  smokeSdSpi.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);

  delay(20);

  smokeSdSpi.beginTransaction(SPISettings(400000U, MSBFIRST, SPI_MODE0));

  for (uint8_t dummyIndex = 0; dummyIndex < 16; dummyIndex++)
  {
    (void)smokeSdSpi.transfer(0xFF);
  }

  smokeSdSpi.endTransaction();

  bool mountOk = false;

  for (size_t attemptIndex = 0;
       attemptIndex < (sizeof(initFrequenciesHz) / sizeof(initFrequenciesHz[0])); attemptIndex++)
  {
    uint32_t initFrequency = initFrequenciesHz[attemptIndex];

    ESP_LOGI(logTag, "SD smoke init attempt %u at %luHz", static_cast<unsigned>(attemptIndex + 1),
             static_cast<unsigned long>(initFrequency));

    SD.end();

    if (SD.begin(PIN_SD_CS, smokeSdSpi, initFrequency))
    {
      mountOk = true;
      break;
    }

    delay(8);
  }

  if (!mountOk)
  {
    ESP_LOGE(logTag, "SD smoke test: mount failed");
  }
  else
  {
    ESP_LOGI(logTag, "SD smoke test: mount OK");
  }

  while (true)
  {
    delay(1000);
  }

} //   runSdSmokeTestAndHalt()
#endif

//-- Warn when critical pin assignments overlap.
static void logPinConflictWarnings()
{
  if (PIN_I2S_WS == PIN_TFT_RST || PIN_I2S_WS == PIN_TFT_CS || PIN_I2S_WS == PIN_TFT_DC ||
      PIN_I2S_WS == PIN_TFT_SCLK || PIN_I2S_WS == PIN_TFT_MOSI || PIN_I2S_WS == PIN_TFT_BLK)
  {
    ESP_LOGE(logTag, "Pin conflict: PIN_I2S_WS=%d overlaps TFT pin assignment", PIN_I2S_WS);
  }

  if (PIN_I2S_DOUT == PIN_TFT_RST || PIN_I2S_DOUT == PIN_TFT_CS || PIN_I2S_DOUT == PIN_TFT_DC ||
      PIN_I2S_DOUT == PIN_TFT_SCLK || PIN_I2S_DOUT == PIN_TFT_MOSI || PIN_I2S_DOUT == PIN_TFT_BLK)
  {
    ESP_LOGE(logTag, "Pin conflict: PIN_I2S_DOUT=%d overlaps TFT pin assignment", PIN_I2S_DOUT);
  }

} //   logPinConflictWarnings()

//-- Run one fallback cycle for input and UI when tasks are unavailable.
static void runInputUiFallbackCycle()
{
  input.update();

  EncoderEvent encoderEvent = input.getEncoderEvent();
  if (encoderEvent != ENCODER_EVENT_NONE)
  {
    uiManagerHandleEncoderEvent(encoderEvent);
  }

  ButtonEvent buttonEvent = input.getAuxButtonEvent();
  if (buttonEvent != BUTTON_EVENT_NONE)
  {
    uiManagerHandleAuxButtonEvent(buttonEvent);
  }

  uiManagerUpdate();

} //   runInputUiFallbackCycle()

//-- Audio task: runs on core 0.
static void audioTask(void* parameter)
{
  (void)parameter;

  uint8_t stepIndex = 0;
  uint8_t trackMask = 0;
  uint8_t trackLevels[sequencerTrackCount] = {0};
  uint8_t trackDecays[sequencerTrackCount] = {0};
  int8_t trackPitches[sequencerTrackCount] = {0};
  uint32_t lastStackLogMs = 0;

  logCurrentTaskStackHighWaterMark("AudioTask start");

  for (;;)
  {
    uint64_t nowUs = static_cast<uint64_t>(esp_timer_get_time());

    if (sequencerConsumeDueStep(nowUs, stepIndex, trackMask, trackLevels, trackDecays,
                                trackPitches))
    {
      for (uint8_t trackIndex = 0; trackIndex < sequencerTrackCount; trackIndex++)
      {
        if ((trackMask & static_cast<uint8_t>(1U << trackIndex)) != 0)
        {
          audioEngineTriggerSample(static_cast<SampleId>(trackIndex), trackLevels[trackIndex],
                                   65535, 0, 0, trackDecays[trackIndex], trackPitches[trackIndex]);
        }
      }
    }

    audioEngineRenderBlock();

    if ((millis() - lastStackLogMs) >= 10000)
    {
      lastStackLogMs = millis();
      logCurrentTaskStackHighWaterMark("AudioTask");
    }

    //-- Always yield one tick so IDLE0 can run and task watchdog stays serviced.
    vTaskDelay(pdMS_TO_TICKS(1));
  }

} //   audioTask()

//-- Input task: polls encoder and buttons on core 1.
static void inputTask(void* parameter)
{
  (void)parameter;

  uint32_t lastStackLogMs = 0;

  logCurrentTaskStackHighWaterMark("InputTask start");

  for (;;)
  {
    InputEventMessage message;

    input.update();

    EncoderEvent encoderEvent = input.getEncoderEvent();

    if (encoderEvent != ENCODER_EVENT_NONE)
    {
      message.encoderEvent = true;
      message.eventValue = static_cast<int>(encoderEvent);
      (void)xQueueSend(inputQueue, &message, 0);
    }

    ButtonEvent buttonEvent = input.getAuxButtonEvent();

    if (buttonEvent != BUTTON_EVENT_NONE)
    {
      message.encoderEvent = false;
      message.eventValue = static_cast<int>(buttonEvent);
      (void)xQueueSend(inputQueue, &message, 0);
    }

    if ((millis() - lastStackLogMs) >= 10000)
    {
      lastStackLogMs = millis();
      logCurrentTaskStackHighWaterMark("InputTask");
    }

    vTaskDelay(pdMS_TO_TICKS(5));
  }

} //   inputTask()

//-- UI task: handles events and redraws on core 1.
static void uiTask(void* parameter)
{
  (void)parameter;

  uint32_t lastStackLogMs = 0;

  logCurrentTaskStackHighWaterMark("UiTask start");

  for (;;)
  {
    InputEventMessage message;

    while (xQueueReceive(inputQueue, &message, 0) == pdTRUE)
    {
      if (message.encoderEvent)
      {
        uiManagerHandleEncoderEvent(static_cast<EncoderEvent>(message.eventValue));
      }
      else
      {
        uiManagerHandleAuxButtonEvent(static_cast<ButtonEvent>(message.eventValue));
      }
    }

    uiManagerUpdate();

    if ((millis() - lastStackLogMs) >= 10000)
    {
      lastStackLogMs = millis();
      logCurrentTaskStackHighWaterMark("UiTask");
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }

} //   uiTask()

//-- System task: WiFi manager, web server and command execution on core 1.
static void systemTask(void* parameter)
{
  (void)parameter;

  uint32_t lastStackLogMs = 0;

  logCurrentTaskStackHighWaterMark("SystemTask start");

  for (;;)
  {
    systemManagerUpdate();
    webServerManagerUpdate(systemManagerIsWifiPortalActive());

    if ((millis() - lastStackLogMs) >= 10000)
    {
      lastStackLogMs = millis();
      logCurrentTaskStackHighWaterMark("SystemTask");
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }

} //   systemTask()

//-- Initialize runtime services and tasks.
void setup()
{
  RuntimeSettings runtimeSettings;

  Serial.begin(115200);
  delay(100);

  Serial.printf("Booting ESP32 MicroCycles Groovebox (%s)\n", PROG_VERSION);

  ESP_LOGI(logTag, "Booting ESP32 MicroCycles Groovebox (%s)", PROG_VERSION);
  ESP_LOGI(logTag, "TFT pins: CS=%d DC=%d RST=%d BL=%d SCL=%d SDA=%d", PIN_TFT_CS, PIN_TFT_DC,
           PIN_TFT_RST, PIN_TFT_BLK, PIN_TFT_SCLK, PIN_TFT_MOSI);
  ESP_LOGI(logTag, "SD pins: CS=%d SCK=%d MISO=%d MOSI=%d", PIN_SD_CS, PIN_SD_SCK, PIN_SD_MISO,
           PIN_SD_MOSI);
  ESP_LOGI(logTag, "I2S pins: BCLK=%d WS=%d DOUT=%d", PIN_I2S_BCLK, PIN_I2S_WS, PIN_I2S_DOUT);

  logPinConflictWarnings();

#ifdef SD_SMOKE_TEST
  runSdSmokeTestAndHalt();
  return;
#endif

#ifdef NO_DAC_HARDWARE
  ESP_LOGW(logTag,
           "NO_DAC_HARDWARE is enabled. System remains active; only I2S/DAC hardware is skipped.");
#endif

  settingsStoreLoadRuntimeSettings(runtimeSettings);

  displayInit();
  displaySetRotation(static_cast<int>(runtimeSettings.displayRotation));
  displaySetThemeColorIndex(runtimeSettings.themeColorIndex);

  displayBootLogClear("Groovebox boot");
  displayBootLogInfo(String("Version ") + PROG_VERSION);
  displayBootLogInfo("Display ready");
  displayBootLogInfo("Read samples");

  if (!sampleManagerInit())
  {
    ESP_LOGW(logTag, "Sample manager init failed, using fallback waveforms");
    displayBootLogInfo("Samples failed");
  }
  else
  {
    displayBootLogInfo("Samples ready");
  }

  inputQueue =
      xQueueCreateStatic(24, sizeof(InputEventMessage), inputQueueStorage, &inputQueueStruct);

  input.begin();
  input.setEncoderDirectionReversed(runtimeSettings.encoderDirectionReversed);

  displayBootLogInfo("Input ready");

  ESP_LOGI(logTag, "Loaded settings: rotation=%u theme=%d encoder=%s",
           static_cast<unsigned>(runtimeSettings.displayRotation), runtimeSettings.themeColorIndex,
           runtimeSettings.encoderDirectionReversed ? "B-A" : "A-B");

  displayBootLogInfo("Init sequencer");
  sequencerInit();

  displayBootLogInfo("Init audio");

  if (!audioEngineInit())
  {
    ESP_LOGE(logTag, "Audio engine init failed");
    displayBootLogError("Audio failed");
  }
  else
  {
    displayBootLogInfo("Audio ready");
  }

  displayBootLogInfo("WiFi check");
  systemManagerInit();

  if (WiFi.status() == WL_CONNECTED)
  {
    displayBootLogInfo("WiFi yes");
    displayBootLogInfo(String("AP ") + WiFi.SSID());
  }
  else
  {
    displayBootLogInfo("WiFi no");
  }

  displayBootLogInfo("Init webserver");
  webServerManagerInit();

  displayBootLogInfo("Read patterns");
  uiManagerInit();

  displayBootLogInfo("Open UI");

  webServerManagerUpdate(false);

  //-- Draw first full UI frame directly from setup.
  uiManagerUpdate();

  webServerManagerSetBootLogEnabled(false);

  audioTaskStarted =
      (xTaskCreatePinnedToCore(audioTask, "AudioTask", 8192, nullptr, 3, nullptr, 0) == pdPASS);

  uiTaskStarted =
      (xTaskCreatePinnedToCore(uiTask, "UiTask", 12288, nullptr, 2, nullptr, 1) == pdPASS);

  inputTaskStarted =
      (xTaskCreatePinnedToCore(inputTask, "InputTask", 4096, nullptr, 2, nullptr, 1) == pdPASS);

  systemTaskStarted =
      (xTaskCreatePinnedToCore(systemTask, "SystemTask", 6144, nullptr, 1, nullptr, 1) == pdPASS);

  if (!audioTaskStarted)
  {
    ESP_LOGE(logTag, "AudioTask creation failed");
    displayBootLogError("AudioTask failed");
  }

  if (!uiTaskStarted)
  {
    ESP_LOGE(logTag, "UiTask creation failed");
    displayBootLogError("UiTask failed");
  }

  if (!inputTaskStarted)
  {
    ESP_LOGE(logTag, "InputTask creation failed");
    displayBootLogError("InputTask failed");
  }

  if (!systemTaskStarted)
  {
    ESP_LOGE(logTag, "SystemTask creation failed");
    displayBootLogError("SystemTask failed");
  }

  if (!uiTaskStarted || !inputTaskStarted)
  {
    ESP_LOGW(logTag, "Input/UI fallback is active in loop() because one or more tasks failed");
    displayBootLogError("Fallback active");
  }

} //   setup()

//-- Main loop remains non-blocking and delegated to tasks.
void loop()
{
  if (!inputTaskStarted || !uiTaskStarted)
  {
    runInputUiFallbackCycle();
  }

  if (!systemTaskStarted)
  {
    systemManagerUpdate();
  }

  vTaskDelay(pdMS_TO_TICKS(1000));

} //   loop()