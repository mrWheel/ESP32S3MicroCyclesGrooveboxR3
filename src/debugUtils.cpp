/*** Last Changed: 2026-06-28 - 11:38 ***/
#include "debugUtils.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

//-- Log remaining stack space for the currently running task.
void logStackHighWaterMark(const char* tag, const char* label)
{
#if CORE_DEBUG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG

  const UBaseType_t freeStackWords = uxTaskGetStackHighWaterMark(nullptr);
  const size_t freeStackBytes = static_cast<size_t>(freeStackWords) * sizeof(StackType_t);

  ESP_LOGD(tag, "[Stack] %s free=%lu bytes", label, static_cast<unsigned long>(freeStackBytes));

#else

  (void)tag;
  (void)label;

#endif

} //   logStackHighWaterMark()
