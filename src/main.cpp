/*** Last Changed: 2026-05-24 - 12:19 ***/
#include <Arduino.h>
#include <esp_log.h>
#include <esp_timer.h>

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
const char* PROG_VERSION = "v0.1.1";

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

  for (;;)
  {
    uint64_t nowUs = static_cast<uint64_t>(esp_timer_get_time());

    if (sequencerConsumeDueStep(nowUs, stepIndex, trackMask))
    {
      for (uint8_t trackIndex = 0; trackIndex < sequencerTrackCount; trackIndex++)
      {
        if ((trackMask & static_cast<uint8_t>(1U << trackIndex)) != 0)
        {
          audioEngineTriggerSample(static_cast<SampleId>(trackIndex), 255);
        }
      }
    }

    audioEngineRenderBlock();

    if (audioEngineIsOutputReady())
    {
      taskYIELD();
    }
    else
    {
      vTaskDelay(pdMS_TO_TICKS(1));
    }
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

  Serial.begin(115200);
  delay(100);

  Serial.printf("Booting ESP32 MicroCycles Groovebox (%s)\n", PROG_VERSION);
  ESP_LOGI(logTag, "Booting ESP32 MicroCycles Groovebox (%s)", PROG_VERSION);
  ESP_LOGI(logTag, "TFT pins: CS=%d DC=%d RST=%d BL=%d SCL=%d SDA=%d", PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST, PIN_TFT_BL, PIN_TFT_SCL, PIN_TFT_SDA);
  ESP_LOGI(logTag, "I2S pins: BCLK=%d WS=%d DOUT=%d", PIN_I2S_BCLK, PIN_I2S_WS, PIN_I2S_DOUT);

#ifdef NO_DAC_HARDWARE
  ESP_LOGW(logTag, "NO_DAC_HARDWARE is enabled. System remains active; only I2S/DAC hardware is skipped.");
#endif

  inputQueue = xQueueCreateStatic(24, sizeof(InputEventMessage), inputQueueStorage, &inputQueueStruct);

  input.begin();
  displayInit();
  display.drawMessage("ESP32 Groovebox", PROG_VERSION);
  sequencerInit();

  if (!sampleManagerInit())
  {
    ESP_LOGW(logTag, "Sample manager init failed, using fallback waveforms");
  }

  settingsStoreLoadRuntimeSettings(runtimeSettings);
  displaySetRotation(static_cast<int>(runtimeSettings.displayRotation));
  displaySetThemeColorIndex(runtimeSettings.themeColorIndex);
  input.setEncoderDirectionReversed(runtimeSettings.encoderDirectionReversed);
  ESP_LOGI(logTag,
           "Loaded settings: rotation=%u theme=%d encoder=%s",
           static_cast<unsigned>(runtimeSettings.displayRotation),
           runtimeSettings.themeColorIndex,
           runtimeSettings.encoderDirectionReversed ? "B-A" : "A-B");

  if (!audioEngineInit())
  {
    ESP_LOGE(logTag, "Audio engine init failed");
  }

  systemManagerInit();
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