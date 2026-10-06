#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_notify_demo, LOG_LEVEL_INF);

#define TRACKER_SVC_UUID_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)

#define STATUS_CHAR_UUID_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1)

#define MOTION_CHAR_UUID_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef2)

static struct bt_uuid_128 tracker_svc_uuid = BT_UUID_INIT_128(TRACKER_SVC_UUID_VAL);
static struct bt_uuid_128 status_char_uuid  = BT_UUID_INIT_128(STATUS_CHAR_UUID_VAL);
static struct bt_uuid_128 motion_char_uuid  = BT_UUID_INIT_128(MOTION_CHAR_UUID_VAL);

static bool status_notify_enabled;
static bool motion_notify_enabled;

static struct bt_conn *current_conn;
static struct k_work_delayable notify_work;

static void status_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    status_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("[CCCD] Status notifications %s",
            status_notify_enabled ? "ENABLED" : "DISABLED");
            if (status_notify_enabled || motion_notify_enabled) {
    k_work_reschedule(&notify_work, K_SECONDS(1));
}
}

static void motion_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    motion_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("[CCCD] Motion notifications %s",
            motion_notify_enabled ? "ENABLED" : "DISABLED");
            if (status_notify_enabled || motion_notify_enabled) {
    k_work_reschedule(&notify_work, K_SECONDS(1));
}
}

BT_GATT_SERVICE_DEFINE(tracker_svc,
    
    BT_GATT_PRIMARY_SERVICE(&tracker_svc_uuid.uuid),

    BT_GATT_CHARACTERISTIC(&status_char_uuid.uuid,
        BT_GATT_CHRC_NOTIFY,                        
        BT_GATT_PERM_NONE,                          
        NULL, NULL, NULL),                         

    BT_GATT_CCC(status_ccc_changed,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),   

    BT_GATT_CHARACTERISTIC(&motion_char_uuid.uuid,
        BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_NONE,
        NULL, NULL, NULL),

    BT_GATT_CCC(motion_ccc_changed,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

typedef enum {
    TRACKER_IDLE   = 0,
    TRACKER_MOVING = 1,
    TRACKER_ALERT  = 2,
} tracker_status_t;

struct motion_payload {
    int16_t x;
    int16_t y;
    int16_t z;
} __packed;

static uint8_t sim_counter;

static struct k_work_delayable notify_work;

static void send_notifications(struct k_work *work)
{
    int err;
    sim_counter++;


    if (status_notify_enabled) {

        uint8_t status = (uint8_t)(sim_counter % 3);
        const char *labels[] = { "IDLE", "MOVING", "ALERT" };

        err = bt_gatt_notify(NULL, &tracker_svc.attrs[2],
                             &status, sizeof(status));
        if (err == 0) {
            LOG_INF("[STATUS] Notified → %s (0x%02X)", labels[status], status);
        } else {
            LOG_ERR("[STATUS] Notify failed (err %d)", err);
        }
    } else {

        LOG_DBG("[STATUS] No subscriber — skipped");
    }

    if (motion_notify_enabled) {
        struct motion_payload motion = {
            .x = (int16_t)(sim_counter * 12),
            .y = (int16_t)(sim_counter * -7),
            .z = (int16_t)(1000 + sim_counter * 3),
        };
        err = bt_gatt_notify(NULL, &tracker_svc.attrs[5],
                             &motion, sizeof(motion));
        if (err == 0) {
            LOG_INF("[MOTION] Notified → X:%d Y:%d Z:%d",
                    motion.x, motion.y, motion.z);
        } else {
            LOG_ERR("[MOTION] Notify failed (err %d)", err);
        }
    } else {
        LOG_DBG("[MOTION] No subscriber — skipped");
    }


    if (current_conn != NULL) {
        k_work_reschedule(&notify_work, K_SECONDS(2));
    }
}


static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS,
                  (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
};

static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE,
            CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static void start_advertising(void)
{
int err = bt_le_adv_start(
    BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONN | BT_LE_ADV_OPT_USE_IDENTITY,
                    BT_GAP_ADV_FAST_INT_MIN_2,
                    BT_GAP_ADV_FAST_INT_MAX_2,
                    NULL),
    ad, ARRAY_SIZE(ad),
    sd, ARRAY_SIZE(sd));
    if (err) {
        LOG_ERR("Advertising start failed (err %d)", err);
        return;
    }
    LOG_INF("Advertising as \"%s\" — waiting for nRF Connect...",
            CONFIG_BT_DEVICE_NAME);
}

static void on_connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_ERR("Connection failed (err 0x%02X)", err);
        return;
    }

    current_conn = bt_conn_ref(conn);
    LOG_INF("Client connected — waiting for CCCD subscribe...");

    k_work_reschedule(&notify_work, K_SECONDS(1));
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("Client disconnected (reason 0x%02X)", reason);

    if (current_conn) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }

    status_notify_enabled = false;
    motion_notify_enabled = false;

    k_work_cancel_delayable(&notify_work);

    k_sleep(K_MSEC(500));   // temporary test

    start_advertising();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected    = on_connected,
    .disconnected = on_disconnected,
};

static void bt_ready_cb(int err)
{
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return;
    }
    LOG_INF("Bluetooth initialized");
    start_advertising();
}

int main(void)
{
    LOG_INF("Tracker Notify starting...");

    k_work_init_delayable(&notify_work, send_notifications);

    int err = bt_enable(bt_ready_cb);
    if (err) {
        LOG_ERR("bt_enable failed (err %d)", err);
        return err;
    }

    return 0;
}
