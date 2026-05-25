/*** Last Changed: 2026-05-25 - 18:06 ***/
#include <Arduino.h>
#include <esp_log.h>
#include <esp_timer.h>
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
#include "appConfig.h"
#include "progVersion.h"

//-- PROG_VERSION.
const char* PROG_VERSION = "v0.4.3";

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

//-- Boot status scroller state.
static const int bootStatusVisibleLines = 9;
static String bootStatusLines[bootStatusVisibleLines];
static bool bootStatusDisplayReady = false;

//-- Draw the current boot status line buffer.
static void bootStatusDraw()
{
  if (!bootStatusDisplayReady)
  {
    return;
  }

  display.drawListScreen("Startup", bootStatusLines, bootStatusVisibleLines, -1, 0, PROG_VERSION);

} //   bootStatusDraw()

//-- Keep only the tail of long lines so right edge remains visible.
static String compactBootStatusLine(const String& line)
{
  const int maxChars = 26;

  if (line.length() <= maxChars)
  {
    return line;
  }

  return String("...") + line.substring(line.length() - (maxChars - 3));

} //   compactBootStatusLine()

//-- Append one startup status line at the bottom and scroll older lines upward.
static void bootStatusPush(const String& rawLine)
{
  if (!bootStatusDisplayReady)
  {
    return;
  }

  for (int lineIndex = 0; lineIndex < (bootStatusVisibleLines - 1); lineIndex++)
  {
    bootStatusLines[lineIndex] = bootStatusLines[lineIndex + 1];
  }

  bootStatusLines[bootStatusVisibleLines - 1] = compactBootStatusLine(rawLine);
  bootStatusDraw();

} //   bootStatusPush()

//-- Prepare the display for boot status scrolling.
static void bootStatusInit()
{
  for (int lineIndex = 0; lineIndex < bootStatusVisibleLines; lineIndex++)
  {
    bootStatusLines[lineIndex] = "";
  }

  displayInit();
  bootStatusDisplayReady = true;
  bootStatusDraw();

} //   bootStatusInit()

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
static void logFilesystemDirectoryRecursive(fs::FS& filesystem, const char* filesystemName, const char* directoryPath, uint8_t depth)
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

    ESP_LOGI(logTag,
             "%s%s (%s, %lu bytes)",
             indent.c_str(),
             entryPath.c_str(),
             entry.isDirectory() ? "dir" : "file",
             static_cast<unsigned long>(entry.size()));

    if (entry.isDirectory())
    {
      logFilesystemDirectoryRecursive(filesystem, filesystemName, entryPath.c_str(), static_cast<uint8_t>(depth + 1));
    }

    entry.close();
  }

  directory.close();

} //   logFilesystemDirectoryRecursive()

//-- Log filesystem root and all nested entries.
static void logFilesystemRoot(fs::FS& filesystem, const char* filesystemName)
{
  ESP_LOGI(logTag, "%s root listing:", filesystemName);
  logFilesystemDirectoryRecursive(filesystem, filesystemName, "/", 1);

} //   logFilesystemRoot()

//-- Show recursive filesystem listing on the startup display.
static void displayFilesystemDirectoryRecursive(fs::FS& filesystem, const char* directoryPath)
{
  File directory = filesystem.open(directoryPath, "r");

  if (!directory)
  {
    bootStatusPush(String("open failed ") + directoryPath);
    return;
  }

  if (!directory.isDirectory())
  {
    bootStatusPush(String("not a dir ") + directoryPath);
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

    if (entry.isDirectory())
    {
      bootStatusPush(String("SD ") + entryPath + "/");
      displayFilesystemDirectoryRecursive(filesystem, entryPath.c_str());
    }
    else
    {
      bootStatusPush(String("SD ") + entryPath);
    }

    entry.close();
  }

  directory.close();

} //   displayFilesystemDirectoryRecursive()

