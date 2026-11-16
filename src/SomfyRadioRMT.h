#ifndef SOMFY_RADIO_RMT_H
#define SOMFY_RADIO_RMT_H

#include <Arduino.h>
#include "SomfyRadio.h"

struct somfy_rx_t;

class SomfyRadioRMT {
public:
    static bool initTx(uint8_t txPin);
    static void sendFrame(const uint8_t *frame, uint8_t sync, uint8_t bitLength);
    static void endTx();
    static void deinitTx();
};

#endif // SOMFY_RADIO_RMT_H

