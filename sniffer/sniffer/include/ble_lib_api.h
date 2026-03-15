#ifndef BLE_LIB_API_H
#define BLE_LIB_API_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Dummy BLE library API for Wi-Fi-only firmware.
 * hal_board.c includes "ble_lib_api.h" and calls BLE controller functions.
 * We provide no-op static inline stubs so:
 *  - compile: functions are declared
 *  - link   : no external symbol is needed
 */

#include <stdint.h>

static inline void ble_controller_init(void) {}
static inline void ble_controller_deinit(void) {}

static inline void ble_controller_sleep(void) {}
static inline void ble_controller_wakeup(void) {}

static inline void ble_controller_get_mac(uint8_t mac[6])
{
    if (!mac) return;
    for (int i = 0; i < 6; i++) {
        mac[i] = 0;
    }
}

/* NEW: stub for the function hal_board.c is calling */
static inline void ble_controller_set_tx_pwr(const int8_t *pwr_table)
{
    (void)pwr_table;
    /* no-op */
}

/* If the build later complains about any other ble_controller_*,
   just add another static inline stub here the same way. */

#ifdef __cplusplus
}
#endif

#endif /* BLE_LIB_API_H */

