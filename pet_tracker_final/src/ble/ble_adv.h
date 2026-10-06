#ifndef BLE_ADV_H_
#define BLE_ADV_H_

#include <stdbool.h>
#include <zephyr/bluetooth/conn.h>

/* Status: REAL.
 *
 * This is the ONE place bt_enable() and BT_CONN_CB_DEFINE happen.
 * The original modules each did their own bt_enable() + advertising
 * + connection-callback setup, which cannot coexist in one build.
 * See architecture.md -> Integration Conflicts for the merge notes.
 */

void ble_adv_init(void);
void ble_adv_start(void);
void ble_adv_stop(void);
bool ble_adv_is_connected(void);

/* NULL when not connected. Data transfer needs the live conn object
 * for bt_nus_send(); the notify-based characteristics don't (they
 * pass NULL to bt_gatt_notify and let the stack fan out to whoever
 * is subscribed). */
struct bt_conn *ble_adv_get_conn(void);

#endif /* BLE_ADV_H_ */
