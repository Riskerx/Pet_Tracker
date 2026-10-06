#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/logging/log.h>

#include "ble_adv.h"
#include "tracker_service.h"
#include "data_transfer.h"
#include "../power/power_mgmt.h"

LOG_MODULE_REGISTER(ble_adv, LOG_LEVEL_INF);

/* CONFIG_BT_DEVICE_NAME is the single source of truth for the
 * advertised name (set in prj.conf). The original modules each
 * hardcoded a different name/param set ("RishiTracker" bytes here,
 * CONFIG_BT_DEVICE_NAME there, no name at all elsewhere) - unified
 * to one here. */
static const struct bt_data adv_data[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
};

static const struct bt_data scan_rsp_data[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static struct bt_conn *current_conn;

/* Restarting advertising from inside the disconnect callback with a
 * blocking k_sleep() (as one of the prototypes did) ties up the BT
 * connection-callback context. Deferring it to a delayable work item
 * avoids that and matches the k_work-based -ENOMEM fix already used
 * elsewhere in the project. */
static struct k_work_delayable restart_adv_work;

bool ble_adv_is_connected(void)
{
    return current_conn != NULL;
}

struct bt_conn *ble_adv_get_conn(void)
{
    return current_conn;
}

void ble_adv_start(void)
{
    static const struct bt_le_adv_param adv_param =
        BT_LE_ADV_PARAM_INIT(BT_LE_ADV_OPT_CONN | BT_LE_ADV_OPT_USE_IDENTITY,
                              BT_GAP_ADV_FAST_INT_MIN_2,
                              BT_GAP_ADV_FAST_INT_MAX_2,
                              NULL);

    int err = bt_le_adv_start(&adv_param,
                               adv_data, ARRAY_SIZE(adv_data),
                               scan_rsp_data, ARRAY_SIZE(scan_rsp_data));
    if (err && err != -EALREADY) {
        LOG_ERR("Advertising start failed (err %d)", err);
        return;
    }
    LOG_INF("Advertising as \"%s\"", CONFIG_BT_DEVICE_NAME);
}

void ble_adv_stop(void)
{
    int err = bt_le_adv_stop();
    if (err) {
        LOG_WRN("bt_le_adv_stop returned %d", err);
    }
}

static void restart_adv_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    ble_adv_start();
}

static void on_connected(struct bt_conn *conn, uint8_t err)
{
    char addr[BT_ADDR_LE_STR_LEN];

    if (err) {
        LOG_ERR("Connection failed (err 0x%02X)", err);
        ble_adv_start();
        return;
    }

    current_conn = bt_conn_ref(conn);
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    LOG_INF("Connected: %s", addr);

    power_notify_activity();
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
    ARG_UNUSED(conn);
    LOG_INF("Disconnected (reason 0x%02X) - restarting advertising", reason);

    if (current_conn) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }

    tracker_service_on_disconnected();
    data_transfer_on_disconnected();

    /* Short delay mirrors the original prototype's intent (let the
     * link fully tear down before re-advertising) without blocking
     * this callback. */
    k_work_schedule(&restart_adv_work, K_MSEC(500));
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = on_connected,
    .disconnected = on_disconnected,
};

static void bt_ready_cb(int err)
{
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return;
    }
    LOG_INF("Bluetooth initialized");

    /* GATT service + NUS must both be registered AFTER the stack is
     * ready, and BEFORE advertising starts. Doing this here, in one
     * place, is what removes the "which module calls bt_enable()
     * and in what order" conflict. */
    tracker_service_init();
    data_transfer_init();
    ble_adv_start();
}

void ble_adv_init(void)
{
    k_work_init_delayable(&restart_adv_work, restart_adv_work_handler);

    int err = bt_enable(bt_ready_cb);
    if (err) {
        LOG_ERR("bt_enable failed (err %d)", err);
    }
}
