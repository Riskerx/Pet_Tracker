# Test Report — Pet Tracker RC

Companion to `pet_tracker/tests/unit/`. This report is a filled-in-as-you-go
document, not a finished one — the automated suite was written and reasoned
through carefully, but not executed anywhere with a real Zephyr/NCS
toolchain, and none of the on-DK sections can be completed without the
actual board. Where you see `[ FILL IN ]`, that's real hardware/build output
only you can produce — everything else here is as complete as it can be
without that.

## Summary

| | |
|---|---|
| Automated suite | Written (5 suites, 12 test cases). Not yet run — needs `west build -t run` on your machine. |
| End-to-end on DK | **First real boot done.** State machine → flash log pipeline confirmed working on hardware (see below). BLE steps blocked on a bug, now fixed — reflash to continue. |
| OTA rollback (bad image) | **Blocked** — OTA is still a stub (see `architecture.md`), there's no real DFU/MCUboot backend to roll back. Not a hardware gap, a not-built-yet gap. |
| Bugs found so far | Two — a boot-order race in `main.c`, and a missing `settings_load()` call blocking BLE advertising. Both fixed. Details below. |

## What changed to support this task

- Added `src/sensor/motion_button.c/.h` — wires the DK's onboard button
  (devicetree alias `sw0`) directly to `state_machine_on_motion_event()`.
  This is the "motion (button stand-in)" the task objective asks for: a
  deterministic, on-demand trigger for bench testing that doesn't depend on
  the not-yet-validated BMI270 path.
- **Fixed a boot-order bug in `main.c`.** `state_machine_init()` was being
  called *after* `motion_sensor_init()`. `state_machine_on_motion_event()`
  submits a `k_work` item, and submitting a `k_work` before it's been
  `k_work_init()`'d is undefined behavior. The window was narrow (both calls
  happen synchronously, microseconds apart, during boot) and nothing in the
  RC push actually hit it — this was found by reasoning through the
  initialization order while writing the state-machine tests, not by an
  observed crash. `state_machine_init()` now runs first, before anything
  that can fire a motion event (`motion_sensor_init()`, `motion_button_init()`).
  Worth a mention in your closing comment since it's a real fix, not just
  test scaffolding.
- **Fixed a second, real bug found on the first actual DK boot.** The log
  showed `Advertising start failed (err -11)` (`-EAGAIN`) right after `No ID
  address. App must call settings_load()`. `prj.conf` turns on
  `CONFIG_BT_SETTINGS` for bonding, but nothing ever called `settings_load()`
  — without it, the Bluetooth stack never loads or creates an identity
  address, so advertising can't start at all. Added the standard
  `if (IS_ENABLED(CONFIG_SETTINGS)) { settings_load(); }` call in
  `ble_adv.c`'s ready callback, right after `bt_enable()` succeeds and
  before anything else BLE-related runs. This blocked every BLE bench step
  below until fixed — reflash before continuing past step 2.

## Automated suite (native_sim)

Run from `pet_tracker/tests/unit/` — see that folder's `README.md` for exact
commands. Covers state machine, storage, and power/OTA flag logic with the
BLE radio and sensor sampling faked out (both need real hardware to mean
anything; faking them is what makes this suite fast and DK-independent).

| Suite | Cases | Covers |
|---|---|---|
| `state_machine_tests` | 3 | Full motion→log→notify→idle cycle; repeated events; BLE-busy notify-skip (the "Notification + Data Transfer" conflict) |
| `flash_log_tests` | 5 | Save/read round-trip, erase, read-cap, ring-buffer wraparound, concurrent-thread safety (the "Flash + BLE" conflict, exercised with two real threads) |
| `power_ota_flag_tests` | 3 | `ota_active` set/clear via `power_ota_lock()`/`unlock()` |
| `ota_stub_tests` | 1 | Lock/unlock handshake completes cleanly |
| `haptic_stub_tests` | 1 | Smoke test |

**Result:** `[ FILL IN — paste the PASSED/FAILED summary here after running
`west build -b native_sim pet_tracker/tests/unit -t run` ]`

If anything fails to build or fails an assertion, paste the output back and
it gets fixed before this goes in the closing comment.

## End-to-end bench procedure (DK)

Everything below needs the physical nRF52840 DK. Work through it in order —
each step assumes the previous one passed.

1. **Flash.** `west flash` from `pet_tracker/`. Confirm it reports success.
   **PASS** — flashed via nRF Connect for VS Code, `merged.hex`, reset,
   `Board(s) ... flashed successfully`.
2. **Open a log view.** The DK's onboard J-Link exposes a USB CDC-ACM serial
   port — open it at 115200 8N1 in any serial terminal (PuTTY, Tera Term,
   `west espressif monitor` equivalent, whatever you normally use). You
   should see `=== Pet Tracker RC booting ===` followed by each module's
   init line, ending in `Setup complete - RC pipeline running`.
   **PASS** — full boot banner seen, every module initialized in the
   corrected order (state machine before motion sources), and the
   simulated-motion → FSM → flash_log cycle then ran correctly and
   repeatedly (20 cycles observed over ~100s, buffer count incrementing
   1/64 → 20/64 as expected, no drops or corruption). The 10s inactivity
   timeout also fired exactly on schedule (`Inactivity timeout reached`).
   This is real confirmation of the state-machine + storage pipeline
   end-to-end on hardware.
