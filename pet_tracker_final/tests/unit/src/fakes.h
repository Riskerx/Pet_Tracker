#ifndef FAKES_H_
#define FAKES_H_

#include "ble/tracker_service.h"
#include "sensor/motion_sensor.h"

/*
 * Test doubles for the two things a host-based (native_sim) test
 * genuinely can't exercise for real: the BLE radio (tracker_service's
 * notify functions) and a physical/simulated sensor (motion_sensor's
 * sample source). Everything else under test - state machine,
 * storage, power flags, OTA stub - is the REAL production code.
 */

extern int fake_notify_status_calls;
extern tracker_status_t fake_last_status;

extern int fake_notify_motion_calls;
extern struct motion_sample fake_last_motion;

/* Sets what the next motion_sensor_get_last_sample() call returns. */
void fake_motion_sensor_set_sample(struct motion_sample sample);

/* Resets every counter/value above. Call from a ztest "before" hook. */
void fakes_reset(void);

#endif /* FAKES_H_ */
