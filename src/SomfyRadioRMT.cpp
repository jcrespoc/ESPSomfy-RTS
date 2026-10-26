#include "SomfyRadioRMT.h"

#if defined(SOMFY_USE_RMT_TX) || defined(SOMFY_USE_RMT_RX)

#include <driver/rmt.h>
#include <esp_err.h>
#if defined(SOMFY_USE_RMT_RX)
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>
#include "Somfy.h"
#endif

#if defined(CONFIG_IDF_TARGET_ESP32C3)
#define RMT_TX_CHANNEL RMT_CHANNEL_0
#define RMT_RX_CHANNEL RMT_CHANNEL_2
#define RMT_RX_MEM_BLOCKS 1
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
#define RMT_TX_CHANNEL RMT_CHANNEL_0
#define RMT_RX_CHANNEL RMT_CHANNEL_2
#define RMT_RX_MEM_BLOCKS 1
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#define RMT_TX_CHANNEL RMT_CHANNEL_0
#define RMT_RX_CHANNEL RMT_CHANNEL_4
#define RMT_RX_MEM_BLOCKS 2
#else // ESP32 classic
#define RMT_TX_CHANNEL RMT_CHANNEL_0
#define RMT_RX_CHANNEL RMT_CHANNEL_4
#define RMT_RX_MEM_BLOCKS 2
#endif

#define RMT_CLK_DIV    80   // 80 MHz / 80 = 1 MHz -> 1 tick = 1 us
#define MAX_RMT_ITEMS  128

static inline rmt_item32_t makeRmtItem(uint16_t dur0, uint8_t lvl0, uint16_t dur1, uint8_t lvl1) {
    rmt_item32_t item;
    item.duration0 = dur0;
    item.level0 = lvl0;
    item.duration1 = dur1;
    item.level1 = lvl1;
    return item;
}

#if defined(SOMFY_USE_RMT_TX)
static int8_t s_tx_pin = -1;
static bool s_tx_initialized = false;
//static bool s_test_running = false;

bool SomfyRadioRMT::initTx(uint8_t txPin) {
    if (s_tx_initialized && s_tx_pin == (int8_t)txPin) {
        return true;
    }
    if (s_tx_initialized) {
        deinitTx();
    }
    rmt_config_t config = {};
    config.rmt_mode = RMT_MODE_TX;
    config.channel = RMT_TX_CHANNEL;
    config.gpio_num = (gpio_num_t)txPin;
    config.mem_block_num = 2;
    config.clk_div = RMT_CLK_DIV;
    config.tx_config.loop_en = false;
    config.tx_config.carrier_en = false;
    config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
    config.tx_config.idle_output_en = true;

    esp_err_t err = rmt_driver_install(config.channel, 0, 0);
    if (err != ESP_OK) {
        Serial.printf("[RMT-TX] rmt_driver_install returned error: %d\n", err);
        return false;
    }
    err = rmt_config(&config);
    if (err != ESP_OK) {
        Serial.printf("[RMT-TX] rmt_config returned error: %d\n", err);
        rmt_driver_uninstall(config.channel);
        return false;
    }
    s_tx_pin = (int8_t)txPin;
    s_tx_initialized = true;
    Serial.printf("[RMT-TX] RMT TX initialized on pin %d\n", s_tx_pin);
    return true;
}

void SomfyRadioRMT::deinitTx() {
    //if (s_test_running) {
    //    stopTestSignal();
    //}
    if (s_tx_initialized) {
        rmt_driver_uninstall(RMT_TX_CHANNEL);
        s_tx_initialized = false;
        s_tx_pin = -1;
        Serial.println("[RMT-TX] RMT TX deinitialized");
    }
}

void SomfyRadioRMT::endTx() {
    if (s_tx_initialized && s_tx_pin >= 0) {
        pinMode(s_tx_pin, OUTPUT);
        digitalWrite(s_tx_pin, 0);
    }
}

