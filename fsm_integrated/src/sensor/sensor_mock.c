#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "sensor/sensor.h"

LOG_MODULE_REGISTER(sensor, LOG_LEVEL_INF);

static const struct gpio_dt_spec button =
    GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);

static struct gpio_callback button_cb;

static sensor_motion_cb_t motion_cb;

static void button_pressed(
    const struct device *dev,
    struct gpio_callback *cb,
    uint32_t pins)
{
    if (motion_cb)
    {
        motion_cb();
    }
}

int sensor_init(sensor_motion_cb_t callback)
{
    motion_cb = callback;

    gpio_pin_configure_dt(
        &button,
        GPIO_INPUT);

    gpio_pin_interrupt_configure_dt(
        &button,
        GPIO_INT_EDGE_TO_ACTIVE);

    gpio_init_callback(
        &button_cb,
        button_pressed,
        BIT(button.pin));

    gpio_add_callback(
        button.port,
        &button_cb);

    LOG_INF("SW0 Ready");

    return 0;
}