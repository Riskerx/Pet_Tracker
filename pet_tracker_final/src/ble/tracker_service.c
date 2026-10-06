#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>

#include "tracker_service.h"

LOG_MODULE_REGISTER(tracker_service, LOG_LEVEL_INF);

#define TRACKER_SVC_UUID_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)
#define STATUS_CHAR_UUID_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1)
#define MOTION_CHAR_UUID_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef2)

static struct bt_uuid_128 tracker_svc_uuid = BT_UUID_INIT_128(TRACKER_SVC_UUID_VAL);
static struct bt_uuid_128 status_char_uuid = BT_UUID_INIT_128(STATUS_CHAR_UUID_VAL);
static struct bt_uuid_128 motion_char_uuid = BT_UUID_INIT_128(MOTION_CHAR_UUID_VAL);

static bool status_notify_enabled;
static bool motion_notify_enabled;

static void status_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    ARG_UNUSED(attr);
    status_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("[CCCD] Status notifications %s", status_notify_enabled ? "ENABLED" : "DISABLED");
}

static void motion_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    ARG_UNUSED(attr);
    motion_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("[CCCD] Motion notifications %s", motion_notify_enabled ? "ENABLED" : "DISABLED");
}

BT_GATT_SERVICE_DEFINE(tracker_svc,
    BT_GATT_PRIMARY_SERVICE(&tracker_svc_uuid.uuid),

    BT_GATT_CHARACTERISTIC(&status_char_uuid.uuid,
        BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE, NULL, NULL, NULL),
    BT_GATT_CCC(status_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    BT_GATT_CHARACTERISTIC(&motion_char_uuid.uuid,
        BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE, NULL, NULL, NULL),
    BT_GATT_CCC(motion_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

/* Attribute indices: [0]=svc [1]=status chrc decl [2]=status value
 * [3]=status CCC [4]=motion chrc decl [5]=motion value [6]=motion CCC */
#define STATUS_VALUE_ATTR (&tracker_svc.attrs[2])
#define MOTION_VALUE_ATTR (&tracker_svc.attrs[5])

void tracker_service_init(void)
{
    LOG_INF("GATT tracker service registered");
}

void tracker_service_on_disconnected(void)
{
    status_notify_enabled = false;
    motion_notify_enabled = false;
}

void tracker_service_notify_status(tracker_status_t status)
{
    if (!status_notify_enabled) {
        LOG_DBG("[STATUS] No subscriber - skipped");
        return;
    }

    uint8_t value = (uint8_t)status;
    int err = bt_gatt_notify(NULL, STATUS_VALUE_ATTR, &value, sizeof(value));
    if (err) {
        LOG_ERR("[STATUS] Notify failed (err %d)", err);
    } else {
        LOG_INF("[STATUS] Notified -> %u", value);
    }
}

void tracker_service_notify_motion(const struct motion_sample *sample)
{
    if (!motion_notify_enabled) {
        LOG_DBG("[MOTION] No subscriber - skipped");
        return;
    }

    int err = bt_gatt_notify(NULL, MOTION_VALUE_ATTR, sample, sizeof(*sample));
    if (err) {
        LOG_ERR("[MOTION] Notify failed (err %d)", err);
    } else {
        LOG_INF("[MOTION] Notified -> X:%d Y:%d Z:%d", sample->x, sample->y, sample->z);
    }
}
