#include "SerialLogAdapter.h"
#include <cstring>
#ifdef ENABLE_TLOG
#include <TLog.h>
#endif

SerialLogAdapter SerialLog;

SerialLogAdapter::SerialLogAdapter() {
    _mutex = xSemaphoreCreateMutex();
}

void SerialLogAdapter::begin(unsigned long baud, uint32_t config, int8_t rxPin, int8_t txPin, bool invert, unsigned long timeout_ms, uint8_t rxfifo_full_thrs) {
    RealHardwareSerial.begin(baud, config, rxPin, txPin, invert, timeout_ms, rxfifo_full_thrs);
    if (!_started) {
        _started = true;
        LoggerInit();
    }
}

void SerialLogAdapter::end() {
    flush();
    _started = false;
    RealHardwareSerial.end();
}

size_t SerialLogAdapter::write(uint8_t c) {
    return write(&c, 1);
}

size_t SerialLogAdapter::write(const uint8_t *buffer, size_t size) {
    if (size == 0) return 0;

    // Direct hardware write if inside ISR context to avoid FreeRTOS mutex blocking or memory allocations
    if (xPortInIsrContext()) {
        return RealHardwareSerial.write(buffer, size);
    }

    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        bool endsWithNewline = (buffer[size - 1] == '\n');
        size_t contentLen = size;
        if (endsWithNewline) {
            contentLen--;
            if (contentLen > 0 && buffer[contentLen - 1] == '\r') {
                contentLen--;
            }
        }

        while (contentLen > 0) {
            size_t available = (BUFFER_SIZE - 1) - _index;
            size_t toCopy = (contentLen < available) ? contentLen : available;
            memcpy(_buffer + _index, buffer, toCopy);
            _index += toCopy;
            buffer += toCopy;
            contentLen -= toCopy;

            if (_index >= BUFFER_SIZE - 1) {
                flushLine();
            }
        }

        if (endsWithNewline) {
            flushLine();
        }

        xSemaphoreGive(_mutex);
        return size;
    } else {
        // Fallback to direct hardware write if mutex acquisition timed out
        return RealHardwareSerial.write(buffer, size);
    }
}

void SerialLogAdapter::flush() {
    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (_index > 0) {
            flushLine();
        }
        xSemaphoreGive(_mutex);
    }
    RealHardwareSerial.flush();
}

void SerialLogAdapter::flushLine() {
    if (_index > 0 && _buffer[_index - 1] == '\r') {
        _index--;
    }

    if (_index == 0) {
        // Empty newline (e.g. Serial.println())
        if (_started) {
#ifndef ELOG_SERIAL_DISABLED
            Logger.log(_logId, _defaultLogLevel, "");
#endif
#ifdef ELOG_SYSLOG_ENABLE
            Logger.log(SYSLOG, _defaultLogLevel, "");
#endif
#ifdef ELOG_SPIFFS_ENABLE
            Logger.log(FILELOG, _defaultLogLevel, "");
#endif
#ifdef ELOG_WS_ENABLE
            Logger.log(WSLOG, _defaultLogLevel, "");
#endif
        } else {
#ifndef ELOG_SERIAL_DISABLED
            RealHardwareSerial.println();
#endif
        }
        return;
    }

    _buffer[_index] = '\0';

    if (!_started) {
        // Fallback if logging has not started yet
#ifndef ELOG_SERIAL_DISABLED
        RealHardwareSerial.println(_buffer);
#endif
        _index = 0;
        return;
    }

    uint8_t level = _defaultLogLevel;
    if (_heuristicLevel) {
        if (strstr(_buffer, "Error") != nullptr || strstr(_buffer, "ERROR") != nullptr || strstr(_buffer, "failed") != nullptr || strstr(_buffer, "Failed") != nullptr) {
            level = ELOG_LEVEL_ERROR;
        } else if (strstr(_buffer, "WARN") != nullptr || strstr(_buffer, "Warning") != nullptr || strstr(_buffer, "warning") != nullptr) {
            level = ELOG_LEVEL_WARNING;
        }
    }

#ifndef ELOG_SERIAL_DISABLED
    Logger.log(_logId, level, "%s", _buffer);
#endif
#ifdef ELOG_SYSLOG_ENABLE
    Logger.log(SYSLOG, level, "%s", _buffer);
#endif
#ifdef ELOG_SPIFFS_ENABLE
    Logger.log(FILELOG, level, "%s", _buffer);
#endif
#ifdef ELOG_WS_ENABLE
    Logger.log(WSLOG, level, "%s", _buffer);
#endif
    
    #ifdef ENABLE_TLOG
    Log.printf("%s\n", _buffer);
    #endif

    _index = 0;
}

