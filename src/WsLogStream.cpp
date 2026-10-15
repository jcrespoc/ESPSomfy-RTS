#include "WsLogStream.h"
#include "Sockets.h"
#include <cstring>

extern WebSocketsServer sockServer;
extern SocketEmitter sockEmit;

WsLogStream wsLogStream;

struct WsLogMsg {
    char text[384];
};

WsLogStream::WsLogStream() {
    _queue = xQueueCreate(20, sizeof(WsLogMsg));
}

void WsLogStream::enqueueLine() {
    if (_index > 0 && _buffer[_index - 1] == '\r') {
        _index--;
    }
    if (_index == 0) return;

    _buffer[_index] = '\0';

    // Only enqueue if at least one client is in the log room
    if (sockEmit.rooms[ROOM_EMIT_LOG].activeClients() > 0 && _queue) {
        WsLogMsg msg;
        strncpy(msg.text, _buffer, sizeof(msg.text) - 1);
        msg.text[sizeof(msg.text) - 1] = '\0';

        // Non-blocking send: if queue is full, drop to prevent memory bloat
        xQueueSend(_queue, &msg, 0);
    }

    _index = 0;
}

size_t WsLogStream::write(uint8_t c) {
    return write(&c, 1);
}

size_t WsLogStream::write(const uint8_t *buffer, size_t size) {
    if (size == 0) return 0;

    // Fast path: if no clients are listening to logs, drop immediately
    if (sockEmit.rooms[ROOM_EMIT_LOG].activeClients() == 0) {
        return size;
    }

    
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
            enqueueLine();
        }
    }

    if (endsWithNewline) {
        enqueueLine();
    }

    return size;
}

void WsLogStream::flush() {
    if (_index > 0) {
        enqueueLine();
    }
}

void WsLogStream::loop() {    
    if (sockEmit.rooms[ROOM_EMIT_LOG].activeClients() == 0) {        
        // Discard any queued messages if no clients are listening
        WsLogMsg dummy;
        while (_queue && xQueueReceive(_queue, &dummy, 0) == pdTRUE) {}
        return;
    }

    WsLogMsg msg;
    char payload[512];
    uint8_t count = 0;
    
    // Drain up to 10 messages per tick to prevent starving other network tasks
    while (_queue && count < 10 && xQueueReceive(_queue, &msg, 0) == pdTRUE) {
        count++;

        // Escape JSON special characters for Socket.IO format: 42["log","<escaped>"]
        String escaped = msg.text;
        escaped.replace("\\", "\\\\");
        escaped.replace("\"", "\\\"");
        escaped.replace("\r", "");
        escaped.replace("\n", "");
        escaped.replace("\t", " ");

        snprintf(payload, sizeof(payload), "42[\"log\",\"%s\"]", escaped.c_str());

        room_t *r = &sockEmit.rooms[ROOM_EMIT_LOG];
        for (uint8_t i = 0; i < sizeof(r->clients); i++) {
            uint8_t num = r->clients[i];
            if (num != 255 && sockServer.clientIsConnected(num)) {
                sockServer.sendTXT(num, payload);
            }
        }
    }
}

