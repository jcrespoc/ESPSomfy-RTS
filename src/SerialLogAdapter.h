#pragma once
#include <Arduino.h>
#include <Stream.h>
#include <Logger.h>

// Direct reference to the underlying hardware serial port (inline function for C++11/C++14 compatibility)
inline HardwareSerial& realSerial() {
    #if ARDUINO_USB_CDC_ON_BOOT==1
    return ::Serial0;
    #else
    return ::Serial;
    #endif
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
    void setLogLevel(uint8_t level) { _logLevel = level; }
    uint8_t getLogLevel() const { return _logLevel; }

private:
    static const size_t BUFFER_SIZE = 512;
    char _buffer[BUFFER_SIZE];
    size_t _index = 0;
    bool _started = false;
    bool _heuristicLevel = true;
    uint8_t _logLevel = ELOG_LEVEL_INFO;
    SemaphoreHandle_t _mutex = nullptr;

    void flushLine();
};

extern SerialLogAdapter SerialLog;

