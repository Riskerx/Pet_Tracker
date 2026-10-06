#ifndef APP_STATE_H_
#define APP_STATE_H_

#include <zephyr/kernel.h>

/*
 * Shared coordination primitives.
 *
 * These exist ONLY because the individual modules were built in
 * isolation and, once combined, touch the same resources at the
 * same time. See architecture.md -> "Integration Conflicts" for the
 * story behind each one.
 */

/* Guards storage_save() / storage_read() / storage_erase() so a BLE
 * log dump can never read the flash/RAM log while the state machine
 * is mid-write to it.  (Conflict: "Flash + BLE") */
extern struct k_mutex storage_lock;

/* Set for the duration of an OTA image transfer. power_mgmt refuses
 * to let the CPU sleep while this is set.  (Conflict: "OTA + Power") */
extern atomic_t ota_active;

/* Set for the duration of a BLE log/data transfer (NUS chunk send).
 * The state machine checks this before firing a new BLE_NOTIFY
 * transition so a fresh motion notification can never interleave
 * with an in-progress log dump.  (Conflict: "Notification + Data
 * Transfer") */
extern atomic_t ble_transfer_active;

void app_state_init(void);

#endif /* APP_STATE_H_ */
