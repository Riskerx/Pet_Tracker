#include <zephyr/ztest.h>
#include <zephyr/kernel.h>

#include "state_machine/state_machine.h"
#include "storage/flash_log.h"
#include "app_state.h"
#include "fakes.h"

/*
 * These exercise the REAL state_machine.c + REAL flash_log.c together
 * - this is the "state machine + storage" integration path. BLE
 * notify is faked (see fakes.c); everything else is production code.
 */

/* Time for the system workqueue to run the FSM's four chained
 * k_work items (motion -> logging -> ble_notify -> idle) after one
 * state_machine_on_motion_event(). Generous for work items that do
 * no real I/O. */
#define SETTLE_MS 50

static void wait_for_cycle(void)
{
    k_msleep(SETTLE_MS);
}

static void *suite_setup(void)
{
    app_state_init();
    storage_init();
    state_machine_init();
    return NULL;
}

static void case_before(void *fixture)
{
    ARG_UNUSED(fixture);
    fakes_reset();
    storage_erase();
    atomic_set(&ble_transfer_active, 0);
}

ZTEST_SUITE(state_machine_tests, NULL, suite_setup, case_before, NULL, NULL);

ZTEST(state_machine_tests, test_full_cycle_logs_and_notifies)
{
    struct motion_sample sample = { .x = 11, .y = -22, .z = 933 };
    fake_motion_sensor_set_sample(sample);

    state_machine_on_motion_event();
    wait_for_cycle();

    zassert_equal(fake_notify_status_calls, 1, "status should notify exactly once");
    zassert_equal(fake_notify_motion_calls, 1, "motion should notify exactly once");
    zassert_equal(fake_last_status, TRACKER_STATUS_MOVING, "status payload should be MOVING");
    zassert_equal(fake_last_motion.x, sample.x, "notified sample.x should match the sensor");
    zassert_equal(fake_last_motion.y, sample.y, "notified sample.y should match the sensor");
    zassert_equal(fake_last_motion.z, sample.z, "notified sample.z should match the sensor");

    struct tracker_log_record recs[4];
    size_t n = 0;
    storage_read(recs, 4, &n);
    zassert_equal(n, 1, "exactly one record should have been logged");
    zassert_equal(recs[0].sample.x, sample.x, "logged record should match the sensor sample");
}

ZTEST(state_machine_tests, test_repeated_motion_events_each_log_and_notify)
{
    struct motion_sample sample = { .x = 1, .y = 2, .z = 3 };
    fake_motion_sensor_set_sample(sample);

    for (int i = 0; i < 3; i++) {
        state_machine_on_motion_event();
        wait_for_cycle();
    }

    zassert_equal(fake_notify_status_calls, 3, "three motion events should notify status three times");
    zassert_equal(fake_notify_motion_calls, 3, "three motion events should notify motion three times");

    struct tracker_log_record recs[8];
    size_t n = 0;
    storage_read(recs, 8, &n);
    zassert_equal(n, 3, "three motion events should log three records");
}

ZTEST(state_machine_tests, test_notify_skipped_while_ble_transfer_active)
{
    /* Directly exercises the "Notification + Data Transfer" conflict
     * resolution documented in architecture.md. */
    struct motion_sample sample = { .x = 5, .y = 5, .z = 5 };
    fake_motion_sensor_set_sample(sample);

    atomic_set(&ble_transfer_active, 1);

    state_machine_on_motion_event();
    wait_for_cycle();

    zassert_equal(fake_notify_status_calls, 0, "notify should be skipped during a transfer");
    zassert_equal(fake_notify_motion_calls, 0, "notify should be skipped during a transfer");

    struct tracker_log_record recs[4];
    size_t n = 0;
    storage_read(recs, 4, &n);
    zassert_equal(n, 1, "the motion event should still be logged even though notify was skipped");

    atomic_set(&ble_transfer_active, 0);

    /* Proves the skip isn't a stuck/latched flag - the next event
     * after the transfer ends should notify normally. */
    state_machine_on_motion_event();
    wait_for_cycle();

    zassert_equal(fake_notify_status_calls, 1, "notify should resume once the transfer ends");
}
