#include <Logger.h>
#include "ConfigSettings.h"
#include "SerialLogAdapter.h"
#undef Serial
#include <SPIFFS.h>
#ifdef ELOG_WS_ENABLE
#include "WsLogStream.h"
#endif

#include <stdio.h>
#include "esp_core_dump.h"
#include "esp_partition.h"

#ifdef ELOG_SYSLOG_ENABLE
  #ifndef ELOG_SYSLOG_HOST
  #define ELOG_SYSLOG_HOST "foo.bar"
  #endif
  #ifndef ELOG_SYSLOG_PORT
    #define ELOG_SYSLOG_PORT 514
  #endif
#endif

#ifdef ELOG_SYSLOG_ENABLE
extern ConfigSettings settings;
#endif

void log(uint8_t logId, uint8_t logLevel, const char* format, ...) {
  if (xPortInIsrContext()) {
    return;
  }
  Logger.log(logId, logLevel, format);
}

void log(uint8_t logId, uint8_t logLevel, const __FlashStringHelper* format, ...) {
  #ifdef ESP32
    if (xPortInIsrContext()) {
      // When called from an ISR, you should not send out logs.
      // Allocating memory from within an ISR is a big no-no.
      // Also long-time blocking like sending logs (especially to a syslog server) 
      // is also really not a good idea from an ISR call.
      return;
    }
  #endif
  Logger.log(logId, logLevel, format);
}

void logHex(uint8_t logId, uint8_t logLevel, const char* message, const uint8_t* data, uint16_t length) {
  #ifdef ESP32
    if (xPortInIsrContext()) {
      // When called from an ISR, you should not send out logs.
      // Allocating memory from within an ISR is a big no-no.
      // Also long-time blocking like sending logs (especially to a syslog server) 
      // is also really not a good idea from an ISR call.
      return;
    }
  #endif
  Logger.logHex(logId, logLevel, message, data, length);
}

void logRoute(uint8_t level, const char *str) {
  #ifndef ELOG_SERIAL_DISABLED
  log(MAINLOG, level, str);
  #endif
  #ifdef ELOG_SYSLOG_ENABLE
  log(SYSLOG, level, str);
  #endif
  #ifdef ELOG_SPIFFS_ENABLE
  log(FILELOG, level, str);
  #endif
  #ifdef ELOG_WS_ENABLE
  log(WSLOG, level, str);
  #endif
}

void logRoute(uint8_t level, const __FlashStringHelper *str) {
  #ifndef ELOG_SERIAL_DISABLED
  log(MAINLOG, level, str);
  #endif
  #ifdef ELOG_SYSLOG_ENABLE
  log(SYSLOG, level, str);
  #endif
  #ifdef ELOG_SPIFFS_ENABLE
  log(FILELOG, level, str);  
  #endif
  #ifdef ELOG_WS_ENABLE
  log(WSLOG, level, str);
  #endif
}

void logRoute(uint8_t level, const String str) {
  #ifndef ELOG_SERIAL_DISABLED
  log(MAINLOG, level, str.c_str());
  #endif
  #ifdef ELOG_SYSLOG_ENABLE
  log(SYSLOG, level, str.c_str());
  #endif
  #ifdef ELOG_SPIFFS_ENABLE
  log(FILELOG, level, str.c_str());
  #endif
  #ifdef ELOG_WS_ENABLE
  log(WSLOG, level, str.c_str());
  #endif
}

void logRoute(uint8_t level, const char *str, const void *buff, size_t len) {
  #ifndef ELOG_SERIAL_DISABLED
  logHex(MAINLOG,  level, str, (const uint8_t*) buff, len);
  #endif
  #ifdef ELOG_SYSLOG_ENABLE
  logHex(SYSLOG, level, str, (const uint8_t*) buff, len);
  #endif
  #ifdef ELOG_SPIFFS_ENABLE
  logHex(FILELOG, level, str, (const uint8_t*) buff, len);
  #endif
  #ifdef ELOG_WS_ENABLE
  logHex(WSLOG, level, str, (const uint8_t*) buff, len);
  #endif
}

void LoggerInit() {
  #ifndef ELOG_SERIAL_DISABLED
  Logger.registerSerial(MAINLOG, ELOG_SERIAL_LEVEL, "Serial", RealHardwareSerial,  ELOG_FLAG_NO_SERVICE | ELOG_FLAG_TIME_SHORT);

  Logger.configureInternalLogging(RealHardwareSerial, ELOG_LEVEL_INFO, 60*1000);
  //Logger.enableQuery(RealHardwareSerial);
  #endif

  #ifdef ELOG_WS_ENABLE
  Logger.registerSerial(WSLOG, ELOG_WS_LEVEL, "Web", wsLogStream, ELOG_FLAG_NO_SERVICE | ELOG_FLAG_TIME_SHORT);
  #endif

  #ifdef ELOG_SYSLOG_ENABLE
  WiFi.begin();
  Logger.configureSyslog(ELOG_SYSLOG_HOST, ELOG_SYSLOG_PORT, settings.hostname != NULL ? settings.hostname : "ESPSomfy", false, 5000, 50);
  Logger.registerSyslog(SYSLOG, ELOG_SYSLOG_LEVEL, ELOG_FAC_SYSLOG, "");
  #endif // ELOG_SYSLOG_ENABLE

  #ifdef ELOG_SPIFFS_ENABLE
  Logger.configureSpiffs(2);
  Logger.registerSpiffs(FILELOG, ELOG_SPIFFS_LEVEL, "espsomfy", ELOG_FLAG_NO_SERVICE, 10000);
  #endif
}
