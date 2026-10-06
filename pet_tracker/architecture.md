# Architecture - Pet Tracker RC

Design overview, not code. See README.md for build/flash instructions.

## Module Map

```
                              +----------------+
                              |     main()     |
                              +----------------+
                                       |
                          initializes everything, in order
                                       |
      -----------------------------------------------------------------
      |          |          |          |          |          |        |
  app_state   storage     power       ota      haptic    ble_adv   sensor
      |                                                     |          |
      |                                                tracker_svc  motion_
      |                                                data_transfer sensor
      |                                                     |          |
      -----------------------------state machine-------------------------
                                       |
                               application logic
```

`ble_adv.c` owns `bt_enable()`. Its ready callback initializes
`tracker_service` and `data_transfer` before starting advertising, so GATT/NUS
registration always happens in the same order relative to the BT stack
becoming ready - see "Integration Conflicts" below for why that matters.

## Modules

### State Machine (`state_machine/`)
Drives the whole pipeline: `IDLE -> MOTION_DETECTED -> LOGGING -> BLE_NOTIFY -> IDLE`,
implemented as `k_work` items chained off an `atomic_t` state variable
(`atomic_cas` guards each transition). LOGGING calls `storage_save()`;
BLE_NOTIFY calls the tracker service's notify functions, deferring if a data
transfer is in flight.

- **Interface:** `state_machine_init()`, `state_machine_on_motion_event()`
- **Status:** Real

### BLE - Advertising (`ble/ble_adv.c`)
Single owner of `bt_enable()`, `BT_CONN_CB_DEFINE`, and the advertising
parameters/name. Restarts advertising after disconnect via a delayable work
item (not a blocking sleep).

- **Interface:** `ble_adv_init()`, `ble_adv_start()`, `ble_adv_stop()`,
  `ble_adv_is_connected()`, `ble_adv_get_conn()`
- **Status:** Real

### BLE - Tracker Service (`ble/tracker_service.c`)
Custom GATT service: Status characteristic (notify) and Motion characteristic
(notify), each with a CCC descriptor.

- **Interface:** `tracker_service_init()`, `tracker_service_notify_status()`,
  `tracker_service_notify_motion()`, `tracker_service_on_disconnected()`
- **Status:** Real

### BLE - Data Transfer (`ble/data_transfer.c`)
Nordic UART Service (NUS). RX callback recognizes a `DUMP` command and
submits a `k_work` item; the work handler reads records from `storage/` and
sends them over NUS TX in 20-byte chunks, paced with `k_msleep(20)`.

- **Interface:** `data_transfer_init()`, `data_transfer_on_disconnected()`
- **Status:** Real

### Sensor (`sensor/motion_sensor.c`)
Real BMI270 bring-up (device get, ODR/range config, any-motion trigger)
behind `#if !TRACKER_SIM_MOTION`; a periodic software timer producing
synthetic samples behind `#if TRACKER_SIM_MOTION` (the current default -
see `app_config.h`). Either path ends by calling
`state_machine_on_motion_event()` and updating the last-sample cache.

- **Interface:** `motion_sensor_init()`, `motion_sensor_get_last_sample()`
- **Status:** Real code, untested on physical hardware (simulated path is
  what actually runs today)

### Storage (`storage/flash_log.c`)
`storage_save()` / `storage_read()` / `storage_erase()` over a fixed-size RAM
ring buffer, guarded by `app_state`'s `storage_lock` mutex.

- **Interface:** `storage_init()`, `storage_save()`, `storage_read()`, `storage_erase()`
- **Status:** Stub (interface is real; backend is RAM, not flash)

### Power (`power/power_mgmt.c`)
Inactivity timer; refuses to let a scheduled sleep proceed while
`ota_active` is set, and gets reset on any BLE activity via
`power_notify_activity()`.

- **Interface:** `power_mgmt_init()`, `power_notify_activity()`,
  `power_ota_lock()`, `power_ota_unlock()`
- **Status:** Real (deep-sleep entry itself is a hook point, board PM config
  is out of scope for this pass)

### OTA (`ota/ota_stub.c`)
Demonstrates the `power_ota_lock()`/`power_ota_unlock()` handshake with no
real image transfer behind it yet.

- **Interface:** `ota_init()`, `ota_start()`
- **Status:** Stub

### Drivers (`drivers/haptic_stub.c`)
DRV2605 haptic placeholder.

- **Interface:** `haptic_init()`, `haptic_buzz()`
- **Status:** Stub

## Data Flow (happy path)

