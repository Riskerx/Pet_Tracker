#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>
#include "tracker_service.h"

LOG_MODULE_REGISTER(tracker_service, LOG_LEVEL_DBG);

#define MY_SERVICE_UUID BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0))
#define MY_CHAR_UUID    BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1))

static uint8_t sensor_value = 0x42;

static ssize_t on_characteristic_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{
    LOG_INF("Read! Sending value: 0x%02X (%d)", sensor_value, sensor_value);
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &sensor_value, sizeof(sensor_value));
}


BT_GATT_SERVICE_DEFINE(tracker_gatt_service,
    BT_GATT_PRIMARY_SERVICE(MY_SERVICE_UUID),
    BT_GATT_CHARACTERISTIC(MY_CHAR_UUID, BT_GATT_CHRC_READ, BT_GATT_PERM_READ, on_characteristic_read, NULL, NULL),
);

int tracker_service_init(void)
{
    LOG_INF("GATT service ready. Characteristic value = 0x%02X", sensor_value);
    return 0;
}

int tracker_service_set_value(uint8_t new_value)
{
    sensor_value = new_value;
    LOG_INF("Value updated to 0x%02X (%d decimal)", new_value, new_value);
    return 0;
}