void SomfyRadioRMT::sendFrame(const uint8_t *frame, uint8_t sync, uint8_t bitLength) {
    //if (s_test_running) {
    //    stopTestSignal();
    //}
    if (!s_tx_initialized) {
        Serial.println("[RMT-TX] TX not initialized");
        return;
    }

    bool has_wakeup = (sync == 2 || sync == 12);
    size_t total_items = (has_wakeup ? 1 : 0) + sync + 1 + bitLength + 1;
    if (total_items > MAX_RMT_ITEMS) {
        Serial.printf("[RMT-TX] Frame rejected: requires %u items, max is %u\n", (unsigned)total_items, MAX_RMT_ITEMS);
        return;
    }

    // Ensure RMT claims GPIO pin from standard GPIO before pulsing
    //rmt_set_pin(RMT_TX_CHANNEL, RMT_MODE_TX, (gpio_num_t)s_tx_pin);
    rmt_set_gpio(RMT_TX_CHANNEL, RMT_MODE_TX, (gpio_num_t)s_tx_pin, false);

    rmt_item32_t items[MAX_RMT_ITEMS];
    size_t num_items = 0;

    // Wake-up pulse (only on initial frame, sync == 2 or sync == 12)
    if (sync == 2 || sync == 12) {
        items[num_items++] = makeRmtItem(10920, 1, 7357, 0);
    }

    // Hardware sync pulses (4 * SYMBOL HIGH, 4 * SYMBOL LOW)
    for (uint8_t i = 0; i < sync; i++) {
        items[num_items++] = makeRmtItem(4 * SOMFY_RTS_SYMBOL, 1, 4 * SOMFY_RTS_SYMBOL, 0);
    }

    // Software sync (4850 us HIGH) + Start 0 (SYMBOL LOW)
    items[num_items++] = makeRmtItem(4850, 1, SOMFY_RTS_SYMBOL, 0);

    // Payload (Manchester encoding)
    for (uint8_t i = 0; i < bitLength; i++) {
        uint8_t bit = (frame[i / 8] >> (7 - (i % 8))) & 1;
        items[num_items++] = makeRmtItem(SOMFY_RTS_SYMBOL, bit == 1 ? 0 : 1, SOMFY_RTS_SYMBOL, bit == 1 ? 1 : 0);
    }

    // Inter-frame silence (for 56-bit frames, silence is ~27.4 ms)
    if (bitLength != 80) {
        items[num_items++] = makeRmtItem(27434, 0, 0, 0);
    } else {
        items[num_items++] = makeRmtItem(0, 0, 0, 0);
    }

    Serial.println("[RMT-TX] Transmitting frame...");
    // Transmit via RMT hardware and wait for completion (non-busy RTOS wait)
    esp_err_t err = rmt_write_items(RMT_TX_CHANNEL, items, num_items, false);
    if (err == ESP_OK) {
        err = rmt_wait_tx_done(RMT_TX_CHANNEL, pdMS_TO_TICKS(500));
        if (err == ESP_OK) {
            Serial.println("[RMT-TX] Frame transmitted successfully.");
        } else {
            Serial.printf("[RMT-TX] rmt_wait_tx_done returned error: %d\n", err);
        }
    } else {
        Serial.printf("[RMT-TX] rmt_write_items returned error: %d\n", err);
    }
}

/*
bool SomfyRadioRMT::isTestSignalRunning() {
    return s_test_running;
}

bool SomfyRadioRMT::startTestSignal(uint16_t high_us, uint16_t low_us) {
    if (!s_tx_initialized) {
        Serial.println("[RMT-TX] Cannot start test signal: TX not initialized");
        return false;
    }
    if (s_test_running) {
        stopTestSignal();
    }

    // Ensure RMT claims GPIO pin from standard GPIO
    //rmt_set_pin(RMT_TX_CHANNEL, RMT_MODE_TX, (gpio_num_t)s_tx_pin);
    rmt_set_gpio(RMT_TX_CHANNEL, RMT_MODE_TX, (gpio_num_t)s_tx_pin, false);

    // Enable hardware loop mode in RMT channel
    esp_err_t err = rmt_set_tx_loop_mode(RMT_TX_CHANNEL, true);
    if (err != ESP_OK) {
        Serial.printf("[RMT-TX] rmt_set_tx_loop_mode failed: %d\n", err);
        return false;
    }

    // Single item representing one period of square wave
    rmt_item32_t item = makeRmtItem(high_us, 1, low_us, 0);

    // Write item in loop mode (non-blocking, hardware loops continuously)
    err = rmt_write_items(RMT_TX_CHANNEL, &item, 1, false);
    if (err != ESP_OK) {
        Serial.printf("[RMT-TX] startTestSignal rmt_write_items error: %d\n", err);
        rmt_set_tx_loop_mode(RMT_TX_CHANNEL, false);
        return false;
    }

    s_test_running = true;
    float freq = 1000000.0f / ((float)high_us + (float)low_us);
    Serial.printf("[RMT-TX] *** PERMANENT TEST SIGNAL ACTIVE on GPIO %d: HIGH=%u us, LOW=%u us (%.2f Hz) ***\n",
                  s_tx_pin, high_us, low_us, freq);
    return true;
}

void SomfyRadioRMT::stopTestSignal() {
    if (!s_test_running) {
        return;
    }
    rmt_tx_stop(RMT_TX_CHANNEL);
    rmt_set_tx_loop_mode(RMT_TX_CHANNEL, false);
    s_test_running = false;

    // Restore pin to GPIO OUTPUT LOW
    if (s_tx_pin >= 0) {
        pinMode(s_tx_pin, OUTPUT);
        digitalWrite(s_tx_pin, 0);
    }
    Serial.println("[RMT-TX] *** TEST SIGNAL STOPPED ***");
}
*/
#endif // SOMFY_USE_RMT_TX

