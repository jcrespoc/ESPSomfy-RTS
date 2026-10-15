#ifndef _LOGGER_H
#define _LOGGER_H

#define LOG_LEVEL_ALWAYS     0
#define LOG_LEVEL_EMERGENCY  1
#define LOG_LEVEL_ALERT      2
#define LOG_LEVEL_CRITICAL   3
#define LOG_LEVEL_ERROR      4
#define LOG_LEVEL_WARNING    5
#define LOG_LEVEL_NOTICE     6
#define LOG_LEVEL_INFO       7
#define LOG_LEVEL_DEBUG      8
#define LOG_LEVEL_TRACE      9
#define LOG_LEVEL_VERBOSE    10
#define LOG_LEVEL_NOLOG      11

#ifndef ELOG_SYSLOG_LEVEL
#define ELOG_SYSLOG_LEVEL LOG_LEVEL_NOLOG
#endif
#ifndef ELOG_SPIFFS_LEVEL
#define ELOG_SPIFFS_LEVEL LOG_LEVEL_NOLOG
#endif
#ifndef ELOG_SERIAL_LEVEL
#define ELOG_SERIAL_LEVEL LOG_LEVEL_NOLOG
#endif
#ifndef ELOG_WS_LEVEL
  #ifdef ELOG_WS_ENABLE
    #define ELOG_WS_LEVEL LOG_LEVEL_INFO
  #else
    #define ELOG_WS_LEVEL LOG_LEVEL_NOLOG
  #endif
#endif

#if ELOG_SERIAL_LEVEL < LOG_LEVEL_NOLOG
#undef ELOG_SERIAL_DISABLED
#else
#define ELOG_SERIAL_DISABLED
#endif

#if ELOG_WS_LEVEL < LOG_LEVEL_NOLOG
  #ifndef ELOG_WS_ENABLE
    #define ELOG_WS_ENABLE
  #endif
#endif


#include <Elog.h>


#define MAINLOG 0
#define SYSLOG 1
#define FILELOG 2
#define WSLOG 3

void LoggerInit();

template <typename Fmt, typename... Args>
inline void logRoute(uint8_t level, Fmt format, Args... args) {
  #ifndef ELOG_SERIAL_DISABLED
    Logger.log(MAINLOG, level, format, args...);
  #endif
  #ifdef ELOG_SYSLOG_ENABLE
    Logger.log(SYSLOG, level, format, args...);
  #endif
  #ifdef ELOG_SPIFFS_ENABLE
    Logger.log(FILELOG, level, format, args...);
  #endif
  #ifdef ELOG_WS_ENABLE
    Logger.log(WSLOG, level, format, args...);
  #endif
}

void logRoute(uint8_t level, const char *str);
void logRoute(uint8_t level, const __FlashStringHelper *str);
void logRoute(uint8_t level, const String str);
void logRoute(uint8_t level, const char *str, const void *buff, size_t len);

#define LOG_E_PRINTF(fmt, ...) logRoute(ELOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define LOG_I_PRINTF(fmt, ...) logRoute(ELOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define LOG_D_PRINTF(fmt, ...) logRoute(ELOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)

#define LOG_E_PRINTLN(str) logRoute(ELOG_LEVEL_ERROR, str)
#define LOG_I_PRINTLN(str) logRoute(ELOG_LEVEL_INFO, str)
#define LOG_D_PRINTLN(str) logRoute(ELOG_LEVEL_DEBUG, str)

#define LOG_E_HEX(str, buff, len) logRoute(ELOG_LEVEL_ERROR, str, buff, len)
#define LOG_I_HEX(str, buff, len) logRoute(ELOG_LEVEL_INFO, str, buff, len)
#define LOG_D_HEX(str, buff, len) logRoute(ELOG_LEVEL_DEBUG, str, buff, len)

#endif //_LOGGER_H