#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "power_mgmt.h"
#include "../app_state.h"

LOG_MODULE_REGISTER(power_mgmt, LOG_LEVEL_INF);

#define SLEEP_TIMEOUT_MS 10000U /* ms of inactivity before we allow a deep sleep */

static struct k_timer sleep_timer;

static void sleep_timer_handler(struct k_timer *timer)
{
    ARG_UNUSED(timer);

    if (atomic_get(&ota_active)) {
        LOG_INF("Sleep deferred - OTA in progress");
        k_timer_start(&sleep_timer, K_MSEC(SLEEP_TIMEOUT_MS), K_NO_WAIT);
        return;
    }

    LOG_INF("Inactivity timeout reached - tracker going idle/low-power");
    /* Real deep-sleep entry (System OFF + GPIO sense wake on BMI270
     * INT1) is board-specific PM config land, not app code - hook
     * point only in this consolidation pass. */
}

void power_mgmt_init(void)
{
    k_timer_init(&sleep_timer, sleep_timer_handler, NULL);
    k_timer_start(&sleep_timer, K_MSEC(SLEEP_TIMEOUT_MS), K_NO_WAIT);
    LOG_INF("Power management ready (sleep after %u ms idle)", SLEEP_TIMEOUT_MS);
}

void power_notify_activity(void)
{
    k_timer_start(&sleep_timer, K_MSEC(SLEEP_TIMEOUT_MS), K_NO_WAIT);
}

void power_ota_lock(void)
{
    atomic_set(&ota_active, 1);
    k_timer_stop(&sleep_timer);
    LOG_INF("Sleep locked out - OTA active");
}

void power_ota_unlock(void)
{
    atomic_set(&ota_active, 0);
    power_notify_activity();
    LOG_INF("Sleep re-armed - OTA finished");
}
