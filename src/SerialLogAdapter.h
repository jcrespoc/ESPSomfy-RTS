#pragma once
#include <Arduino.h>
#include <Stream.h>
#include <Logger.h>

// Direct reference to the underlying hardware serial port (inline function for C++11/C++14 compatibility)
inline HardwareSerial& realSerial() {
    return ::Serial;
}
#define RealHardwareSerial realSerial()

class SerialLogAdapter : public Stream {
public:
    SerialLogAdapter();

    // Serial.begin overloads matching HardwareSerial
    void begin(unsigned long baud, uint32_t config = SERIAL_8N1, int8_t rxPin = -1, int8_t txPin = -1, bool invert = false, unsigned long timeout_ms = 20000UL, uint8_t rxfifo_full_thrs = 112);
    void end();

    // Stream & Print overrides
    int available() override { return RealHardwareSerial.available(); }
    int read() override { return RealHardwareSerial.read(); }
    int peek() override { return RealHardwareSerial.peek(); }
    void flush() override;

    size_t write(uint8_t c) override;
    size_t write(const uint8_t *buffer, size_t size) override;

    // Passthrough methods
    void setDebugOutput(bool enable) { RealHardwareSerial.setDebugOutput(enable); }
    explicit operator bool() const { return (bool)RealHardwareSerial; }

    // Configuration
    void setDefaultLogLevel(uint8_t level) { _defaultLogLevel = level; }
    uint8_t getDefaultLogLevel() const { return _defaultLogLevel; }
    void setHeuristicLevel(bool enable) { _heuristicLevel = enable; }
    bool getHeuristicLevel() const { return _heuristicLevel; }
    void setLogId(uint8_t id) { _logId = id; }
    uint8_t getLogId() const { return _logId; }

private:
    static const size_t BUFFER_SIZE = 512;
    char _buffer[BUFFER_SIZE];
    size_t _index = 0;
    bool _started = false;
    bool _heuristicLevel = true;
    uint8_t _defaultLogLevel = ELOG_LEVEL_INFO;
    uint8_t _logId = MAINLOG;
    SemaphoreHandle_t _mutex = nullptr;

    void flushLine();
};

extern SerialLogAdapter SerialLog;

