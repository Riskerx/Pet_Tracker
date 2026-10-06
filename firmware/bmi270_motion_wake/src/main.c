
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pet_tracker, LOG_LEVEL_INF);

/* ─────────────────────────────────────────────────────────────────
 * Tunable parameters
 * ───────────────────────────────────────────────────────────────── */
#define SLEEP_TIMEOUT_MS    10000U  /* ms before returning to sleep after motion */
#define ACCEL_ODR_HZ        25      /* accelerometer output data rate            */
#define ACCEL_RANGE_G       2       /* ±2g — captures walk/run/jump cleanly      */


static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS,
                  (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE,
            CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE,
            CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};


static struct k_timer sleep_timer;

static void sleep_timer_handler(struct k_timer *timer)
{
    ARG_UNUSED(timer);
    LOG_INF("Inactivity timeout — stopping BLE advertising");

    int err = bt_le_adv_stop();
    if (err) {
        LOG_WRN("bt_le_adv_stop returned %d", err);
    }


    LOG_INF("Tracker sleeping. Next wake: motion on BMI270 INT1.");
}


static void motion_trigger_handler(const struct device *dev,
                                    const struct sensor_trigger *trig)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(trig);

    LOG_INF("Motion detected! → starting BLE advertising");


    static const struct bt_le_adv_param adv_param =
    BT_LE_ADV_PARAM_INIT(BT_LE_ADV_OPT_CONN,
                          BT_GAP_ADV_FAST_INT_MIN_2,
                          BT_GAP_ADV_FAST_INT_MAX_2,
                          NULL);

int err = bt_le_adv_start(&adv_param,
                           ad, ARRAY_SIZE(ad),
                           sd, ARRAY_SIZE(sd));
    if (err != 0 && err != -EALREADY) {
        LOG_ERR("bt_le_adv_start failed: %d", err);
        return;
    }


    k_timer_start(&sleep_timer, K_MSEC(SLEEP_TIMEOUT_MS), K_NO_WAIT);
}

/* ─────────────────────────────────────────────────────────────────
 * main()
 * ───────────────────────────────────────────────────────────────── */
int main(void)
{
    int err;

    /* ── 1. Acquire BMI270 device from devicetree ─────────────── */
    const struct device *bmi270_dev = DEVICE_DT_GET_ANY(bosch_bmi270);

    if (!device_is_ready(bmi270_dev)) {
        LOG_ERR("BMI270 not ready — verify overlay and I2C connection");
        return -ENODEV;
    }
    LOG_INF("BMI270 ready");

    /* ── 2. Initialize Bluetooth stack ───────────────────────── */
    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("bt_enable failed: %d", err);
        return err;
    }
    LOG_INF("Bluetooth initialized — device name: %s", CONFIG_BT_DEVICE_NAME);

    /* ── 3. Init inactivity timer ─────────────────────────────── */
    k_timer_init(&sleep_timer, sleep_timer_handler, NULL);

    /* ── 4. Configure BMI270 accelerometer ───────────────────── */
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

    err = sensor_trigger_set(bmi270_dev, &motion_trig, motion_trigger_handler);
    if (err) {
        LOG_ERR("sensor_trigger_set (motion) failed: %d", err);
        return err;
    }
    LOG_INF("Any-motion trigger armed (threshold ~83mg, duration 4 samples)");
    LOG_INF("Waiting for motion on BMI270 INT1...");


    while (1) {
        k_sleep(K_FOREVER);
    }

    return 0;
}