#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "storage/storage.h"
#include "ble/ble.h"
#include "app/state_machine.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

int main(void)
{
        LOG_INF("Pet Tracker booting.....");

        int rc = storage_init();
        if (rc)
        {
                LOG_WRN("Storage init failed (%d) — continuing without flash", rc);
        }

        rc = ble_init();
        if (rc)
        {
                LOG_ERR("BLE init failed: %d", rc);
                return rc;
        }

        rc = state_machine_start();
        if (rc)
        {
                LOG_ERR("FSM start failed: %d", rc);
                return rc;
        }

        LOG_INF("Boot complete — press SW0 to trigger motion");

        k_sleep(K_FOREVER);
        return 0;
}