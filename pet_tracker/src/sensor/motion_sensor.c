#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

#include "motion_sensor.h"
#include "../app_config.h"
#include "../state_machine/state_machine.h"

LOG_MODULE_REGISTER(motion_sensor, LOG_LEVEL_INF);

#define ACCEL_ODR_HZ   25  /* accelerometer output data rate */
#define ACCEL_RANGE_G  2   /* +/-2g - covers walk/run/jump    */

static struct motion_sample last_sample;
static uint8_t sim_counter;

struct motion_sample motion_sensor_get_last_sample(void)
{
    return last_sample;
}

#if !TRACKER_SIM_MOTION

/* ---- Real path: BMI270 any-motion interrupt ---------------------- */

static const struct device *bmi270_dev;

static void motion_trigger_handler(const struct device *dev,
                                    const struct sensor_trigger *trig)
{
    ARG_UNUSED(trig);

    struct sensor_value ax, ay, az;

    if (sensor_sample_fetch(dev) == 0 &&
        sensor_channel_get(dev, SENSOR_CHAN_ACCEL_X, &ax) == 0 &&
        sensor_channel_get(dev, SENSOR_CHAN_ACCEL_Y, &ay) == 0 &&
        sensor_channel_get(dev, SENSOR_CHAN_ACCEL_Z, &az) == 0) {
        last_sample.x = (int16_t)ax.val1;
        last_sample.y = (int16_t)ay.val1;
        last_sample.z = (int16_t)az.val1;
    } else {
        LOG_WRN("sensor_sample_fetch/channel_get failed - keeping last sample");
    }

    LOG_INF("Motion IRQ - X:%d Y:%d Z:%d", last_sample.x, last_sample.y, last_sample.z);
    state_machine_on_motion_event();
}

int motion_sensor_init(void)
{
    bmi270_dev = DEVICE_DT_GET_ANY(bosch_bmi270);

    if (!device_is_ready(bmi270_dev)) {
        LOG_ERR("BMI270 not ready - verify overlay and I2C wiring");
        return -ENODEV;
    }
    LOG_INF("BMI270 ready");

    struct sensor_value odr   = { .val1 = ACCEL_ODR_HZ, .val2 = 0 };
    struct sensor_value range = { .val1 = ACCEL_RANGE_G, .val2 = 0 };

    sensor_attr_set(bmi270_dev, SENSOR_CHAN_ACCEL_XYZ,
                     SENSOR_ATTR_SAMPLING_FREQUENCY, &odr);
    sensor_attr_set(bmi270_dev, SENSOR_CHAN_ACCEL_XYZ,
                     SENSOR_ATTR_FULL_SCALE, &range);

    static struct sensor_trigger motion_trig = {
        .type = SENSOR_TRIG_MOTION,
        .chan = SENSOR_CHAN_ACCEL_XYZ,
    };

    int err = sensor_trigger_set(bmi270_dev, &motion_trig, motion_trigger_handler);
    if (err) {
        LOG_ERR("sensor_trigger_set failed: %d", err);
        return err;
    }

    LOG_INF("Any-motion trigger armed - waiting for motion on BMI270 INT1");
    return 0;
}

#else /* TRACKER_SIM_MOTION */

/* ---- Simulated path: periodic timer, no hardware required --------- */

static struct k_timer sim_motion_timer;

static void sim_motion_timer_handler(struct k_timer *timer)
{
    ARG_UNUSED(timer);

    sim_counter++;
    last_sample.x = (int16_t)(sim_counter * 12);
    last_sample.y = (int16_t)(sim_counter * -7);
    last_sample.z = (int16_t)(1000 + sim_counter * 3);

    LOG_INF("Simulated motion - X:%d Y:%d Z:%d", last_sample.x, last_sample.y, last_sample.z);
    state_machine_on_motion_event();
}

int motion_sensor_init(void)
{
    LOG_WRN("TRACKER_SIM_MOTION=1 - no BMI270 present, using simulated motion "
            "every 5s (first one in 2s). Flip this in app_config.h once hardware "
            "is wired up.");

    k_timer_init(&sim_motion_timer, sim_motion_timer_handler, NULL);
    k_timer_start(&sim_motion_timer, K_SECONDS(2), K_SECONDS(5));
    return 0;
}

#endif /* TRACKER_SIM_MOTION */
