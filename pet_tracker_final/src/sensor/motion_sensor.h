#ifndef MOTION_SENSOR_H_
#define MOTION_SENSOR_H_

#include <stdint.h>

/* Shared payload shape - reused by storage (flash record) and BLE
 * (motion characteristic notify) so the same reading only ever has
 * one struct definition in the whole project. */
struct motion_sample {
    int16_t x;
    int16_t y;
    int16_t z;
};

/*
 * Status: MIXED.
 *   - Real BMI270 driver bring-up (device get, ODR/range config,
 *     any-motion trigger) ported as-is from the driver prototype.
 *   - Not yet validated on the physical DK (no board on hand yet),
 *     so it's real code on an untested path.
 *   - CONFIG_TRACKER_SIM_MOTION (see app_config.h) switches to a
 *     periodic software timer that fires synthetic motion events
 *     instead, so the rest of the pipeline can be exercised on
 *     native_sim / the DK without a sensor wired up.
 */
int motion_sensor_init(void);

/* Last sample captured (real reading, or synthesized when
 * simulated). Used by storage/flash_log.c and ble/tracker_service.c
 * at LOGGING / BLE_NOTIFY time. */
struct motion_sample motion_sensor_get_last_sample(void);

#endif /* MOTION_SENSOR_H_ */
