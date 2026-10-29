#include "SerialLogAdapter.h"
#include <cstring>
#include "Logger.h"

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

    if (xPortInIsrContext()) {
      return 0;
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
            log(_logLevel, "");
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

    log(_logLevel, _buffer);
    _index = 0;
}

