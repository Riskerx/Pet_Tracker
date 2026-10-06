#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/logging/log.h>
#include "tracker_service.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);


// ADVERTISING DATA
static const struct bt_data advertising_data[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR), BT_DATA_BYTES(BT_DATA_NAME_COMPLETE,'R','i','s','h','i','T', 'r', 'a', 'c', 'k', 'e', 'r'),
};

static void start_advertising(void);

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
    start_advertising();
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
    result = bt_le_adv_start(BT_LE_ADV_NCONN, advertising_data, ARRAY_SIZE(advertising_data), NULL, 0);

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
    start_advertising();
}

int main(void)
{
    int result;
    LOG_INF("  TrackerDK BLE Peripheral Starting...  ");
    result = bt_enable(on_bluetooth_ready);
    if (result != 0)
    {
        LOG_ERR("bt_enable() failed! BLE cannot start. Error: %d", result);
        return result;
    }
    LOG_INF("BLE initialization started, waiting for hardware...");
    while (1)
    {
        k_sleep(K_FOREVER);
    }
    return 0;
}