//-- Run isolated SD smoke test and stop firmware startup.
#ifdef SD_SMOKE_TEST
static void runSdSmokeTestAndHalt()
{
  static const uint32_t initFrequenciesHz[] = {400000U, 1000000U, 4000000U};

  ESP_LOGI(logTag,
           "SD smoke test pins: CS=%d SCK=%d MISO=%d MOSI=%d",
           PIN_SD_CS,
           PIN_SD_SCK,
           PIN_SD_MISO,
           PIN_SD_MOSI);

  pinMode(PIN_TFT_CS, OUTPUT);
  digitalWrite(PIN_TFT_CS, HIGH);
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);
  pinMode(PIN_SD_MISO, INPUT_PULLUP);

  SPI.end();
  SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  delay(20);

  SPI.beginTransaction(SPISettings(400000U, MSBFIRST, SPI_MODE0));

  for (uint8_t dummyIndex = 0; dummyIndex < 16; dummyIndex++)
  {
    (void)SPI.transfer(0xFF);
  }

  SPI.endTransaction();

  bool mountOk = false;

  for (size_t attemptIndex = 0; attemptIndex < (sizeof(initFrequenciesHz) / sizeof(initFrequenciesHz[0])); attemptIndex++)
  {
    uint32_t initFrequency = initFrequenciesHz[attemptIndex];

    ESP_LOGI(logTag,
             "SD smoke init attempt %u at %luHz",
             static_cast<unsigned>(attemptIndex + 1),
             static_cast<unsigned long>(initFrequency));

    SD.end();

    if (SD.begin(PIN_SD_CS, SPI, initFrequency))
    {
      mountOk = true;
      break;
    }

    delay(8);
  }

  if (!mountOk)
  {
    ESP_LOGE(logTag, "SD smoke test: mount failed");

    for (;;)
    {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

  uint8_t cardType = SD.cardType();
  uint64_t cardSizeMb = static_cast<uint64_t>(SD.cardSize()) / (1024ULL * 1024ULL);

  ESP_LOGI(logTag,
           "SD smoke test: mount OK, cardType=%u, cardSizeMB=%llu",
           static_cast<unsigned>(cardType),
           static_cast<unsigned long long>(cardSizeMb));

  File rootDir = SD.open("/");

  if (!rootDir || !rootDir.isDirectory())
  {
    ESP_LOGE(logTag, "SD smoke test: failed to open root directory");
  }
  else
  {
    ESP_LOGI(logTag, "SD smoke test: root directory listing");

    while (true)
    {
      File entry = rootDir.openNextFile();

      if (!entry)
      {
        break;
      }

      ESP_LOGI(logTag,
               "  %s (%s, %lu bytes)",
               entry.name(),
               entry.isDirectory() ? "dir" : "file",
               static_cast<unsigned long>(entry.size()));

      entry.close();
    }
  }

  const char* samplePaths[] = {
      "/samples/kick.wav",
      "/samples/snare.wav",
      "/samples/ch.wav",
      "/samples/oh.wav",
      "/samples/tone.wav",
      "/samples/metal.wav"};

  for (size_t sampleIndex = 0; sampleIndex < (sizeof(samplePaths) / sizeof(samplePaths[0])); sampleIndex++)
  {
    const char* samplePath = samplePaths[sampleIndex];
    File sampleFile = SD.open(samplePath, FILE_READ);

    if (!sampleFile)
    {
      ESP_LOGW(logTag, "SD smoke test: missing %s", samplePath);
      continue;
    }

    ESP_LOGI(logTag,
             "SD smoke test: found %s (%lu bytes)",
             samplePath,
             static_cast<unsigned long>(sampleFile.size()));
    sampleFile.close();
  }

  ESP_LOGI(logTag, "SD smoke test done. Firmware halted.");

  for (;;)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

} //   runSdSmokeTestAndHalt()
#endif

//-- Warn when critical pin assignments overlap.
static void logPinConflictWarnings()
{
  if (PIN_I2S_WS == PIN_TFT_RST || PIN_I2S_WS == PIN_TFT_CS || PIN_I2S_WS == PIN_TFT_DC || PIN_I2S_WS == PIN_TFT_SCL || PIN_I2S_WS == PIN_TFT_SDA || PIN_I2S_WS == PIN_TFT_BL)
  {
    ESP_LOGE(logTag, "Pin conflict: PIN_I2S_WS=%d overlaps TFT pin assignment", PIN_I2S_WS);
  }

  if (PIN_I2S_DOUT == PIN_TFT_RST || PIN_I2S_DOUT == PIN_TFT_CS || PIN_I2S_DOUT == PIN_TFT_DC || PIN_I2S_DOUT == PIN_TFT_SCL || PIN_I2S_DOUT == PIN_TFT_SDA || PIN_I2S_DOUT == PIN_TFT_BL)
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

  for (;;)
  {
    uint64_t nowUs = static_cast<uint64_t>(esp_timer_get_time());

    if (sequencerConsumeDueStep(nowUs, stepIndex, trackMask, trackLevels))
    {
      for (uint8_t trackIndex = 0; trackIndex < sequencerTrackCount; trackIndex++)
      {
        if ((trackMask & static_cast<uint8_t>(1U << trackIndex)) != 0)
        {
          audioEngineTriggerSample(static_cast<SampleId>(trackIndex), trackLevels[trackIndex]);
        }
      }
    }

    audioEngineRenderBlock();

    //-- Always yield one tick so IDLE0 can run and task watchdog stays serviced.
    vTaskDelay(pdMS_TO_TICKS(1));
  }

} //   audioTask()

//-- Input task: polls encoder and buttons on core 1.
static void inputTask(void* parameter)
{
  (void)parameter;

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

    vTaskDelay(pdMS_TO_TICKS(5));
  }

} //   inputTask()

