#pragma once
#include <Arduino.h>
#include <Stream.h>

class WsLogStream : public Stream {
public:
    WsLogStream();

    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override;

    size_t write(uint8_t c) override;
    size_t write(const uint8_t *buffer, size_t size) override;

    void loop();

private:
    static const size_t BUFFER_SIZE = 512;
    char _buffer[BUFFER_SIZE];
    size_t _index = 0;
    QueueHandle_t _queue = nullptr;
    SemaphoreHandle_t _mutex = nullptr;

    void enqueueLine();
};

extern WsLogStream wsLogStream;