#if defined(SOMFY_USE_RMT_RX)
static int8_t s_rx_pin = -1;
static bool s_rx_initialized = false;
static bool s_rx_enabled = false;
static RingbufHandle_t s_rx_ringbuf = nullptr;
static somfy_rx_t s_rx_state;
static somfy_rx_queue_t s_rx_queue;

static void processPulse(uint32_t duration, uint8_t level) {
    if (duration < SOMFY_TEMPO_HALF_SYMBOL_MIN) {
        if (s_rx_state.pulseCount < MAX_TIMINGS && s_rx_state.cpt_synchro_hw > 0) {
            s_rx_state.pulses[s_rx_state.pulseCount++] = duration;
        }
        return;
    }

    switch (s_rx_state.status) {
    case waiting_synchro:
        if (s_rx_state.pulseCount < MAX_TIMINGS) {
            s_rx_state.pulses[s_rx_state.pulseCount++] = duration;
        }
        if (duration >= SOMFY_TEMPO_SYNCHRO_HW_MIN && duration <= SOMFY_TEMPO_SYNCHRO_HW_MAX) {
            ++s_rx_state.cpt_synchro_hw;
            if (s_rx_state.cpt_synchro_hw == 2) {
                Serial.printf("[RMT-RX] HW Sync preamble detected (%u us)\n", duration);
            }
        }
        else if (duration >= SOMFY_TEMPO_SYNCHRO_SW_MIN && duration <= SOMFY_TEMPO_SYNCHRO_SW_MAX && s_rx_state.cpt_synchro_hw >= 4) {
            memset(s_rx_state.payload, 0x00, sizeof(s_rx_state.payload));
            s_rx_state.previous_bit = 0x00;
            s_rx_state.waiting_half_symbol = false;
            s_rx_state.cpt_bits = 0;
            if (s_rx_state.cpt_synchro_hw <= 7) s_rx_state.bit_length = 56;
            else if (s_rx_state.cpt_synchro_hw == 14) s_rx_state.bit_length = 56;
            else if (s_rx_state.cpt_synchro_hw == 13) s_rx_state.bit_length = 80;
            else if (s_rx_state.cpt_synchro_hw == 12) s_rx_state.bit_length = 80;
            else if (s_rx_state.cpt_synchro_hw > 17) s_rx_state.bit_length = 80;
            else s_rx_state.bit_length = 56;
            s_rx_state.status = receiving_data;
            Serial.printf("[RMT-RX] SW Sync OK (%u us, %d HW syncs). Decoding %d-bit frame...\n",
                          duration, s_rx_state.cpt_synchro_hw, s_rx_state.bit_length);
        }
        else {
            if (s_rx_state.cpt_synchro_hw >= 2) {
                Serial.printf("[RMT-RX] HW Sync lost after %d pulses (got %u us)\n", s_rx_state.cpt_synchro_hw, duration);
            }
            s_rx_state.cpt_synchro_hw = 0;
            if (duration >= SOMFY_TEMPO_WAKEUP_MIN && duration <= SOMFY_TEMPO_WAKEUP_MAX) {
                memset(s_rx_state.payload, 0x00, sizeof(s_rx_state.payload));
                s_rx_state.previous_bit = 0x00;
                s_rx_state.waiting_half_symbol = false;
                s_rx_state.cpt_bits = 0;
                s_rx_state.bit_length = 56;
                Serial.printf("[RMT-RX] Wake-up pulse detected (%u us)\n", duration);
            }
            else if ((s_rx_state.pulseCount > 20 && s_rx_state.cpt_synchro_hw == 0) || duration > 250000) {
                s_rx_state.pulseCount = 0;
            }
        }
        break;

    case receiving_data:
        if (s_rx_state.pulseCount < MAX_TIMINGS) {
            s_rx_state.pulses[s_rx_state.pulseCount++] = duration;
        }
        if (duration >= SOMFY_TEMPO_SYMBOL_MIN && duration <= SOMFY_TEMPO_SYMBOL_MAX && !s_rx_state.waiting_half_symbol) {
            s_rx_state.previous_bit = 1 - s_rx_state.previous_bit;
            s_rx_state.payload[s_rx_state.cpt_bits / 8] += s_rx_state.previous_bit << (7 - s_rx_state.cpt_bits % 8);
            ++s_rx_state.cpt_bits;
        }
        else if (duration >= SOMFY_TEMPO_HALF_SYMBOL_MIN && duration <= SOMFY_TEMPO_HALF_SYMBOL_MAX) {
            if (s_rx_state.waiting_half_symbol) {
                s_rx_state.waiting_half_symbol = false;
                s_rx_state.payload[s_rx_state.cpt_bits / 8] += s_rx_state.previous_bit << (7 - s_rx_state.cpt_bits % 8);
                ++s_rx_state.cpt_bits;
            }
            else {
                s_rx_state.waiting_half_symbol = true;
            }
        }
        else {
            // Timing violation: reset to waiting_synchro
            Serial.printf("[RMT-RX] Data error at bit %d/%d (dur=%u us, half=%d). Aborting frame.\n",
                          s_rx_state.cpt_bits, s_rx_state.bit_length, duration, s_rx_state.waiting_half_symbol);
            memset(s_rx_state.payload, 0x00, sizeof(s_rx_state.payload));
            s_rx_state.pulseCount = 1;
            s_rx_state.cpt_synchro_hw = 0;
            s_rx_state.previous_bit = 0x00;
            s_rx_state.waiting_half_symbol = false;
            s_rx_state.cpt_bits = 0;
            s_rx_state.bit_length = 56;
            s_rx_state.status = waiting_synchro;
            s_rx_state.pulses[0] = duration;
        }
        break;

    default:
        break;
    }
}

