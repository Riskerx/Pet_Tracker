#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/poweroff.h>
#include <zephyr/sys/printk.h>

#define BUTTON_NODE DT_ALIAS(sw0)

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);

static struct gpio_callback button_cb;

// Button interrupt
void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
        k_msleep(100);
        printk("Button Pressed!\n");
}

// Enter System OFF
void enter_system_off(void)
{
        printk("Entering System OFF...\n");
        printk("Press Button 1 to Wake Up\n");

        k_msleep(100);

        // Configure button as wake source
        gpio_pin_interrupt_configure_dt(&button, GPIO_INT_LEVEL_ACTIVE);
        sys_poweroff();
}

int main(void)
{
        int ret;

        printk("\nLow Power Demo Started\n");

        if (!device_is_ready(button.port))
        {
                printk("Button not ready\n");
                return 0;
        }

        // Configure button
        ret = gpio_pin_configure_dt(
            &button,
            GPIO_INPUT);

        if (ret)
        {
                printk("Button config failed\n");
                return 0;
        }

        // Normal interrupt while running
        ret = gpio_pin_interrupt_configure_dt(
            &button,
            GPIO_INT_EDGE_TO_ACTIVE);

        if (ret)
        {
                printk("Interrupt config failed\n");
                return 0;
        }

        gpio_init_callback(&button_cb, button_pressed, BIT(button.pin));

        gpio_add_callback(button.port, &button_cb);

        printk("Running...\n");
        printk("Wait 10 seconds...\n");

        k_sleep(K_SECONDS(10));

        enter_system_off();

        return 0;
}