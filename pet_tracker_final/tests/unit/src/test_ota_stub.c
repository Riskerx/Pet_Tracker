#include <zephyr/ztest.h>
#include <zephyr/kernel.h>

#include "ota/ota_stub.h"
#include "power/power_mgmt.h"
#include "app_state.h"

static void *suite_setup(void)
{
    power_mgmt_init();
    ota_init();
    return NULL;
}

ZTEST_SUITE(ota_stub_tests, NULL, suite_setup, NULL, NULL, NULL);

ZTEST(ota_stub_tests, test_ota_start_completes_and_releases_the_lock)
{
    /*
     * ota_start() is currently synchronous - locks, logs, and
     * immediately unlocks, since there's no real image transfer
     * behind it yet. This proves the power_mgmt lock/unlock handshake
     * completes cleanly. It does NOT prove anything about a real
     * transfer or rollback being safe - there's no MCUboot/DFU
     * backend to test yet. See TEST_REPORT.md, "Known gaps".
     */
    ota_start();
    zassert_equal(atomic_get(&ota_active), 0,
                  "ota_active should be released once ota_start() returns");
}