bool SomfyRadioRMT::initRx(uint8_t rxPin) {
    if (s_rx_initialized && s_rx_pin == (int8_t)rxPin) {
        return true;
    }
    if (s_rx_initialized) {
        deinitRx();
    }

    rmt_config_t config = {};
    config.rmt_mode = RMT_MODE_RX;
    config.channel = RMT_RX_CHANNEL;
    config.gpio_num = (gpio_num_t)rxPin;
    config.mem_block_num = RMT_RX_MEM_BLOCKS;
    config.clk_div = RMT_CLK_DIV;
    config.rx_config.filter_en = true;
    config.rx_config.filter_ticks_thresh = 200;
    config.rx_config.idle_threshold = 13000;

    esp_err_t err = rmt_config(&config);
    if (err != ESP_OK) {
        Serial.printf("[RMT-RX] rmt_config returned error: %d\n", err);
        return false;
    }

    err = rmt_driver_install(config.channel, 2048, 0);
    if (err != ESP_OK) {
        Serial.printf("[RMT-RX] rmt_driver_install returned error: %d\n", err);
        return false;
    }

    err = rmt_get_ringbuf_handle(config.channel, &s_rx_ringbuf);
    if (err != ESP_OK || s_rx_ringbuf == nullptr) {
        Serial.printf("[RMT-RX] rmt_get_ringbuf_handle returned error: %d\n", err);
        rmt_driver_uninstall(config.channel);
        s_rx_ringbuf = nullptr;
        return false;
    }

    s_rx_pin = (int8_t)rxPin;
    s_rx_initialized = true;
    s_rx_enabled = false;
    s_rx_queue.init();
    s_rx_state.clear();
    Serial.printf("[RMT-RX] RMT RX initialized on pin %d (channel %d, %d block(s), filter 200 us, idle 13 ms)\n",
                  s_rx_pin, RMT_RX_CHANNEL, RMT_RX_MEM_BLOCKS);
    return true;
}

void SomfyRadioRMT::enableRx() {
    if (!s_rx_initialized || s_rx_enabled) {
        return;
    }
    if (s_rx_ringbuf != nullptr) {
        size_t size = 0;
        rmt_item32_t *items = nullptr;
        while ((items = (rmt_item32_t *)xRingbufferReceive(s_rx_ringbuf, &size, 0)) != nullptr) {
            vRingbufferReturnItem(s_rx_ringbuf, (void *)items);
        }
    }
    s_rx_state.clear();
    rmt_rx_start(RMT_RX_CHANNEL, true);
    s_rx_enabled = true;
    Serial.println("[RMT-RX] RMT RX reception enabled");
}

