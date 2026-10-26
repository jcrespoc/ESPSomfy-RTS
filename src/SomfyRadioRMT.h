#ifndef SOMFY_RADIO_RMT_H
#define SOMFY_RADIO_RMT_H

#include <Arduino.h>
#include "SomfyRadio.h"

#if defined(SOMFY_USE_RMT_TX) || defined(SOMFY_USE_RMT_RX)

struct somfy_rx_t;

class SomfyRadioRMT {
public:
#if defined(SOMFY_USE_RMT_TX)
    static bool initTx(uint8_t txPin);
    static void sendFrame(const uint8_t *frame, uint8_t sync, uint8_t bitLength);
    static void endTx();
    static void deinitTx();
    //static bool startTestSignal(uint16_t high_us = 640, uint16_t low_us = 640);
    //static void stopTestSignal();
    //static bool isTestSignalRunning();
#endif

#if defined(SOMFY_USE_RMT_RX)
    static bool initRx(uint8_t rxPin);
    static void enableRx();
    static void disableRx();
    static bool isRxInitialized();
    static bool isRxEnabled();
    static bool isReceiving();
    static bool pollReceive(somfy_rx_t *rx);
    static void deinitRx();
#endif
};

#endif // SOMFY_USE_RMT_TX || SOMFY_USE_RMT_RX

#endif // SOMFY_RADIO_RMT_H

