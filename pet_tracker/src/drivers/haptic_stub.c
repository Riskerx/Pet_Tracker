#include <zephyr/logging/log.h>

#include "haptic_stub.h"

LOG_MODULE_REGISTER(haptic_stub, LOG_LEVEL_INF);

void haptic_init(void)
{
    LOG_WRN("Haptic driver is STUBBED - DRV2605 not yet wired up on this DK");
}

void haptic_buzz(void)
{
    LOG_INF("Haptic buzz requested (stub - no DRV2605 present)");
}
