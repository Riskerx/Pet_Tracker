#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "state_machine.h"
#include "../app_state.h"
#include "../sensor/motion_sensor.h"
#include "../storage/flash_log.h"
#include "../ble/tracker_service.h"

LOG_MODULE_REGISTER(state_machine, LOG_LEVEL_INF);

static const char *const state_names[] = {
    [TRACKER_STATE_IDLE] = "IDLE",
    [TRACKER_STATE_MOTION_DETECTED] = "MOTION_DETECTED",
    [TRACKER_STATE_LOGGING] = "LOGGING",
    [TRACKER_STATE_BLE_NOTIFY] = "BLE_NOTIFY",
};

static atomic_t fsm_state = ATOMIC_INIT(TRACKER_STATE_IDLE);
static atomic_t setup_done = ATOMIC_INIT(0);

static struct k_work motion_work;
static struct k_work logging_work;
static struct k_work ble_notify_work;
static struct k_work idle_work;

static bool fsm_transition(tracker_state_t from, tracker_state_t to)
{
    if (atomic_cas(&fsm_state, (atomic_val_t)from, (atomic_val_t)to)) {
        LOG_INF("FSM  %-16s -->  %-16s", state_names[from], state_names[to]);
        return true;
    }

    LOG_WRN("FSM  transition %s->%s rejected (current = %s)",
            state_names[from], state_names[to],
            state_names[(tracker_state_t)atomic_get(&fsm_state)]);
    return false;
}

/* IDLE -> MOTION_DETECTED */
static void motion_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    if (!atomic_get(&setup_done)) {
        LOG_WRN("Motion event arrived before setup complete - dropped");
        return;
    }

    if (fsm_transition(TRACKER_STATE_IDLE, TRACKER_STATE_MOTION_DETECTED)) {
        k_work_submit(&logging_work);
    }
}

/* MOTION_DETECTED -> LOGGING */
static void logging_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    if (fsm_transition(TRACKER_STATE_MOTION_DETECTED, TRACKER_STATE_LOGGING)) {
        struct tracker_log_record rec = {
            .timestamp_ms = k_uptime_get_32(),
            .sample = motion_sensor_get_last_sample(),
        };
        storage_save(&rec);
        k_work_submit(&ble_notify_work);
    }
}

/* LOGGING -> BLE_NOTIFY */
static void ble_notify_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    if (fsm_transition(TRACKER_STATE_LOGGING, TRACKER_STATE_BLE_NOTIFY)) {
        if (atomic_get(&ble_transfer_active)) {
            /* A log dump is mid-flight over NUS - don't interleave a
             * fresh notification with it (see app_state.h). Still
             * complete the transition and drop straight through to
             * idle; the phone will pick up the new record on its
             * next dump. */
            LOG_WRN("BLE busy with a data transfer - skipping this notify");
        } else {
            tracker_service_notify_status(TRACKER_STATUS_MOVING);
            struct motion_sample sample = motion_sensor_get_last_sample();
            tracker_service_notify_motion(&sample);
        }
        k_work_submit(&idle_work);
    }
}

/* BLE_NOTIFY -> IDLE */
static void idle_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    if (fsm_transition(TRACKER_STATE_BLE_NOTIFY, TRACKER_STATE_IDLE)) {
        LOG_INF("Back to idle, waiting for next motion event");
    }
}

void state_machine_on_motion_event(void)
{
    k_work_submit(&motion_work);
}

void state_machine_init(void)
{
    LOG_INF("Initial state: %s", state_names[(tracker_state_t)atomic_get(&fsm_state)]);

    k_work_init(&motion_work, motion_work_handler);
    k_work_init(&logging_work, logging_work_handler);
    k_work_init(&ble_notify_work, ble_notify_work_handler);
    k_work_init(&idle_work, idle_work_handler);

    atomic_set(&setup_done, 1);
    LOG_INF("State machine ready");
}
