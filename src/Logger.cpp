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
  #define ELOG_SYSLOG_HOST ""
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

void logf(uint8_t level, const char *format, ...) {
  if (xPortInIsrContext()) {
    return;
  }
  va_list args;
  va_start(args, format);
  #ifndef ELOG_SERIAL_DISABLED
  Logger.log(MAINLOG, level, format, args);
  #endif
  #ifdef ELOG_SYSLOG_ENABLE
  Logger.log(SYSLOG, level, format, args);
  #endif
  #ifdef ELOG_SPIFFS_ENABLE
  Logger.log(FILELOG, level, format, args);
  #endif
  #ifdef ELOG_WS_ENABLE
  Logger.log(WSLOG, level, format, args);
  #endif
  va_end(args);
}

void log(uint8_t level, const char *str) {
  if (xPortInIsrContext()) {
    return;
  }
  #ifndef ELOG_SERIAL_DISABLED
  Logger.log(MAINLOG, level, str);
  #endif
  #ifdef ELOG_SYSLOG_ENABLE
  Logger.log(SYSLOG, level, str);
  #endif
  #ifdef ELOG_SPIFFS_ENABLE
  Logger.log(FILELOG, level, str);
  #endif
  #ifdef ELOG_WS_ENABLE
  Logger.log(WSLOG, level, str);
  #endif
}

void log(uint8_t level, const __FlashStringHelper *str) {
  if (xPortInIsrContext()) {
    return;
  }
  #ifndef ELOG_SERIAL_DISABLED
  Logger.log(MAINLOG, level, str);
  #endif
  #ifdef ELOG_SYSLOG_ENABLE
  Logger.log(SYSLOG, level, str);
  #endif
  #ifdef ELOG_SPIFFS_ENABLE
  Logger.log(FILELOG, level, str);  
  #endif
  #ifdef ELOG_WS_ENABLE
  Logger.log(WSLOG, level, str);
  #endif
}

void log(uint8_t level, const String str) {
  if (xPortInIsrContext()) {
    return;
  }
  #ifndef ELOG_SERIAL_DISABLED
  Logger.log(MAINLOG, level, str.c_str());
  #endif
  #ifdef ELOG_SYSLOG_ENABLE
  Logger.log(SYSLOG, level, str.c_str());
  #endif
  #ifdef ELOG_SPIFFS_ENABLE
  Logger.log(FILELOG, level, str.c_str());
  #endif
  #ifdef ELOG_WS_ENABLE
  Logger.log(WSLOG, level, str.c_str());
  #endif
}

void log(uint8_t level, const char *str, const void *buff, size_t len) {
  if (xPortInIsrContext()) {
    return;
  }  
  #ifndef ELOG_SERIAL_DISABLED
  Logger.log(MAINLOG,  level, str, (const uint8_t*) buff, len);
  #endif
  #ifdef ELOG_SYSLOG_ENABLE
  Logger.log(SYSLOG, level, str, (const uint8_t*) buff, len);
  #endif
  #ifdef ELOG_SPIFFS_ENABLE
  Logger.log(FILELOG, level, str, (const uint8_t*) buff, len);
  #endif
  #ifdef ELOG_WS_ENABLE
  Logger.log(WSLOG, level, str, (const uint8_t*) buff, len);
  #endif
}

void LoggerInit() {
  #ifndef ELOG_SERIAL_DISABLED
  Logger.registerSerial(MAINLOG, ELOG_SERIAL_LEVEL, "Serial", RealHardwareSerial,  ELOG_FLAG_NO_SERVICE | ELOG_FLAG_TIME_SHORT);

  //#define ELOG_INTERNAL_LOGGING_ENABLED
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
  Logger.setSerialLogLevel(MAINLOG, cfg.serialLevel, RealHardwareSerial);
  #endif

  #ifdef ELOG_WS_ENABLE
  Logger.setSerialLogLevel(WSLOG, cfg.wsLevel, wsLogStream);
  #endif

  #ifdef ELOG_SYSLOG_ENABLE
  if (strlen(cfg.syslogHost) > 0 && cfg.syslogPort > 0 && cfg.syslogPort < 65536) {
    Logger.setSyslogServer(cfg.syslogHost, cfg.syslogPort);
    Logger.setSyslogLogLevel(SYSLOG, cfg.syslogLevel, ELOG_FAC_SYSLOG);
  }
  if (settings.hostname != NULL) {
    Logger.configureSyslog(ELOG_SYSLOG_HOST, ELOG_SYSLOG_PORT, settings.hostname, false, 5000, 50);
  }
  #endif

  #ifdef ELOG_SPIFFS_ENABLE
  Logger.setSpiffsLogLevel(FILELOG, cfg.spiffsLevel, "espsomfy");
  #endif
}
