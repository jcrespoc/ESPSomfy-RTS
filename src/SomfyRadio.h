#ifndef SOMFY_RADIO_H
#define SOMFY_RADIO_H

#include <Arduino.h>

/**
 * Feature flags for radio driver implementation.
 * Can be defined via platformio.ini build_flags:
 *   -D SOMFY_USE_RMT       (enables both TX and RX with RMT)
 *   -D SOMFY_USE_RMT_TX    (enables RMT for TX only)
 *   -D SOMFY_USE_RMT_RX    (enables RMT for RX only)
 *
 * If none are defined, the legacy bit-banging TX and GPIO interrupt RX are used.
 */
#if defined(SOMFY_USE_RMT)
  #ifndef SOMFY_USE_RMT_TX
    #define SOMFY_USE_RMT_TX 1
  #endif
  #ifndef SOMFY_USE_RMT_RX
    #define SOMFY_USE_RMT_RX 1
  #endif
#endif

// Base RTS timing constant in microseconds
#ifndef SOMFY_RTS_SYMBOL
  #define SOMFY_RTS_SYMBOL 640
#endif

#define SOMFY_RTS_TOLERANCE_MIN 0.7f
#define SOMFY_RTS_TOLERANCE_MAX 1.3f

#define SOMFY_TEMPO_WAKEUP_MIN       ((uint32_t)(9415 * SOMFY_RTS_TOLERANCE_MIN))
#define SOMFY_TEMPO_WAKEUP_MAX       ((uint32_t)(9415 * SOMFY_RTS_TOLERANCE_MAX))
#define SOMFY_TEMPO_SYNCHRO_HW_MIN   ((uint32_t)(SOMFY_RTS_SYMBOL * 4 * SOMFY_RTS_TOLERANCE_MIN))
#define SOMFY_TEMPO_SYNCHRO_HW_MAX   ((uint32_t)(SOMFY_RTS_SYMBOL * 4 * SOMFY_RTS_TOLERANCE_MAX))
#define SOMFY_TEMPO_SYNCHRO_SW_MIN   ((uint32_t)(4850 * SOMFY_RTS_TOLERANCE_MIN))
#define SOMFY_TEMPO_SYNCHRO_SW_MAX   ((uint32_t)(4850 * SOMFY_RTS_TOLERANCE_MAX))
#define SOMFY_TEMPO_HALF_SYMBOL_MIN  ((uint32_t)(SOMFY_RTS_SYMBOL * SOMFY_RTS_TOLERANCE_MIN))
#define SOMFY_TEMPO_HALF_SYMBOL_MAX  ((uint32_t)(SOMFY_RTS_SYMBOL * SOMFY_RTS_TOLERANCE_MAX))
#define SOMFY_TEMPO_SYMBOL_MIN       ((uint32_t)(SOMFY_RTS_SYMBOL * 2 * SOMFY_RTS_TOLERANCE_MIN))
#define SOMFY_TEMPO_SYMBOL_MAX       ((uint32_t)(SOMFY_RTS_SYMBOL * 2 * SOMFY_RTS_TOLERANCE_MAX))

#endif // SOMFY_RADIO_H

