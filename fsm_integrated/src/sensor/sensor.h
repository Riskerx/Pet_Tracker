#ifndef SENSOR_H
#define SENSOR_H

typedef void (*sensor_motion_cb_t)(void);

int sensor_init(sensor_motion_cb_t on_motion);

#endif