```
Power On -> app_state/storage/power/ota/haptic init -> bt_enable()
  -> tracker_service_init() + data_transfer_init() -> advertising starts
  -> motion_sensor_init() -> state_machine_init()
  -> [event loop, driven entirely by k_work / k_timer from here]

Motion event (real IRQ or simulated timer)
  -> state_machine_on_motion_event()
  -> IDLE -> MOTION_DETECTED -> LOGGING (storage_save) -> BLE_NOTIFY
     (tracker_service_notify_status/motion, unless a transfer is active)
  -> IDLE

Phone writes "DUMP" over NUS
  -> data_transfer's k_work reads storage_read(), streams chunks over NUS TX

Disconnect -> tracker_service_on_disconnected(), data_transfer_on_disconnected(),
  advertising restarts after a short deferred delay
```

## Integration Conflicts

These are the concrete places the five source modules disagreed once put
together, and how each was resolved.

1. **Five `main()` functions, four `BT_CONN_CB_DEFINE` blocks, mixed
   `bt_enable()` styles.** Each original module was a standalone app with its
   own entry point and its own Bluetooth bring-up (one used a blocking
   `bt_enable(NULL)`, the others used an async callback). Collapsed into one
   `main()` and one `ble_adv.c` that owns `bt_enable()` and the connection
   callbacks; every other module is initialized from a single ordered chain.

2. **Two GATT services reused the same custom UUID base with overlapping
   characteristic UUIDs.** An early read-only characteristic
   (`tracker_service.c` / `ble_tracker` main.c) and the newer Status/Motion
   notify service (`ble_notify.c`) both used `...cdef0` as the service UUID.
   Kept the newer notify-based service; retired the old read-only
   characteristic rather than have two services collide on UUID space. Its
   "trigger something from the phone" role is now covered by the NUS RX
   command channel.

3. **Three different advertised names/parameter sets.** `"RishiTracker"`
   hardcoded as raw bytes in one module, `CONFIG_BT_DEVICE_NAME` in another,
   no name at all in a third. Unified to `CONFIG_BT_DEVICE_NAME` (set once in
   `prj.conf`), used everywhere.

4. **BLE + Sleep.** Advertising needs the CPU awake; the inactivity timer
   wants to sleep it. Resolved: `power_notify_activity()` resets the sleep
   countdown on every connection event, and any pending sleep is bypassed
   while `ota_active` is set.

5. **OTA + Power.** An OTA in progress must not let the device sleep mid
   transfer. Resolved via `power_ota_lock()` / `power_ota_unlock()`, called
   from `ota_start()`; the sleep timer handler checks `ota_active` before
   doing anything.

6. **Flash + BLE.** A log dump reading storage must not race a fresh motion
   event writing to it. Resolved with `app_state`'s `storage_lock` mutex,
   held inside `storage_save()` / `storage_read()` / `storage_erase()`.

7. **Notification + Data Transfer.** A motion notification firing mid-dump
   would interleave BLE traffic with the chunked transfer. Resolved with the
   `ble_transfer_active` flag: the state machine checks it before calling
   `tracker_service_notify_*` and just skips that one notification if a dump
   is in flight, rather than blocking or corrupting the transfer.

8. **Blocking call in a BLE callback.** The original NUS dump handler called
   its (blocking, `k_msleep`-paced) send loop directly from the RX callback,
   and the original disconnect handler used a blocking `k_sleep(500ms)`
   before restarting advertising. Both now defer to a work item
   (`k_work` / `k_work_delayable`) instead, so the BT host's callback context
   never blocks.

9. **Duplicate/inconsistent `LOG_MODULE_REGISTER` names and log levels.**
   Two of the original files both registered a module literally named
   `pet_tracker`, and log levels were a mix of `DBG`/`INF` with no clear
   pattern. Every module now registers under its own file-matching name
   (`state_machine`, `ble_adv`, `tracker_service`, `data_transfer`,
   `motion_sensor`, `flash_log`, `power_mgmt`, `ota_stub`, `haptic_stub`) at
   `LOG_LEVEL_INF`, with `LOG_WRN` reserved for stub/fallback paths so those
   stand out in the log.

## Not Yet Reconciled

Two pieces of prior, already-working project history (per earlier notes,
not part of the five files this RC was built from) aren't reflected in this
consolidation and would need to be merged in separately:

- The passkey-display bonding flow (verified via nRF Connect).
- The `bt_le_ext_adv_create()` extended-advertising path used to fix an
  earlier `-ENOMEM` on advertising restart.

Both are drop-in candidates for `ble/ble_adv.c` once available.
