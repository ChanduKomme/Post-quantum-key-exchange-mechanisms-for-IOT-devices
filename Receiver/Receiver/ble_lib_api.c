/* ble_rf_stub.c
 * Stub implementation of BLE RF power offset for Wi-Fi–only app.
 * This satisfies hal_board.c's reference without needing the real BLE library.
 */

#include <stdint.h>

/* Signature doesn't need to match exactly, but this is reasonable. */
void ble_rf_set_pwr_offset(const int8_t *pwr_table)
{
    (void)pwr_table;
    // No-op: BLE is not used in this project.
}
