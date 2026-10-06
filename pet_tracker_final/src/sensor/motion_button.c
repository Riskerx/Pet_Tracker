#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "motion_button.h"
#include "../state_machine/state_machine.h"

LOG_MODULE_REGISTER(motion_button, LOG_LEVEL_INF);

#if DT_NODE_EXISTS(DT_ALIAS(sw0))

#define SW0_NODE DT_ALIAS(sw0)

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
static struct gpio_callback button_cb_data;

static void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    LOG_INF("Button pressed - injecting a motion event (bench test trigger)");
    state_machine_on_motion_event();
}

int motion_button_init(void)
{
    if (!gpio_is_ready_dt(&button)) {
        LOG_WRN("Button device not ready - motion button trigger disabled");
        return -ENODEV;
    }

    int err = gpio_pin_configure_dt(&button, GPIO_INPUT);
    if (err) {
        LOG_ERR("gpio_pin_configure_dt failed: %d", err);
        return err;
    }

    err = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
    if (err) {
        LOG_ERR("gpio_pin_interrupt_configure_dt failed: %d", err);
        return err;
    }

    gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
    gpio_add_callback(button.port, &button_cb_data);

    LOG_INF("Motion button trigger ready - press SW1 (button0) to simulate motion");
    return 0;
}

#else /* !DT_NODE_EXISTS(DT_ALIAS(sw0)) */

int motion_button_init(void)
{
    LOG_WRN("No \"sw0\" devicetree alias on this board - motion button trigger skipped");
    return -ENOTSUP;
}

#endif
