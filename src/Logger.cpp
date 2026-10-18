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
    #error "ELOG_SYSLOG_HOST is not defined"
  #endif
  #ifndef ELOG_SYSLOG_PORT
    #define ELOG_SYSLOG_PORT 514
  #endif
#endif

#ifdef ELOG_SYSLOG_ENABLE
extern ConfigSettings settings;
#endif
#ifdef ELOG_SPIFFS_ENABLE
extern ConfigSettings settings;
#endif

const char* LOG_LEVEL_TO_STRING(uint8_t level) {
  return (level == LOG_LEVEL_ERROR ? "ERROR" :
          level == LOG_LEVEL_WARNING ? "WARNING" :
          level == LOG_LEVEL_INFO ? "INFO" :
          level == LOG_LEVEL_DEBUG ? "DEBUG" :
          level == LOG_LEVEL_VERBOSE ? "VERBOSE" :
          level == LOG_LEVEL_NOLOG ? "DISABLED" : "UNKNOWN");
}

void log(uint8_t logId, uint8_t logLevel, const char* format, ...) {
  if (xPortInIsrContext()) {
    return;
  }
  Logger.log(logId, logLevel, format);
}

void log(uint8_t logId, uint8_t logLevel, const __FlashStringHelper* format, ...) {
  #ifdef ESP32
    if (xPortInIsrContext()) {
      return;
    }
  #endif
  Logger.log(logId, logLevel, format);
}

void logHex(uint8_t logId, uint8_t logLevel, const char* message, const uint8_t* data, uint16_t length) {
  #ifdef ESP32
    if (xPortInIsrContext()) {
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

void logRouteHex(uint8_t level, const char *str, const void *buff, size_t len) {
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

void logRoute(uint8_t level, const char *str, const void *buff, size_t len) {
  logRouteHex(level, str, buff, len);
}

void LoggerInit() {
  #ifdef ELOG_SERIAL_DISABLED
  Logger.registerSerial(MAINLOG, ELOG_SERIAL_LEVEL, "Serial", RealHardwareSerial,  ELOG_FLAG_NO_SERVICE | ELOG_FLAG_TIME_SHORT);

  #ifdef ELOG_INTERNAL_LOGGING_ENABLED
  Logger.configureInternalLogging(RealHardwareSerial, ELOG_LEVEL_INFO, 60*1000);
  #endif
  
  #ifdef ELOG_QUERY_ENABLED
  Logger.enableQuery(RealHardwareSerial);
  #endif
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
  #ifndef ELOG_SPIFFS_FILESIZE
  #define ELOG_SPIFFS_FILESIZE 100*1024 // 100 KB
  #endif
  Logger.configureSpiffs();
  Logger.registerSpiffs(FILELOG, ELOG_SPIFFS_LEVEL, "espsomfy", ELOG_FLAG_NO_SERVICE, ELOG_SPIFFS_FILESIZE);
  #endif
}

void applyLogSettings(const LogSettings& cfg) {  
  #ifndef ELOG_SERIAL_DISABLED
  Logger.log(MAINLOG, ELOG_LEVEL_INFO, "LOGLEVEL %s", LOG_LEVEL_TO_STRING(cfg.serialLevel));
  Logger.setSerialLogLevel(MAINLOG, cfg.serialLevel, RealHardwareSerial);
  #endif

  #ifdef ELOG_WS_ENABLE
  Logger.log(WSLOG, ELOG_LEVEL_INFO, "LOGLEVEL %s", LOG_LEVEL_TO_STRING(cfg.wsLevel));
  Logger.setSerialLogLevel(WSLOG, cfg.wsLevel, wsLogStream);
  #endif

  #ifdef ELOG_SYSLOG_ENABLE
  if (strlen(cfg.syslogHost) > 0) {    
    Logger.setSyslogServer(cfg.syslogHost, cfg.syslogPort);
  }
  Logger.log(SYSLOG, ELOG_LEVEL_INFO, "LOGLEVEL %s", LOG_LEVEL_TO_STRING(cfg.syslogLevel));
  Logger.setSyslogLogLevel(SYSLOG, cfg.syslogLevel, ELOG_FAC_SYSLOG);
  #endif

  #ifdef ELOG_SPIFFS_ENABLE
  Logger.setSpiffsLogLevel(FILELOG, cfg.spiffsLevel, "espsomfy");
  #endif
}
