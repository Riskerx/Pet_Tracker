# Pet Tracker Firmware - Release Candidate

Consolidated, single-build version of the Pet Tracker firmware. Everything
that used to live in separate per-week modules (state machine, BLE service,
notifications, chunked data transfer, motion sensing, power management, OTA
and driver scaffolding) now lives in one project and runs as one pipeline.

This is a Release Candidate: it's believed to be feature-complete for the
current milestone, not a final release. See **Known Limitations** below for
what's still stubbed pending the physical DK.

## Features

- BLE peripheral (single advertising/connection owner, one `bt_enable()`)
- GATT tracker service - Status + Motion notify characteristics
- Chunked log/data transfer over Nordic UART Service (NUS)
- Motion-driven state machine (`IDLE -> MOTION_DETECTED -> LOGGING -> BLE_NOTIFY -> IDLE`)
- Flash logging interface (currently RAM-backed, see Known Limitations)
- Inactivity-based power management with OTA/BLE guards
- Bonding/pairing enabled at the Kconfig level (SMP + settings)
- OTA and haptic (DRV2605) hook points, stubbed

## Build

Built with nRF Connect SDK v3.3.0 / Zephyr RTOS 4.x, same Docker-based build
environment used for the rest of this project.

```
west build -b nrf52840dk/nrf52840
```

## Flash

```
west flash
```

## Connect

Use **nRF Connect for Mobile**. The device advertises as `RishiTracker`.
Subscribe to the Status and Motion characteristics to see notifications;
write `DUMP` to the NUS RX characteristic to trigger a chunked log dump over
NUS TX.

## Project Structure

```
pet_tracker/
├── src/
│   ├── main.c              single init chain for every module
│   ├── app_state.{c,h}     shared mutex/flags used to resolve conflicts below
│   ├── app_config.h        TRACKER_SIM_MOTION build-time switch
│   ├── state_machine/      motion -> logging -> BLE notify -> idle
│   ├── ble/
│   │   ├── ble_adv.{c,h}         advertising + connection callbacks (single owner)
│   │   ├── tracker_service.{c,h} GATT Status/Motion notify characteristics
│   │   └── data_transfer.{c,h}   NUS chunked log dump
│   ├── sensor/              BMI270 real driver + simulated-motion fallback
│   ├── storage/             flash logging interface (RAM-backed stub)
│   ├── power/                inactivity sleep + OTA/BLE guards
│   ├── ota/                  OTA hook points (stub)
│   └── drivers/              DRV2605 haptic hook point (stub)
├── CMakeLists.txt
├── prj.conf
├── README.md
└── architecture.md
```

## Real vs. Stubbed

| Module                              | Status |
|--------------------------------------|--------|
| State machine                        | ✅ Real |
| BLE advertising & connection mgmt    | ✅ Real |
| BLE GATT service (Status/Motion)     | ✅ Real |
| BLE data transfer (NUS chunked dump) | ✅ Real |
| Power management                     | ✅ Real |
| Security / bonding                   | ⚠ Kconfig-enabled only - passkey/pairing callback code not in this consolidation pass |
| Motion sensor (BMI270)               | ⚠ Real driver code, not yet validated on physical hardware |
| Flash logging                        | ⚠ Stub - RAM ring buffer behind the real `storage_*` API |
| OTA                                  | ⚠ Stub - no DFU backend |
| Haptic driver (DRV2605)              | ⚠ Stub |

## Known Limitations

- **Flash logging** is a 64-record RAM ring buffer; it is lost on reset.
  Swapping in `zephyr/fs/nvs.h` only requires changing `storage/flash_log.c` -
  every caller goes through the same `storage_save()/storage_read()/storage_erase()`
  interface.
- **BMI270 driver** code is ported as-is and hasn't run on a physical DK yet.
  `app_config.h` defaults to `TRACKER_SIM_MOTION 1`, which drives the same
  pipeline off a periodic software timer instead. Flip that to `0` once the
  sensor is wired up.
- **Bonding** is enabled at the Kconfig level (`CONFIG_BT_SMP`,
  `CONFIG_BT_BONDABLE`, `CONFIG_BT_SETTINGS`) but the passkey-display pairing
  flow built and verified earlier in this project isn't among the five
  source files this RC was consolidated from, so it isn't wired into
  `ble_adv.c` yet. Drop that module in and it slots into the existing
  connection-callback file.
- **Extended advertising** (`bt_le_ext_adv_create()`, used earlier to fix an
  `-ENOMEM` on advertising restart) isn't reflected here either, for the same
  reason - this RC's advertising path uses legacy `bt_le_adv_start()`, matching
  the source files it was built from.
- **OTA** and **haptic (DRV2605)** are hook points only - see `ota/` and
  `drivers/`.
- Not build-tested in this pass (no Zephyr toolchain available in the
  environment this was assembled in) - run a `west build` locally as the next
  step before tagging the RC.

## Integration Conflicts

See `architecture.md` for the full list of what fought when these modules
were combined and how each was resolved.
