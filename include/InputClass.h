/*** Last Changed: 2026-06-22 - 16:55 ***/
#ifndef INPUT_CLASS_H
#define INPUT_CLASS_H

#include <Arduino.h>

//--- Encoder events
enum EncoderEvent
{
  ENCODER_EVENT_NONE = 0,
  ENCODER_EVENT_LEFT = 1,
  ENCODER_EVENT_RIGHT = 2,
  ENCODER_EVENT_SHORT_PRESS = 3,
  ENCODER_EVENT_LONG_PRESS = 4,
  ENCODER_EVENT_MEDIUM_PRESS = 5
};

//--- Auxiliary button events
enum ButtonEvent
{
  BUTTON_EVENT_NONE = 0,
  BUTTON_EVENT_SHORT_PRESS = 1,
  BUTTON_EVENT_LONG_PRESS = 2,
  BUTTON_EVENT_MEDIUM_PRESS = 3
};

//--- Input pin mapping
struct InputPinConfig
{
  int encoderPinA;
  int encoderPinB;
  int encoderButtonPin;
  int auxButtonPin;
};

//--- Input timing thresholds
struct InputTimingConfig
{
  uint32_t encoderShortPressMs;
  uint32_t encoderMediumPressMs;
  uint32_t encoderLongPressMs;
  uint32_t auxButtonShortPressMs;
  uint32_t auxButtonMediumPressMs;
  uint32_t auxButtonLongPressMs;
};

//--- Full input configuration
struct InputConfig
{
  InputPinConfig pins;
  InputTimingConfig timing;
  bool encoderDirectionReversed;
};

//--- Build a configuration from project build flags
InputConfig inputCreateConfigFromBuildFlags();

//--- Generic input class for encoder + auxiliary button hardware
class InputClass
{
public:
  InputClass();
  explicit InputClass(const InputConfig& inputConfig);

  void begin();
  void update();
  void updateEncoder();
  void updateAuxButton();

  void setConfig(const InputConfig& inputConfig);
  const InputConfig& getConfig() const;

  EncoderEvent getEncoderEvent();
  void clearEncoderEvent();

  ButtonEvent getAuxButtonEvent();
  void clearAuxButtonEvent();

  void setEncoderDirectionReversed(bool reversed);
  bool getEncoderDirectionReversed() const;

private:
  uint8_t readEncoderState() const;

  InputConfig config;
  bool initialized;

  int32_t encoderDelta;
  uint8_t lastEncoderState;
  EncoderEvent pendingEncoderEvent;

  bool encoderButtonPressed;
  bool encoderLongPressReported;
  uint32_t encoderPressStartMs;

  ButtonEvent pendingAuxButtonEvent;
  bool auxButtonPressed;
  bool auxButtonLongPressReported;
  uint32_t auxButtonPressStartMs;
};

//--- Shared project instance
extern InputClass input;

#endif