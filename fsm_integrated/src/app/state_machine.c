#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app/state_machine.h"
#include "sensor/sensor.h"
#include "storage/storage.h"
#include "ble/ble.h"

LOG_MODULE_REGISTER(fsm, LOG_LEVEL_INF);

typedef enum
{
    STATE_IDLE,
    STATE_MOTION_DETECTED,
    STATE_LOGGING,
    STATE_BLE_NOTIFY

} state_t;

static state_t current_state = STATE_IDLE;

static struct k_work motion_work;
static struct k_work logging_work;
static struct k_work ble_work;
static struct k_work idle_work;

static struct storage_record record;

static void motion_handler(struct k_work *work)
{
    current_state = STATE_MOTION_DETECTED;

    LOG_INF("Motion Detected");

    k_work_submit(&logging_work);
}

static void logging_handler(struct k_work *work)
{
    current_state = STATE_LOGGING;

    LOG_INF("Saving Record");

    record.uptime_ms = k_uptime_get_32();

    storage_save(&record);

    k_work_submit(&ble_work);
}

static void ble_handler(struct k_work *work)
{
    current_state = STATE_BLE_NOTIFY;

    LOG_INF("Sending BLE Notification");

    ble_notify((uint8_t *)&record,
               sizeof(record));

    k_work_submit(&idle_work);
}

static void idle_handler(struct k_work *work)
{
    current_state = STATE_IDLE;

    LOG_INF("Back To IDLE");
}

static void on_motion(void)
{
    k_work_submit(&motion_work);
}

int state_machine_start(void)
{
    k_work_init(&motion_work, motion_handler);
    k_work_init(&logging_work, logging_handler);
    k_work_init(&ble_work, ble_handler);
    k_work_init(&idle_work, idle_handler);

    int rc = sensor_init(on_motion);

    if (rc)
    {
        LOG_ERR("Sensor Init Failed");
        return rc;
    }

    LOG_INF("FSM Started");

    return 0;
}