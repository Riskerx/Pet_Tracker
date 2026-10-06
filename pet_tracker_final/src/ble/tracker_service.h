#ifndef TRACKER_SERVICE_H_
#define TRACKER_SERVICE_H_

#include <stdint.h>
#include "../sensor/motion_sensor.h"

/*
 * Status: REAL.
 *
 * Merge note: the project had two GATT service prototypes sharing
 * the same custom 128-bit UUID base (...cdef0). One was an early
 * single read-only characteristic (tracker_service.c / ble_tracker
 * main.c); the other was a newer Status+Motion notify-based service
 * (ble_notify.c). The newer one supersedes the old one functionally,
 * so the read-only characteristic was retired here rather than kept
 * alongside a service reusing its UUID space. Its Command-style
 * role (trigger an action from the phone) is now covered by the NUS
 * RX channel in data_transfer.c.
 */

typedef enum {
    TRACKER_STATUS_IDLE = 0,
    TRACKER_STATUS_MOVING = 1,
    TRACKER_STATUS_ALERT = 2,
} tracker_status_t;

void tracker_service_init(void);
void tracker_service_on_disconnected(void);

void tracker_service_notify_status(tracker_status_t status);
void tracker_service_notify_motion(const struct motion_sample *sample);

#endif /* TRACKER_SERVICE_H_ */
