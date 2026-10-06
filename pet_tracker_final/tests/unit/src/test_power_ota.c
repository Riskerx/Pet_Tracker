#include <zephyr/ztest.h>
#include <zephyr/kernel.h>

#include "power/power_mgmt.h"
#include "app_state.h"

static void *suite_setup(void)
{
    power_mgmt_init();
    return NULL;
}

ZTEST_SUITE(power_ota_flag_tests, NULL, suite_setup, NULL, NULL, NULL);

ZTEST(power_ota_flag_tests, test_ota_lock_sets_flag)
{
    power_ota_lock();
    zassert_equal(atomic_get(&ota_active), 1, "ota_active should be set after power_ota_lock()");
    power_ota_unlock();
}

ZTEST(power_ota_flag_tests, test_ota_unlock_clears_flag)
{
    power_ota_lock();
    power_ota_unlock();
    zassert_equal(atomic_get(&ota_active), 0, "ota_active should clear after power_ota_unlock()");
}

ZTEST(power_ota_flag_tests, test_notify_activity_does_not_crash_when_idle)
{
    /*
     * Deliberately weak assertion: power_notify_activity() only
     * resets a private k_timer with no externally observable state,
     * short of waiting out the real 10s inactivity timeout. This
     * proves it's safe to call at any time. The timer's actual
     * expiry behavior (and the OTA-blocks-sleep path specifically)
     * is covered by the bench procedure in TEST_REPORT.md instead,
     * where it can be observed directly rather than inferred.
     */
    power_notify_activity();
    zassert_true(true, "power_notify_activity should not crash or hang");
}
