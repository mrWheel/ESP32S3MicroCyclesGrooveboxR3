/*** Last Changed: 2026-06-27 - 16:18 ***/
#include "loadStatus.h"

#include <string.h>

static bool statusActive = false;
static bool statusFailed = false;
static char statusTitle[48] = "";
static char statusItem[32] = "";
static char statusError[80] = "";
static uint8_t statusCurrent = 0;
static uint8_t statusTotal = 0;

//-- Clear load status.
void loadStatusClear()
{
  statusActive = false;
  statusFailed = false;
  statusTitle[0] = '\0';
  statusItem[0] = '\0';
  statusError[0] = '\0';
  statusCurrent = 0;
  statusTotal = 0;

} //   loadStatusClear()

//-- Start a new load status operation.
void loadStatusStart(const char* title, uint8_t total)
{
  statusActive = true;
  statusFailed = false;
  statusCurrent = 0;
  statusTotal = total;
  statusItem[0] = '\0';
  statusError[0] = '\0';

  strncpy(statusTitle, title ? title : "", sizeof(statusTitle) - 1);
  statusTitle[sizeof(statusTitle) - 1] = '\0';

} //   loadStatusStart()

//-- Update current load item.
void loadStatusUpdate(uint8_t current, const char* item)
{
  statusActive = true;
  statusCurrent = current;

  strncpy(statusItem, item ? item : "", sizeof(statusItem) - 1);
  statusItem[sizeof(statusItem) - 1] = '\0';

} //   loadStatusUpdate()

//-- Mark load operation finished.
void loadStatusFinish()
{
  statusActive = false;
  statusFailed = false;
  statusItem[0] = '\0';

} //   loadStatusFinish()

//-- Mark load operation failed.
void loadStatusFail(const char* errorText)
{
  statusActive = false;
  statusFailed = true;

  strncpy(statusError, errorText ? errorText : "", sizeof(statusError) - 1);
  statusError[sizeof(statusError) - 1] = '\0';

} //   loadStatusFail()

//-- Return whether a load operation is active.
bool loadStatusIsActive()
{
  return statusActive;

} //   loadStatusIsActive()

//-- Return load operation title.
String loadStatusGetTitle()
{
  return String(statusTitle);

} //   loadStatusGetTitle()

//-- Return current load item.
String loadStatusGetItem()
{
  return String(statusItem);

} //   loadStatusGetItem()

//-- Return compact load message.
String loadStatusGetMessage()
{
  if (!statusActive)
  {
    return String("");
  }

  String message = String(statusTitle);

  if (statusItem[0] != '\0')
  {
    message += " - ";
    message += statusItem;
  }

  if (statusTotal > 0)
  {
    message += " (";
    message += String(statusCurrent);
    message += "/";
    message += String(statusTotal);
    message += ")";
  }

  return message;

} //   loadStatusGetMessage()

//-- Return current load item index.
uint8_t loadStatusGetCurrent()
{
  return statusCurrent;

} //   loadStatusGetCurrent()

//-- Return total load item count.
uint8_t loadStatusGetTotal()
{
  return statusTotal;

} //   loadStatusGetTotal()

//-- Return whether the last load failed.
bool loadStatusHasFailed()
{
  return statusFailed;

} //   loadStatusHasFailed()

//-- Return last load error.
String loadStatusGetError()
{
  return String(statusError);

} //   loadStatusGetError()