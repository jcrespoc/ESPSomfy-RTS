#if defined(SOMFY_USE_RMT)

#include "SomfyRadioRMT.h"
#include <driver/rmt.h>
#include <esp_err.h>
#include <rom/gpio.h>

#if defined(CONFIG_IDF_TARGET_ESP32C3)
#define RMT_TX_CHANNEL RMT_CHANNEL_0
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
#define RMT_TX_CHANNEL RMT_CHANNEL_0
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#define RMT_TX_CHANNEL RMT_CHANNEL_0
#else // ESP32 classic
#define RMT_TX_CHANNEL RMT_CHANNEL_0
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

static int8_t s_tx_pin = -1;
static bool s_tx_initialized = false;

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
    config.tx_config.idle_output_en = false;

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
        gpio_matrix_out((gpio_num_t)s_tx_pin, 0x100, false, false);
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

    // Transmit via RMT hardware and wait for completion (non-busy RTOS wait)
    esp_err_t err = rmt_write_items(RMT_TX_CHANNEL, items, num_items, false);
    if (err == ESP_OK) {
        err = rmt_wait_tx_done(RMT_TX_CHANNEL, pdMS_TO_TICKS(500));
        if (err != ESP_OK) {
            Serial.printf("[RMT-TX] rmt_wait_tx_done returned error: %d\n", err);
        }
    } else {
        Serial.printf("[RMT-TX] rmt_write_items returned error: %d\n", err);
    }
}

#endif 

