# fsm_integrated

Firmware for the **nRF52840 DK** that integrates a Finite State Machine (FSM) with BLE notifications using the nRF Connect SDK (NCS) v3.3.0 / Zephyr RTOS 4.x.

---

## Overview

This project demonstrates a state-machine-driven BLE peripheral. The FSM controls application states and triggers BLE notify packets to a connected central (phone or laptop) whenever a state transition occurs.

---

## Features

- Finite State Machine using `k_timer` and `k_work` for state transitions
- BLE GATT service with a notify characteristic and CCCD handling
- Notifications only sent when a central has subscribed (CCCD enabled)
- Modular source layout — BLE service and notify logic separated into their own files
- Structured logging via Zephyr `LOG_MODULE_REGISTER`

---

## Hardware

| Item | Details |
|---|---|
| Board | Nordic nRF52840 DK |
| SDK | nRF Connect SDK v3.3.0 |
| RTOS | Zephyr 4.x |
| Toolchain | arm-zephyr-eabi-gcc 12.2.0 |

---

## Project Structure

```
fsm_integrated/
├── src/
│   ├── main.c              # FSM logic, k_timer, k_work
│   └── ble/
│       ├── ble.h           # Public BLE API
│       ├── ble_service.c   # bt_enable, advertising, GATT service definition
│       └── ble_notify.c    # ble_notify() — sends GATT notification
├── boards/
│   └── nrf52840dk_nrf52840.overlay  # Board-specific DTS overlay
├── CMakeLists.txt
├── prj.conf
└── README.md
```

---

## BLE Service

| Attribute | Value |
|---|---|
| Service UUID | `0x180A` |
| Characteristic UUID | `0x2A57` |
| Properties | Notify |
| CCCD | Read / Write |

The central must write `0x0001` to the CCCD to enable notifications.

---

## Building

```bash
west build -b nrf52840dk/nrf52840
```

## Flashing

```bash
west flash
```

## Monitoring logs (RTT)

```bash
west espressif monitor   # or use RTT Viewer / nRF Terminal in VS Code
```

---

## Key Zephyr 4.x API Notes

These breaking changes from Zephyr 4.0 apply to this project:

- `BT_LE_ADV_OPT_CONNECTABLE` → renamed to `BT_LE_ADV_OPT_CONN`
- `BT_LE_ADV_OPT_USE_NAME` → removed; device name must be included manually in the `ad[]` payload using `BT_DATA_NAME_COMPLETE`

---

## Author

Rishikesh — Electronics and Communication Engineering  
GitHub: [@Riskerx](https://github.com/Riskerx)
