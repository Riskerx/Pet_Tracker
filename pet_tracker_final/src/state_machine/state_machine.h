#ifndef STATE_MACHINE_H_
#define STATE_MACHINE_H_

/* Status: REAL. Ported from the standalone FSM prototype (native_sim
 * tested) with two changes: the simulated timer moved out to
 * sensor/motion_sensor.c, and the LOGGING / BLE_NOTIFY states now
 * call the real storage and BLE modules instead of just logging. */

typedef enum {
    TRACKER_STATE_IDLE = 0,
    TRACKER_STATE_MOTION_DETECTED,
    TRACKER_STATE_LOGGING,
    TRACKER_STATE_BLE_NOTIFY,
} tracker_state_t;

void state_machine_init(void);

/* Entry point for ANY motion source - the real BMI270 trigger or the
 * simulated timer in sensor/motion_sensor.c both call this. Starts
 * the IDLE -> MOTION_DETECTED -> LOGGING -> BLE_NOTIFY -> IDLE chain. */
void state_machine_on_motion_event(void);

#endif /* STATE_MACHINE_H_ */
