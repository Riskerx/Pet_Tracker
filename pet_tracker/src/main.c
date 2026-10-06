#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app_state.h"
#include "state_machine/state_machine.h"
#include "ble/ble_adv.h"
#include "sensor/motion_sensor.h"
#include "storage/flash_log.h"
#include "power/power_mgmt.h"
#include "ota/ota_stub.h"
#include "drivers/haptic_stub.h"

LOG_MODULE_REGISTER(app_main, LOG_LEVEL_INF);

/*
 * Single init chain, replacing five separate main()s from the
 * standalone modules. Order matters:
 *   1. app_state / storage / power / ota / haptic - no dependencies
 *      on Bluetooth, safe to bring up first.
 *   2. ble_adv - owns bt_enable(); tracker_service + data_transfer
 *      are initialized from inside its ready callback so GATT/NUS
 *      registration always happens after the BT stack is up.
 *   3. motion_sensor - starts producing motion events (real IRQ or
 *      simulated timer), which the state machine below is now ready
 *      to receive.
 *   4. state_machine - initialized last so nothing can fire a
 *      transition before its work items exist.
 */
int main(void)
{
    LOG_INF("=== Pet Tracker RC booting ===");

    app_state_init();
    storage_init();
    power_mgmt_init();
    ota_init();
    haptic_init();

    ble_adv_init();

    int err = motion_sensor_init();
    if (err) {
        LOG_ERR("motion_sensor_init failed: %d (continuing - BLE/state machine still run)", err);
    }

    state_machine_init();

    LOG_INF("Setup complete - RC pipeline running");

    k_sleep(K_FOREVER);
    return 0;
}
