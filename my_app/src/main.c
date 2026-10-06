#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <periph/haptic_iface.h>
#include <periph/mag_iface.h>

LOG_MODULE_REGISTER(my_app, LOG_LEVEL_INF);

int main(void)
{
        struct haptic_dev *hap = haptic_get_default();
        struct mag_dev *mag = mag_get_default();

        if (haptic_init(hap) < 0)
        {
                LOG_ERR("haptic init failed");
                return 0;
        }
        if (mag_init(mag) < 0)
        {
                LOG_ERR("magnetometer init failed");
                return 0;
        }

        while (1)
        {
                struct mag_sample sample;

                mag_sample_fetch(mag);
                mag_get_data(mag, &sample);
                LOG_INF("mag: x=%d y=%d z=%d (0.1 uT)", sample.x, sample.y, sample.z);

                haptic_play_effect(hap, 14);
                k_sleep(K_MSEC(500));
                haptic_stop(hap);

                k_sleep(K_SECONDS(2));
        }

        return 0;
}
