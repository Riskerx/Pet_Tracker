#include <zephyr/logging/log.h>

#include "ota_stub.h"
#include "../power/power_mgmt.h"

LOG_MODULE_REGISTER(ota_stub, LOG_LEVEL_INF);

void ota_init(void)
{
    LOG_WRN("OTA is STUBBED - no MCUboot/SMP DFU backend integrated yet. "
            "ota_start() only demonstrates the power_mgmt lock handshake.");
}

void ota_start(void)
{
    LOG_INF("OTA start requested (stub)");
    power_ota_lock();

    /* TODO: real image transfer + MCUboot swap goes here. */
    LOG_INF("OTA stub: nothing to transfer - immediately releasing lock");

    power_ota_unlock();
}