void SomfyRadioRMT::disableRx() {
    if (!s_rx_initialized || !s_rx_enabled) {
        return;
    }
    rmt_rx_stop(RMT_RX_CHANNEL);
    s_rx_enabled = false;
    if (s_rx_ringbuf != nullptr) {
        size_t size = 0;
        rmt_item32_t *items = nullptr;
        while ((items = (rmt_item32_t *)xRingbufferReceive(s_rx_ringbuf, &size, 0)) != nullptr) {
            vRingbufferReturnItem(s_rx_ringbuf, (void *)items);
        }
    }
    s_rx_state.clear();
    Serial.println("[RMT-RX] RMT RX reception disabled");
}

void SomfyRadioRMT::deinitRx() {
    if (s_rx_initialized) {
        if (s_rx_enabled) {
            disableRx();
        }
        rmt_driver_uninstall(RMT_RX_CHANNEL);
        s_rx_initialized = false;
        s_rx_ringbuf = nullptr;
        s_rx_pin = -1;
        s_rx_queue.init();
        s_rx_state.clear();
        Serial.println("[RMT-RX] RMT RX deinitialized");
    }
}

bool SomfyRadioRMT::isRxInitialized() {
    return s_rx_initialized;
}

bool SomfyRadioRMT::isRxEnabled() {
    return s_rx_enabled;
}

bool SomfyRadioRMT::isReceiving() {
    return s_rx_enabled && (s_rx_state.cpt_synchro_hw > 0 || s_rx_state.status == receiving_data);
}

bool SomfyRadioRMT::pollReceive(somfy_rx_t *rx) {
    if (!s_rx_initialized || !s_rx_enabled || s_rx_ringbuf == nullptr) {
        return false;
    }

    size_t rx_size = 0;
    rmt_item32_t *items = (rmt_item32_t *)xRingbufferReceive(s_rx_ringbuf, &rx_size, 0);
    //process all received RMT items. push it into the RX queue completed/valid framed detected
    while (items != nullptr) {
        size_t num_items = rx_size / sizeof(rmt_item32_t);
        for (size_t i = 0; i < num_items; i++) {
            if (items[i].duration0 > 0) {
                processPulse(items[i].duration0, items[i].level0);
                if (s_rx_state.status == receiving_data && s_rx_state.cpt_bits >= s_rx_state.bit_length) {
                    Serial.printf("[RMT-RX] Frame complete: %d bits (%d pulses) -> Queued. Payload: %02X %02X %02X %02X %02X %02X %02X\n",
                                  s_rx_state.bit_length, s_rx_state.pulseCount,
                                  s_rx_state.payload[0], s_rx_state.payload[1], s_rx_state.payload[2],
                                  s_rx_state.payload[3], s_rx_state.payload[4], s_rx_state.payload[5],
                                  s_rx_state.payload[6]);
                    s_rx_queue.push(&s_rx_state);
                    s_rx_state.clear();
                }
            }

            if (items[i].duration1 > 0) {
                processPulse(items[i].duration1, items[i].level1);
                if (s_rx_state.status == receiving_data && s_rx_state.cpt_bits >= s_rx_state.bit_length) {
                    Serial.printf("[RMT-RX] Frame complete: %d bits (%d pulses) -> Queued. Payload: %02X %02X %02X %02X %02X %02X %02X\n",
                                  s_rx_state.bit_length, s_rx_state.pulseCount,
                                  s_rx_state.payload[0], s_rx_state.payload[1], s_rx_state.payload[2],
                                  s_rx_state.payload[3], s_rx_state.payload[4], s_rx_state.payload[5],
                                  s_rx_state.payload[6]);
                    s_rx_queue.push(&s_rx_state);
                    s_rx_state.clear();
                }
            }
        }
        vRingbufferReturnItem(s_rx_ringbuf, (void *)items);
        items = (rmt_item32_t *)xRingbufferReceive(s_rx_ringbuf, &rx_size, 0);
    }

    //return just first item from the RX queue or null
    return s_rx_queue.pop(rx);
}
#endif // SOMFY_USE_RMT_RX

#endif // SOMFY_USE_RMT_TX || SOMFY_USE_RMT_RX

