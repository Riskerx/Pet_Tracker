#include "fakes.h"

int fake_notify_status_calls;
tracker_status_t fake_last_status;

int fake_notify_motion_calls;
struct motion_sample fake_last_motion;

static struct motion_sample injected_sample;

/* ---- fakes for ble/tracker_service.h ---- */

void tracker_service_notify_status(tracker_status_t status)
{
    fake_notify_status_calls++;
    fake_last_status = status;
}

void tracker_service_notify_motion(const struct motion_sample *sample)
{
    fake_notify_motion_calls++;
    fake_last_motion = *sample;
}

/* ---- fake for sensor/motion_sensor.h ---- */

struct motion_sample motion_sensor_get_last_sample(void)
{
    return injected_sample;
}

void fake_motion_sensor_set_sample(struct motion_sample sample)
{
    injected_sample = sample;
}

void fakes_reset(void)
{
    fake_notify_status_calls = 0;
    fake_notify_motion_calls = 0;
    fake_last_status = 0;

    struct motion_sample zero = { 0 };
    fake_last_motion = zero;
    injected_sample = zero;
}
