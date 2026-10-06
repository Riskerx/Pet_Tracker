#include <periph/haptic_iface.h>
#include <zephyr/device.h>
#include <zephyr/drivers/haptics.h>
#include <zephyr/drivers/haptics/drv2605.h>
#include <errno.h>

#if DT_HAS_COMPAT_STATUS_OKAY(ti_drv2605)
#define DRV2605_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(ti_drv2605)
static const struct device *const drv2605_dev = DEVICE_DT_GET(DRV2605_NODE);
#else

static const struct device *const drv2605_dev;
#endif

static struct haptic_dev handle;

static int real_init(struct haptic_dev *dev)
{
    (void)dev;

    if (drv2605_dev == NULL)
    {
        return -ENODEV;
    }
    if (!device_is_ready(drv2605_dev))
    {
        return -ENODEV;
    }
    return 0;
}

static int real_play_effect(struct haptic_dev *dev, uint8_t effect_id)
{
    (void)dev;

    if (drv2605_dev == NULL)
    {
        return -ENODEV;
    }
    if (effect_id < HAPTIC_EFFECT_MIN || effect_id > HAPTIC_EFFECT_MAX)
    {
        return -EINVAL;
    }

    const union drv2605_config_data config = {
        .rom_data = {
            .library = DRV2605_LIBRARY_LRA,
            .seq_regs = {effect_id, 0x00},
        },
    };

    int ret = drv2605_haptic_config(drv2605_dev, DRV2605_HAPTICS_SOURCE_ROM,
                                    &config);
    if (ret < 0)
    {
        return ret;
    }

    return haptics_start_output(drv2605_dev);
}

static int real_stop(struct haptic_dev *dev)
{
    (void)dev;

    if (drv2605_dev == NULL)
    {
        return -ENODEV;
    }
    return haptics_stop_output(drv2605_dev);
}

static bool real_is_playing(const struct haptic_dev *dev)
{
    (void)dev;

    return false;
}

static const struct haptic_api real_api = {
    .init = real_init,
    .play_effect = real_play_effect,
    .stop = real_stop,
    .is_playing = real_is_playing,
};

struct haptic_dev *haptic_drv2605_get(void)
{
    handle.api = &real_api;
    handle.ctx = NULL;
    return &handle;
}
