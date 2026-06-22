/*** Last Changed: 2026-06-22 - 16:55 ***/
#include "InputClass.h"
#include "appConfig.h"

#include <esp_log.h>

//--- Logging tag
static const char* logTag = "InputClass";

//--- Shared project instance
InputClass input;

//--- Build a configuration from project build flags
InputConfig inputCreateConfigFromBuildFlags()
{
  InputConfig buildConfig;

  buildConfig.pins.encoderPinA = PIN_ENC_A;
  buildConfig.pins.encoderPinB = PIN_ENC_B;
  buildConfig.pins.encoderButtonPin = PIN_ENC_BTN;
  buildConfig.pins.auxButtonPin = PIN_KEY0;

  buildConfig.timing.encoderShortPressMs = ENCODER_SHORT_PRESS_MS;
  buildConfig.timing.encoderMediumPressMs = ENCODER_MEDIUM_PRESS_MS;
  buildConfig.timing.encoderLongPressMs = ENCODER_LONG_PRESS_MS;
  buildConfig.timing.auxButtonShortPressMs = BUTTON_SHORT_PRESS_MS;
  buildConfig.timing.auxButtonMediumPressMs = BUTTON_MEDIUM_PRESS_MS;
  buildConfig.timing.auxButtonLongPressMs = BUTTON_LONG_PRESS_MS;

  buildConfig.encoderDirectionReversed = false;

  return buildConfig;

} //   inputCreateConfigFromBuildFlags()

//--- Constructor with build-flag configuration
InputClass::InputClass()
    : config(inputCreateConfigFromBuildFlags()), initialized(false), encoderDelta(0),
      lastEncoderState(0), pendingEncoderEvent(ENCODER_EVENT_NONE), encoderButtonPressed(false),
      encoderLongPressReported(false), encoderPressStartMs(0),
      pendingAuxButtonEvent(BUTTON_EVENT_NONE), auxButtonPressed(false),
      auxButtonLongPressReported(false), auxButtonPressStartMs(0)
{
} //   InputClass()

//--- Constructor with custom configuration
InputClass::InputClass(const InputConfig& inputConfig)
    : config(inputConfig), initialized(false), encoderDelta(0), lastEncoderState(0),
      pendingEncoderEvent(ENCODER_EVENT_NONE), encoderButtonPressed(false),
      encoderLongPressReported(false), encoderPressStartMs(0),
      pendingAuxButtonEvent(BUTTON_EVENT_NONE), auxButtonPressed(false),
      auxButtonLongPressReported(false), auxButtonPressStartMs(0)
{
} //   InputClass()

//--- Initialize input hardware
void InputClass::begin()
{
  if (initialized)
  {
    return;
  }

  pinMode(config.pins.encoderPinA, INPUT_PULLUP);
  pinMode(config.pins.encoderPinB, INPUT_PULLUP);
  pinMode(config.pins.encoderButtonPin, INPUT_PULLUP);
  pinMode(config.pins.auxButtonPin, INPUT_PULLUP);

  encoderDelta = 0;
  lastEncoderState = readEncoderState();
  pendingEncoderEvent = ENCODER_EVENT_NONE;
  pendingAuxButtonEvent = BUTTON_EVENT_NONE;
  encoderButtonPressed = false;
  encoderLongPressReported = false;
  auxButtonPressed = false;
  auxButtonLongPressReported = false;

  initialized = true;

  ESP_LOGI(logTag, "Input initialized");

} //   begin()

//--- Update encoder and auxiliary button
void InputClass::update()
{
  updateEncoder();
  updateAuxButton();

} //   update()

//--- Update encoder quadrature and encoder button
void InputClass::updateEncoder()
{
  uint8_t currentState = readEncoderState();

  if (currentState != lastEncoderState)
  {
    if ((lastEncoderState == 0b00 && currentState == 0b01) ||
        (lastEncoderState == 0b01 && currentState == 0b11) ||
        (lastEncoderState == 0b11 && currentState == 0b10) ||
        (lastEncoderState == 0b10 && currentState == 0b00))
    {
      encoderDelta++;
    }
    else
    {
      encoderDelta--;
    }

    if (encoderDelta >= 4)
    {
      if (pendingEncoderEvent == ENCODER_EVENT_NONE)
      {
        pendingEncoderEvent =
            config.encoderDirectionReversed ? ENCODER_EVENT_LEFT : ENCODER_EVENT_RIGHT;
      }
      encoderDelta = 0;
    }
    else if (encoderDelta <= -4)
    {
      if (pendingEncoderEvent == ENCODER_EVENT_NONE)
      {
        pendingEncoderEvent =
            config.encoderDirectionReversed ? ENCODER_EVENT_RIGHT : ENCODER_EVENT_LEFT;
      }
      encoderDelta = 0;
    }

    lastEncoderState = currentState;
  }

  bool rawPressed = digitalRead(config.pins.encoderButtonPin) == LOW;

  if (rawPressed && !encoderButtonPressed)
  {
    encoderButtonPressed = true;
    encoderLongPressReported = false;
    encoderPressStartMs = millis();
  }
  else if (rawPressed && encoderButtonPressed && !encoderLongPressReported)
  {
    if (millis() - encoderPressStartMs >= config.timing.encoderLongPressMs)
    {
      pendingEncoderEvent = ENCODER_EVENT_LONG_PRESS;
      encoderLongPressReported = true;
    }
  }
  else if (!rawPressed && encoderButtonPressed)
  {
    uint32_t pressDurationMs = millis() - encoderPressStartMs;

    if (!encoderLongPressReported)
    {
      if (pressDurationMs >= config.timing.encoderMediumPressMs)
      {
        if (pendingEncoderEvent == ENCODER_EVENT_NONE)
        {
          pendingEncoderEvent = ENCODER_EVENT_MEDIUM_PRESS;
        }
      }
      else if (pressDurationMs >= config.timing.encoderShortPressMs)
      {
        if (pendingEncoderEvent == ENCODER_EVENT_NONE)
        {
          pendingEncoderEvent = ENCODER_EVENT_SHORT_PRESS;
        }
      }
    }

    encoderButtonPressed = false;
    encoderLongPressReported = false;
  }

} //   updateEncoder()