//-- UI task: handles events and redraws on core 1.
static void uiTask(void* parameter)
{
  (void)parameter;

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

    vTaskDelay(pdMS_TO_TICKS(10));
  }

} //   uiTask()

//-- System task: WiFi manager and command execution on core 1.
static void systemTask(void* parameter)
{
  (void)parameter;

  for (;;)
  {
    systemManagerUpdate();
    vTaskDelay(pdMS_TO_TICKS(20));
  }

} //   systemTask()

//-- Initialize runtime services and tasks.
void setup()
{
  RuntimeSettings runtimeSettings;
  String patternNames[32];
  size_t patternCount = 0;

  Serial.begin(115200);
  delay(100);

  Serial.printf("Booting ESP32 MicroCycles Groovebox (%s)\n", PROG_VERSION);
  ESP_LOGI(logTag, "Booting ESP32 MicroCycles Groovebox (%s)", PROG_VERSION);
  ESP_LOGI(logTag, "TFT pins: CS=%d DC=%d RST=%d BL=%d SCL=%d SDA=%d", PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST, PIN_TFT_BL, PIN_TFT_SCL, PIN_TFT_SDA);
  ESP_LOGI(logTag, "I2S pins: BCLK=%d WS=%d DOUT=%d", PIN_I2S_BCLK, PIN_I2S_WS, PIN_I2S_DOUT);
  logPinConflictWarnings();

#ifdef SD_SMOKE_TEST
  runSdSmokeTestAndHalt();
  return;
#endif

#ifdef NO_DAC_HARDWARE
  ESP_LOGW(logTag, "NO_DAC_HARDWARE is enabled. System remains active; only I2S/DAC hardware is skipped.");
#endif

  inputQueue = xQueueCreateStatic(24, sizeof(InputEventMessage), inputQueueStorage, &inputQueueStruct);

  if (!sampleManagerInit())
  {
    ESP_LOGW(logTag, "Sample manager init failed, using fallback waveforms");
  }

  input.begin();

  //-- Keep SD initialization first on shared SPI bus for reliable SD mount.
  bootStatusInit();
  bootStatusPush(String("Boot ") + PROG_VERSION);
  bootStatusPush(sampleManagerIsSdCardReady() ? "Sample manager ready" : "Sample init failed");
  bootStatusPush("Input ready");

  if (sampleManagerIsSdCardReady())
  {
    bootStatusPush("SD listing /");
    displayFilesystemDirectoryRecursive(SD, "/");
  }
  else
  {
    bootStatusPush("SD unavailable");
  }

  for (uint8_t sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++)
  {
    const SampleSlot& slot = sampleManagerGetSample(static_cast<SampleId>(sampleIndex));
    String sampleLine = String("SMP ") + slot.name + " ";

    if (!slot.valid)
    {
      sampleLine += "invalid";
    }
    else if (!slot.fromSd)
    {
      sampleLine += "fallback";
    }
    else
    {
      sampleLine += slot.storedInPsram ? "PSRAM" : "RAM";
    }

    bootStatusPush(sampleLine);
  }

  bootStatusPush("Init sequencer");
  sequencerInit();

  bootStatusPush("Load settings");
  settingsStoreLoadRuntimeSettings(runtimeSettings);
  displaySetRotation(static_cast<int>(runtimeSettings.displayRotation));
  displaySetThemeColorIndex(runtimeSettings.themeColorIndex);
  input.setEncoderDirectionReversed(runtimeSettings.encoderDirectionReversed);
  ESP_LOGI(logTag,
           "Loaded settings: rotation=%u theme=%d encoder=%s",
           static_cast<unsigned>(runtimeSettings.displayRotation),
           runtimeSettings.themeColorIndex,
           runtimeSettings.encoderDirectionReversed ? "B-A" : "A-B");

  bootStatusPush("Init LittleFS patterns");
  if (!settingsStoreInitPatternStorage())
  {
    ESP_LOGW(logTag, "Pattern storage init failed");
    bootStatusPush("LittleFS init failed");
  }
  else if (!settingsStoreListPatterns(patternNames, sizeof(patternNames) / sizeof(patternNames[0]), patternCount))
  {
    bootStatusPush("LittleFS list failed");
  }
  else if (patternCount == 0)
  {
    bootStatusPush("LittleFS no patterns");
  }
  else
  {
    bootStatusPush("LittleFS patterns");

    for (size_t patternIndex = 0; patternIndex < patternCount; patternIndex++)
    {
      bootStatusPush(String("PAT ") + patternNames[patternIndex]);
    }
  }

  logFilesystemRoot(LittleFS, "LittleFS");

  bootStatusPush("Init audio engine");
  if (!audioEngineInit())
  {
    ESP_LOGE(logTag, "Audio engine init failed");
    bootStatusPush("Audio init failed");
  }
  else
  {
    bootStatusPush("Audio engine ready");
  }

  bootStatusPush("WiFi action");
  systemManagerInit();

  if (WiFi.status() == WL_CONNECTED)
  {
    bootStatusPush(String("WiFi ") + WiFi.SSID() + " " + WiFi.localIP().toString());
  }
  else
  {
    bootStatusPush("WiFi standby");
  }

  bootStatusPush("System manager ready");
  bootStatusPush("Open Groovebox UI");
  uiManagerInit();

  //-- Draw first full UI frame directly from setup.
  uiManagerUpdate();

  audioTaskStarted = (xTaskCreatePinnedToCore(audioTask, "AudioTask", 8192, nullptr, 3, nullptr, 0) == pdPASS);
  uiTaskStarted = (xTaskCreatePinnedToCore(uiTask, "UiTask", 6144, nullptr, 2, nullptr, 1) == pdPASS);
  inputTaskStarted = (xTaskCreatePinnedToCore(inputTask, "InputTask", 4096, nullptr, 2, nullptr, 1) == pdPASS);
  systemTaskStarted = (xTaskCreatePinnedToCore(systemTask, "SystemTask", 6144, nullptr, 1, nullptr, 1) == pdPASS);

  if (!audioTaskStarted)
  {
    ESP_LOGE(logTag, "AudioTask creation failed");
  }

  if (!uiTaskStarted)
  {
    ESP_LOGE(logTag, "UiTask creation failed");
  }

  if (!inputTaskStarted)
  {
    ESP_LOGE(logTag, "InputTask creation failed");
  }

  if (!systemTaskStarted)
  {
    ESP_LOGE(logTag, "SystemTask creation failed");
  }

  if (!uiTaskStarted || !inputTaskStarted)
  {
    ESP_LOGW(logTag, "Input/UI fallback is active in loop() because one or more tasks failed");
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