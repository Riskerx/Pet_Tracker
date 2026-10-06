#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>
#include "ble/ble.h"

LOG_MODULE_REGISTER(ble, LOG_LEVEL_INF);

static bool notify_enabled = false;
uint8_t ble_value;
bool ble_is_notifying(void)
{
    return notify_enabled;
}

/* ── Advertising payload ── */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS,
                  BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    BT_DATA(BT_DATA_NAME_COMPLETE,
            CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static void ccc_changed(const struct bt_gatt_attr *attr,
                        uint16_t value)
{
    notify_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Notifications %s",
            notify_enabled ? "Enabled" : "Disabled");
}

BT_GATT_SERVICE_DEFINE(my_service,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_16(0x180A)),
    BT_GATT_CHARACTERISTIC(
        BT_UUID_DECLARE_16(0x2A57),
        BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_NONE,
        NULL, NULL,
        &ble_value),
    BT_GATT_CCC(ccc_changed,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE));

int ble_init(void)
{
    int err;

    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth Failed (%d)", err);
        return err;
    }
    LOG_INF("Bluetooth Started");

    err = bt_le_adv_start(
        BT_LE_ADV_PARAM(
            BT_LE_ADV_OPT_CONN,          /* was BT_LE_ADV_OPT_CONNECTABLE */
            BT_GAP_ADV_FAST_INT_MIN_2,
            BT_GAP_ADV_FAST_INT_MAX_2,
            NULL),
        ad, ARRAY_SIZE(ad),
        NULL, 0);

    if (err) {
        LOG_ERR("Advertising Failed (%d)", err);
        return err;
    }
    LOG_INF("Advertising Started");
    return 0;
}