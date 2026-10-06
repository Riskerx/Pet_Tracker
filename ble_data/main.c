#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/addr.h>
#include <bluetooth/services/nus.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(simple_ble, LOG_LEVEL_INF);

#define LOG_SIZE 1000
uint8_t log_data[LOG_SIZE];
static struct bt_conn *current_conn = NULL;

static const struct bt_data ad[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE,
            CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

const struct bt_le_adv_param *adv_param =
    BT_LE_ADV_PARAM(
        BT_LE_ADV_OPT_CONN,
        BT_GAP_ADV_FAST_INT_MIN_2,
        BT_GAP_ADV_FAST_INT_MAX_2,
        NULL);

/* Wrapped so we can call it again after a disconnect */
static void start_advertising(void)
{
        int err = bt_le_adv_start(adv_param, ad, ARRAY_SIZE(ad), NULL, 0);
        if (err)
        {
                LOG_ERR("Advertising failed to start (err %d)", err);
                return;
        }
        LOG_INF("Advertising started, device name: %s", CONFIG_BT_DEVICE_NAME);
}

void send_log_data(void)
{
        const int CHUNK_SIZE = 20;
        int offset = 0;

        LOG_INF("Starting data transfer (%d bytes total)", LOG_SIZE);

        while (offset < LOG_SIZE)
        {
                int bytes_left = LOG_SIZE - offset;
                int chunk_length = (bytes_left > CHUNK_SIZE) ? CHUNK_SIZE : bytes_left;
                int err = bt_nus_send(current_conn, &log_data[offset], chunk_length);
                if (err)
                {
                        LOG_ERR("Failed to send chunk at offset %d (err %d)", offset, err);
                        return;
                }
                LOG_INF("Sent %d bytes (offset %d/%d)", chunk_length, offset, LOG_SIZE);
                offset += chunk_length;
                k_msleep(20);
        }
        LOG_INF("Data transfer complete");
}

void nus_received_cb(struct bt_conn *conn, const uint8_t *data, uint16_t len)
{
        LOG_INF("Received %u bytes over NUS", len);

        if (len >= 4 && memcmp(data, "DUMP", 4) == 0)
        {
                LOG_INF("DUMP command received");
                send_log_data();
        }
        else
        {
                LOG_WRN("Unrecognized command received");
        }
}

static struct bt_nus_cb nus_cb = {
    .received = nus_received_cb,
};

void connected(struct bt_conn *conn, uint8_t err)
{
        char addr[BT_ADDR_LE_STR_LEN];

        if (err)
        {
                LOG_ERR("Connection failed (err %u)", err);
                return;
        }

        current_conn = bt_conn_ref(conn);
        bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
        LOG_INF("BLE connected: %s", addr);
}

void disconnected(struct bt_conn *conn, uint8_t reason)
{
        LOG_INF("BLE disconnected (reason 0x%02x)", reason);

        if (current_conn)
        {
                bt_conn_unref(current_conn);
                current_conn = NULL;
        }

        /* Without this, the device stops being connectable after the first disconnect */
        start_advertising();
}

static struct bt_conn_cb conn_callbacks = {
    .connected = connected,
    .disconnected = disconnected,
};

int main(void)
{
        int err;

        LOG_INF("BLE Started");

        for (int i = 0; i < LOG_SIZE; i++)
        {
                log_data[i] = i % 256;
        }

        err = bt_enable(NULL);
        if (err)
        {
                LOG_ERR("bt_enable failed (err %d)", err);
                return err;
        }
        LOG_INF("Bluetooth initialized");

        bt_conn_cb_register(&conn_callbacks);

        err = bt_nus_init(&nus_cb);
        if (err)
        {
                LOG_ERR("bt_nus_init failed (err %d)", err);
                return err;
        }
        LOG_INF("NUS service initialized");

        start_advertising();

        return 0;
}