#include <zephyr/ztest.h>
#include <zephyr/kernel.h>

#include "storage/flash_log.h"
#include "app_state.h"

#define RECORD(t, xx, yy, zz) \
    (struct tracker_log_record){ .timestamp_ms = (t), .sample = { .x = (xx), .y = (yy), .z = (zz) } }

static void *suite_setup(void)
{
    app_state_init();
    return NULL;
}

static void case_before(void *fixture)
{
    ARG_UNUSED(fixture);
    storage_erase();
}

ZTEST_SUITE(flash_log_tests, NULL, suite_setup, case_before, NULL, NULL);

ZTEST(flash_log_tests, test_save_then_read_single_record)
{
    struct tracker_log_record rec = RECORD(1000, 1, 2, 3);
    zassert_equal(storage_save(&rec), 0, "storage_save should return 0");

    struct tracker_log_record out[1];
    size_t n = 0;
    zassert_equal(storage_read(out, 1, &n), 0, "storage_read should return 0");
    zassert_equal(n, 1, "should read back exactly the one saved record");
    zassert_equal(out[0].timestamp_ms, 1000, "timestamp should round-trip");
    zassert_equal(out[0].sample.x, 1, "sample.x should round-trip");
}

ZTEST(flash_log_tests, test_erase_clears_buffer)
{
    struct tracker_log_record rec = RECORD(1, 1, 1, 1);
    storage_save(&rec);
    storage_erase();

    struct tracker_log_record out[4];
    size_t n = 99;
    storage_read(out, 4, &n);
    zassert_equal(n, 0, "buffer should be empty after erase");
}

ZTEST(flash_log_tests, test_read_caps_at_requested_max)
{
    for (int i = 0; i < 5; i++) {
        struct tracker_log_record rec = RECORD(i, i, i, i);
        storage_save(&rec);
    }

    struct tracker_log_record out[2];
    size_t n = 0;
    storage_read(out, 2, &n);
    zassert_equal(n, 2, "read should cap at the caller's max_records, not the total saved");
}

ZTEST(flash_log_tests, test_ring_buffer_wraparound_keeps_newest)
{
    /* Save more than the 64-record capacity and confirm the oldest
     * ones were the ones overwritten, not the newest. */
    const int total = 70;

    for (int i = 0; i < total; i++) {
        struct tracker_log_record rec = RECORD(i, 0, 0, 0);
        storage_save(&rec);
    }

    struct tracker_log_record out[64];
    size_t n = 0;
    storage_read(out, 64, &n);

    zassert_equal(n, 64, "buffer should be full at capacity after wraparound");
    zassert_equal(out[0].timestamp_ms, total - 64, "oldest surviving record is wrong");
    zassert_equal(out[63].timestamp_ms, total - 1, "newest record is wrong");
}

/* ---- Concurrency: proves storage_lock actually prevents corruption
 * when two threads save at once - this is the "Flash + BLE" conflict
 * from architecture.md, exercised directly rather than just asserted
 * by inspection. ---- */

#define THREAD_STACK_SIZE   1024
#define RECORDS_PER_THREAD  20

K_THREAD_STACK_DEFINE(producer_a_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(producer_b_stack, THREAD_STACK_SIZE);
static struct k_thread producer_a_tid;
static struct k_thread producer_b_tid;

static void producer_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    int32_t tag = (int32_t)(intptr_t)p1;

    for (int i = 0; i < RECORDS_PER_THREAD; i++) {
        struct tracker_log_record rec = RECORD(i, tag, i, 0);
        storage_save(&rec);
    }
}

ZTEST(flash_log_tests, test_concurrent_saves_do_not_corrupt_count)
{
    k_tid_t a = k_thread_create(&producer_a_tid, producer_a_stack, THREAD_STACK_SIZE,
                                 producer_entry, (void *)1, NULL, NULL,
                                 5, 0, K_NO_WAIT);
    k_tid_t b = k_thread_create(&producer_b_tid, producer_b_stack, THREAD_STACK_SIZE,
                                 producer_entry, (void *)2, NULL, NULL,
                                 5, 0, K_NO_WAIT);

    k_thread_join(a, K_FOREVER);
    k_thread_join(b, K_FOREVER);

    struct tracker_log_record out[64];
    size_t n = 0;
    storage_read(out, 64, &n);

    /* 40 saves total, capacity is 64, so nothing should wrap. If the
     * mutex ever let two saves interleave and corrupt head/count,
     * this would come out wrong (or storage_read would crash). */
    zassert_equal(n, RECORDS_PER_THREAD * 2, "no records should be lost or duplicated");
}
