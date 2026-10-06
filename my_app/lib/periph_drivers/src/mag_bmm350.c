

#include <periph/mag_iface.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <errno.h>

#if DT_HAS_COMPAT_STATUS_OKAY(bosch_bmm350)
#define BMM350_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(bosch_bmm350)
static const struct device *const bmm350_dev = DEVICE_DT_GET(BMM350_NODE);
#else

static const struct device *const bmm350_dev;
#endif

static struct mag_dev handle;

static int real_init(struct mag_dev *dev)
{
    (void)dev;

    if (bmm350_dev == NULL)
    {
        return -ENODEV;
    }
    if (!device_is_ready(bmm350_dev))
    {
        return -ENODEV;
    }
    return 0;
}

static int real_sample_fetch(struct mag_dev *dev)
{
    (void)dev;

    if (bmm350_dev == NULL)
    {
        return -ENODEV;
    }
    return sensor_sample_fetch_chan(bmm350_dev, SENSOR_CHAN_MAGN_XYZ);
}

static int real_get_data(struct mag_dev *dev, struct mag_sample *sample)
{
    struct sensor_value val[3];
    int ret;

    (void)dev;

    if (bmm350_dev == NULL || sample == NULL)
    {
        return -ENODEV;
    }

    ret = sensor_channel_get(bmm350_dev, SENSOR_CHAN_MAGN_XYZ, val);
    if (ret < 0)
    {
        return ret;
    }

    sample->x = val[0].val1 * 10 + val[0].val2 / 100000;
    sample->y = val[1].val1 * 10 + val[1].val2 / 100000;
    sample->z = val[2].val1 * 10 + val[2].val2 / 100000;

    return 0;
}

static const struct mag_api real_api = {
    .init = real_init,
    .sample_fetch = real_sample_fetch,
    .get_data = real_get_data,
};

struct mag_dev *mag_bmm350_get(void)
{
    handle.api = &real_api;
    handle.ctx = NULL;
    return &handle;
}
