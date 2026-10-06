#include <zephyr/ztest.h>

#include "drivers/haptic_stub.h"

ZTEST_SUITE(haptic_stub_tests, NULL, NULL, NULL, NULL, NULL);

ZTEST(haptic_stub_tests, test_init_and_buzz_do_not_crash)
{
    /* Smoke test only - the stub has no observable state to assert on. */
    haptic_init();
    haptic_buzz();
    zassert_true(true, "haptic stub calls should not crash");
}
