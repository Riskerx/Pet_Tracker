

#include <periph/mag_iface.h>
#include <periph/mag_mock.h>
#include <errno.h>
#include <string.h>

#define BMM350_MOCK_CHIP_ID 0x33
#define BMM350_MOCK_PMU_NORMAL 0x01

static struct bmm350_mock_regs regs;
static struct mag_sample last_sample;
static struct mag_dev handle;

static int mock_init(struct mag_dev *dev)
{
    (void)dev;

    memset(&regs, 0, sizeof(regs));
    memset(&last_sample, 0, sizeof(last_sample));

    regs.chip_id = BMM350_MOCK_CHIP_ID;

    if (regs.chip_id != BMM350_MOCK_CHIP_ID)
    {
        return -ENODEV;
    }

    regs.pmu_cmd = BMM350_MOCK_PMU_NORMAL;
    regs.fetch_count = 0;
    return 0;
}

static int mock_sample_fetch(struct mag_dev *dev)
{
    (void)dev;

    regs.fetch_count++;

    last_sample.x = 2500 + (int32_t)(regs.fetch_count % 10) * 10;
    last_sample.y = -1000 - (int32_t)(regs.fetch_count % 5) * 5;
    last_sample.z = 4000;

    return 0;
}

static int mock_get_data(struct mag_dev *dev, struct mag_sample *sample)
{
    (void)dev;

    if (sample == NULL)
    {
        return -EINVAL;
    }
    *sample = last_sample;
    return 0;
}

static const struct mag_api mock_api = {
    .init = mock_init,
    .sample_fetch = mock_sample_fetch,
    .get_data = mock_get_data,
};

struct mag_dev *mag_mock_get(void)
{
    handle.api = &mock_api;
    handle.ctx = &regs;
    return &handle;
}

const struct bmm350_mock_regs *mag_mock_peek_regs(void)
{
    return &regs;
}