//--- Update auxiliary button
void InputClass::updateAuxButton()
{
  bool rawPressed = digitalRead(config.pins.auxButtonPin) == LOW;

  if (rawPressed && !auxButtonPressed)
  {
    auxButtonPressed = true;
    auxButtonLongPressReported = false;
    auxButtonPressStartMs = millis();
  }
  else if (rawPressed && auxButtonPressed && !auxButtonLongPressReported)
  {
    if (millis() - auxButtonPressStartMs >= config.timing.auxButtonLongPressMs)
    {
      pendingAuxButtonEvent = BUTTON_EVENT_LONG_PRESS;
      auxButtonLongPressReported = true;
    }
  }
  else if (!rawPressed && auxButtonPressed)
  {
    uint32_t pressDurationMs = millis() - auxButtonPressStartMs;

    if (!auxButtonLongPressReported)
    {
      if (pressDurationMs >= config.timing.auxButtonMediumPressMs)
      {
        pendingAuxButtonEvent = BUTTON_EVENT_MEDIUM_PRESS;
      }
      else if (pressDurationMs >= config.timing.auxButtonShortPressMs)
      {
        pendingAuxButtonEvent = BUTTON_EVENT_SHORT_PRESS;
      }
    }

    auxButtonPressed = false;
    auxButtonLongPressReported = false;
  }

} //   updateAuxButton()

//--- Set full input configuration
void InputClass::setConfig(const InputConfig& inputConfig)
{
  config = inputConfig;
  encoderDelta = 0;
  pendingEncoderEvent = ENCODER_EVENT_NONE;
  pendingAuxButtonEvent = BUTTON_EVENT_NONE;
  encoderButtonPressed = false;
  encoderLongPressReported = false;
  auxButtonPressed = false;
  auxButtonLongPressReported = false;

  if (initialized)
  {
    pinMode(config.pins.encoderPinA, INPUT_PULLUP);
    pinMode(config.pins.encoderPinB, INPUT_PULLUP);
    pinMode(config.pins.encoderButtonPin, INPUT_PULLUP);
    pinMode(config.pins.auxButtonPin, INPUT_PULLUP);
    lastEncoderState = readEncoderState();
  }

} //   setConfig()

//--- Get active input configuration
const InputConfig& InputClass::getConfig() const
{
  return config;

} //   getConfig()

//--- Get next encoder event
EncoderEvent InputClass::getEncoderEvent()
{
  EncoderEvent event = pendingEncoderEvent;
  pendingEncoderEvent = ENCODER_EVENT_NONE;

  return event;

} //   getEncoderEvent()

//--- Clear pending encoder event
void InputClass::clearEncoderEvent()
{
  pendingEncoderEvent = ENCODER_EVENT_NONE;

} //   clearEncoderEvent()

//--- Get next auxiliary button event
ButtonEvent InputClass::getAuxButtonEvent()
{
  ButtonEvent event = pendingAuxButtonEvent;
  pendingAuxButtonEvent = BUTTON_EVENT_NONE;

  return event;

} //   getAuxButtonEvent()

//--- Clear pending auxiliary button event
void InputClass::clearAuxButtonEvent()
{
  pendingAuxButtonEvent = BUTTON_EVENT_NONE;

} //   clearAuxButtonEvent()

//--- Set encoder direction reversal
void InputClass::setEncoderDirectionReversed(bool reversed)
{
  config.encoderDirectionReversed = reversed;

} //   setEncoderDirectionReversed()

//--- Get encoder direction reversal
bool InputClass::getEncoderDirectionReversed() const
{
  return config.encoderDirectionReversed;

} //   getEncoderDirectionReversed()

//--- Read current encoder quadrature state
uint8_t InputClass::readEncoderState() const
{
  uint8_t encoderA = static_cast<uint8_t>(digitalRead(config.pins.encoderPinA));
  uint8_t encoderB = static_cast<uint8_t>(digitalRead(config.pins.encoderPinB));

  return static_cast<uint8_t>((encoderA << 1) | encoderB);

} //   readEncoderState()
