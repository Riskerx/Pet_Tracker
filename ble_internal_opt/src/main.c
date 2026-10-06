#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gap.h>

#define CONN_INTERVAL_MIN  400 
#define CONN_INTERVAL_MAX  800 
#define CONN_LATENCY       4   
#define CONN_TIMEOUT       1500 

static void start_advertising(void);

static void on_connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        printf("Connection failed (err 0x%02x)\n", err);
        return;
    }
    printf("Connected!\n");

    struct bt_le_conn_param conn_params = {
        .interval_min = CONN_INTERVAL_MIN,
        .interval_max = CONN_INTERVAL_MAX,
        .latency      = CONN_LATENCY,
        .timeout      = CONN_TIMEOUT,
    };
    bt_conn_le_param_update(conn, &conn_params);
    printf("Requested: 500-1000ms interval, latency=4, 15s timeout\n");

    struct bt_conn_le_phy_param phy_params = {
        .options     = BT_CONN_LE_PHY_OPT_NONE,
        .pref_tx_phy = BT_GAP_LE_PHY_2M,
        .pref_rx_phy = BT_GAP_LE_PHY_2M,
    };
    bt_conn_le_phy_update(conn, &phy_params);
    printf("Requested: 2M PHY\n");
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
    printf("Disconnected (reason 0x%02x)\n", reason);
    start_advertising();          /* ← fixed: added missing semicolon */
}

static void on_params_updated(struct bt_conn *conn, uint16_t interval,
                               uint16_t latency, uint16_t timeout)
{
    printf("Params now: interval=%ums, latency=%u, timeout=%ums\n",
           interval * 5 / 4, latency, timeout * 10);
}

static bool on_params_requested(struct bt_conn *conn, struct bt_le_conn_param *param)
{
    return true;
}

static void on_phy_updated(struct bt_conn *conn, struct bt_conn_le_phy_info *info)
{
    printf("PHY now: TX=%u RX=%u (1=1M, 2=2M)\n", info->tx_phy, info->rx_phy);
}

BT_CONN_CB_DEFINE(callbacks) = {
    .connected        = on_connected,
    .disconnected     = on_disconnected,
    .le_param_updated = on_params_updated,
    .le_param_req     = on_params_requested,
    .le_phy_updated   = on_phy_updated,
};

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static void start_advertising(void)
{
    int err = bt_le_adv_start(
        BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONN,
                         BT_GAP_ADV_FAST_INT_MIN_2,
                         BT_GAP_ADV_FAST_INT_MAX_2,
                         NULL),
        ad, ARRAY_SIZE(ad), NULL, 0);

    if (err) {
        printf("Advertising failed to start (err %d)\n", err);
        return;
    }
    printf("Advertising started, waiting for connection...\n");
}

int main(void)
{
    bt_enable(NULL);
    printf("Bluetooth ready\n");
    start_advertising();

    while (1) {
        k_sleep(K_FOREVER);
    }
    return 0;
}