3. **Connect over BLE.** Open nRF Connect for Mobile, scan for
   `RishiTracker`, connect. Find the custom service
   (`...56789abcdef0`), and enable notifications (the CCCD toggle) on both
   the Status and Motion characteristics. Confirm the log shows `[CCCD]
   Status notifications ENABLED` / `[CCCD] Motion notifications ENABLED`.
   **Advertising confirmed fixed** — `settings_load()` now generates a
   random identity (`Identity: DE:18:2D:33:1A:84`) and
   `Advertising as "RishiTracker"` appears right after boot. Still need an
   actual phone connection + CCCD subscribe to close this step out.
   `[ FILL IN — PASS/FAIL after connecting from nRF Connect ]`
4. **Trigger motion (button stand-in).** Press Button 1 on the DK. Expect,
   in order: `Button pressed - injecting a motion event`, the four FSM
   transition lines (`IDLE --> MOTION_DETECTED --> LOGGING --> BLE_NOTIFY
   --> IDLE`), a `Saved record` line, and `[STATUS] Notified` / `[MOTION]
   Notified` lines. Confirm the phone app's characteristic values actually
   update. `[ FILL IN — PASS/FAIL ]`
5. **Data transfer.** In nRF Connect, write the ASCII bytes `DUMP` to the
   NUS RX characteristic. Expect `DUMP command received - queuing transfer`,
   then `Starting data transfer (...)` and `Data transfer complete` in the
   log, with the phone receiving the chunked NUS TX notifications.
   `[ FILL IN — PASS/FAIL ]`
6. **State machine + BLE + power interaction (edge case).** Press Button 1
   again *immediately* after issuing another `DUMP`, so the motion event
   lands while the transfer is still in flight. Expect `BLE busy with a
   data transfer - skipping this notify` in the log, and confirm the event
   still shows up in the *next* `DUMP` (proves it was logged, just not
   notified). `[ FILL IN — PASS/FAIL ]`
7. **Disconnect/reconnect.** Close the nRF Connect connection. Expect
   `Disconnected (reason ...)` followed by `Advertising as "RishiTracker"`
   within about half a second. Reconnect from the phone to confirm it's
   actually advertising again, not just logging that it is.
   `[ FILL IN — PASS/FAIL ]`
8. **OTA (partial — see gap below).** Nothing in the app currently exposes
   a way to trigger `ota_start()` from the phone (no NUS command routes to
   it yet — it's only called from firmware). If you want this bench-testable
   at all before a real DFU backend exists, that's a small follow-up (route
   an NUS command like `h`/`b`/`i` pattern from the Tracker_X firmware to
   `ota_start()`). Until then this step is **not executable**. `N/A`
9. **OTA rollback (bad image).** **Not testable.** There is no MCUboot/SMP
   DFU backend integrated (`ota_stub.c` only demonstrates the
   `power_ota_lock()`/`unlock()` handshake — see `architecture.md`). A
   rollback test needs a real second (bad) image and a real bootloader to
   reject it; neither exists in this RC. This is the task's most
   significant open gap. `BLOCKED — no DFU backend`

## Known gaps (what can't be tested without more than this RC provides)

- **OTA image transfer + rollback** — blocked on a real MCUboot/SMP DFU
  backend, not just on hardware access (see step 9 above).
- **BMI270 real-sensor path** — code is real (`sensor/motion_sensor.c`,
  `#if !TRACKER_SIM_MOTION`), unvalidated on physical silicon; the button
  trigger added for this task exercises the same downstream pipeline but
  not the sensor driver itself.
- **Bonding/pairing** — Kconfig-enabled (`prj.conf`) but the passkey-display
  flow from earlier project work isn't merged into this RC (noted in the
  README since the first consolidation pass).
- **Power timer real-time expiry** — `power_ota_flag_tests` proves the
  `ota_active` flag semantics; it doesn't wait out the real 10-second
  inactivity timeout, since that's a slow test for not much extra
  confidence. If you want this covered by the automated suite rather than
  just reasoned about, say so and it's a straightforward (if slow) test to
  add.
- **Extended advertising fix** (`bt_le_ext_adv_create()`) — same gap noted
  in the original RC README, still not merged into this codebase.

## Closing comment (template — fill in the bracketed parts, then post)

```
Verified the RC firmware end-to-end and added a consolidated Ztest suite.

Automated (native_sim), tests/unit/:
- state_machine_tests (3), flash_log_tests (5), power_ota_flag_tests (3),
  ota_stub_tests (1), haptic_stub_tests (1) - [ X/13 PASSED / paste summary ]

Bench (DK), see TEST_REPORT.md:
- Boot, BLE connect, motion (button stand-in) -> log -> notify: [ PASS/FAIL ]
- Data transfer (NUS DUMP): [ PASS/FAIL ]
- State machine + BLE + power interaction (notify skipped mid-transfer): [ PASS/FAIL ]
- Disconnect/reconnect advertising restart: [ PASS/FAIL ]

Found and fixed along the way:
- Boot-order race in main.c - state_machine_init() now runs before anything
  that can fire a motion event, was previously after motion_sensor_init().

Not testable in this pass:
- OTA rollback (bad image) - blocked, no MCUboot/DFU backend exists yet to
  roll back. Not a hardware gap; needs that built first.
- BMI270 real-sensor path - code is real, unvalidated on physical silicon.
- Bonding/pairing flow from earlier work - not merged into this RC.

Pushed to [ branch/PR link ].
```
