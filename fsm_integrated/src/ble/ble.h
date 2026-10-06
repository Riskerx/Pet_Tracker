#ifndef BLE_H
#define BLE_H

#include <stdint.h>
#include <stdbool.h>

int ble_init(void);
bool ble_is_notifying(void);
extern uint8_t ble_value;
int ble_notify(const uint8_t *data, uint16_t len);

#endif