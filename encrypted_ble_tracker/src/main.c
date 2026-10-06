#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/logging/log.h>
#include "tracker_service.h"
#include "ble/ble_security.h"
#include <zephyr/settings/settings.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static const struct bt_data advertising_data[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
};
static void start_advertising(void);

static struct k_work adv_work;

static void adv_work_handler(struct k_work *work)
{
    start_advertising();
}
 // errif 0= connected successfully
static void on_connected(struct bt_conn *conn, uint8_t err)
{
    if (err != 0)
    {
        LOG_ERR("Connection attempt failed. Error code: %d", err);
        start_advertising();
        return;
    }
    LOG_INF("A device just connected!");
}
//   0x08 → Connection timed out (signal lost)
 //   0x13 → Remote device disconnected (phone closed app)
 //   0x16 → We disconnected ourselves (local host terminated)
static void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("Device disconnected. Reason code: 0x%02X", reason);
    LOG_INF("Re-starting advertising so we can be found again...");
    k_work_submit(&adv_work);
}
// This tells Zephyr which functions to call for connection events.
BT_CONN_CB_DEFINE(connection_callbacks) = {
    .connected    = on_connected,
    .disconnected = on_disconnected,
};
static void start_advertising(void)
{
    int result;
     //the function that actually starts the radio
    result = bt_le_adv_start(
    BT_LE_ADV_CONN_FAST_1,
    advertising_data,
    ARRAY_SIZE(advertising_data),
    NULL,
    0
);
    // Check if advertising failed to start
    if (result != 0)
    {
        LOG_ERR("Failed to start advertising! Error code: %d", result);
        return;
    }
    LOG_INF("Advertising started!");
}
static void on_bluetooth_ready(int err)
{
    if (err != 0)
    {
        LOG_ERR("Bluetooth failed to initialize! Error: %d", err);
        return;
    }
    LOG_INF("Bluetooth hardware is ready!");
    tracker_service_init();

    // Register pairing/bonding behaviour (passkey callbacks, bond-clear button)
    err = ble_security_init();
    if (err != 0)
    {
        LOG_ERR("Failed to initialize BLE security! Error: %d", err);
        return;
    }

    // Restore any bonds saved from a previous session, BEFORE advertising starts
    if (IS_ENABLED(CONFIG_SETTINGS))
    {
        settings_load();
    }

    start_advertising();
}
int main(void)
{
    int result;
    LOG_INF("  TrackerDK BLE Peripheral Starting...  ");
    k_work_init(&adv_work, adv_work_handler);
    result = bt_enable(on_bluetooth_ready);
    if (result != 0)
    {
        LOG_ERR("bt_enable() failed! BLE cannot start. Error: %d", result);
        return result;
    }
    LOG_INF("BLE initialization started, waiting for hardware...");
    while (1)
    {
        LOG_INF("Main thread alive");
        k_sleep(K_SECONDS(5));
    }
    return 0;
}