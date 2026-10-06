#include <zephyr/ztest.h>
#include <errno.h>
#include <periph/mag_iface.h>
#include <periph/mag_mock.h>

static struct mag_dev *mag;

static void *mag_suite_setup(void)
{
    mag = mag_mock_get();
    return NULL;
}

static void mag_before(void *fixture)
{
    (void)fixture;
    zassert_ok(mag_init(mag), "mock init should succeed with a valid chip id");
}

ZTEST_SUITE(mag_mock, NULL, mag_suite_setup, mag_before, NULL, NULL);

ZTEST(mag_mock, test_init_reads_expected_chip_id)
{
    const struct bmm350_mock_regs *regs = mag_mock_peek_regs();

    zassert_equal(regs->chip_id, 0x33, "BMM350 CHIP_ID must read back 0x33");
    zassert_equal(regs->pmu_cmd, 0x01, "power mode should be NORMAL after init");
}

ZTEST(mag_mock, test_fetch_then_get_returns_a_sample)
{
    struct mag_sample sample;

    zassert_ok(mag_sample_fetch(mag));
    zassert_ok(mag_get_data(mag, &sample));

    zassert_equal(sample.z, 4000, "z axis is fixed in the synthetic model");
}

ZTEST(mag_mock, test_repeated_fetches_are_deterministic)
{
    struct mag_sample first, second;

    zassert_ok(mag_sample_fetch(mag));
    zassert_ok(mag_get_data(mag, &first));

    zassert_ok(mag_init(mag));
    zassert_ok(mag_sample_fetch(mag));
    zassert_ok(mag_get_data(mag, &second));

    zassert_equal(first.x, second.x, "same fetch index must give the same sample");
    zassert_equal(first.y, second.y, "same fetch index must give the same sample");
}

ZTEST(mag_mock, test_get_data_rejects_null_output)
{
    zassert_equal(mag_get_data(mag, NULL), -EINVAL);
}
