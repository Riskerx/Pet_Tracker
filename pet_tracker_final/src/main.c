#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app_state.h"
#include "state_machine/state_machine.h"
#include "ble/ble_adv.h"
#include "sensor/motion_sensor.h"
#include "sensor/motion_button.h"
#include "storage/flash_log.h"
#include "power/power_mgmt.h"
#include "ota/ota_stub.h"
#include "drivers/haptic_stub.h"

LOG_MODULE_REGISTER(app_main, LOG_LEVEL_INF);

/*
 * Single init chain, replacing five separate main()s from the
 * standalone modules. Order matters:
 *   1. app_state / storage / power / ota / haptic - no dependencies
 *      on Bluetooth or motion, safe to bring up first.
 *   2. state_machine - MUST be initialized before anything that can
 *      call state_machine_on_motion_event() (motion_sensor's real
 *      BMI270 IRQ, its simulated timer, or motion_button's GPIO IRQ
 *      below). That call submits a k_work item, and k_work_submit()
 *      on an item that hasn't been k_work_init()'d yet is undefined
 *      behavior. Originally this ran AFTER motion_sensor_init(),
 *      which is a real (if extremely narrow) boot-order race - found
 *      while writing the test suite, see TEST_REPORT.md.
 *   3. ble_adv - owns bt_enable(); tracker_service + data_transfer
 *      are initialized from inside its ready callback so GATT/NUS
 *      registration always happens after the BT stack is up.
 *   4. motion_sensor / motion_button - both safe to arm now that the
 *      state machine can actually accept an event.
 */
int main(void)
{
    LOG_INF("=== Pet Tracker RC booting ===");

    app_state_init();
    storage_init();
    power_mgmt_init();
    ota_init();
    haptic_init();

    state_machine_init();

    ble_adv_init();

    int err = motion_sensor_init();
    if (err) {
        LOG_ERR("motion_sensor_init failed: %d (continuing - BLE/state machine still run)", err);
    }

    motion_button_init();

    LOG_INF("Setup complete - RC pipeline running");

    k_sleep(K_FOREVER);
    return 0;
}
