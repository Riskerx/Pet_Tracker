#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>

#include "ble/ble.h"

LOG_MODULE_REGISTER(ble_notify, LOG_LEVEL_INF);

extern uint8_t ble_value;

extern const struct bt_gatt_service_static my_service;

int ble_notify(const uint8_t *data,
               uint16_t len)
{
    if (!ble_is_notifying())
    {
        LOG_WRN("Phone Not Subscribed");
        return -1;
    }

    /* Store first byte */
    ble_value = data[0];

    int err = bt_gatt_notify(
        NULL,
        &my_service.attrs[2],
        &ble_value,
        sizeof(ble_value));

    if (err)
    {
        LOG_ERR("Notify Failed");
    }
    else
    {
        LOG_INF("Notification Sent");
    }

    return err;
}