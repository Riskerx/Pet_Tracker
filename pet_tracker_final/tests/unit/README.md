# pet_tracker unit tests

Consolidated Ztest suite covering state machine, storage, power/OTA flag
semantics, and the OTA/haptic stubs. Runs on `native_sim` - no DK, no
Nordic-specific config, and no BLE stack required, because the two things
that genuinely need real hardware (the BLE radio and the motion sensor) are
replaced with test doubles in `src/fakes.c`. Everything else compiled in is
the real file from `../../src`.

## Run it

From your NCS workspace (same one you use for `pet_tracker/`):

```powershell
west build -b native_sim pet_tracker/tests/unit -t run
```

If that target name doesn't match your setup, the two-step version works
everywhere:

```powershell
west build -b native_sim pet_tracker/tests/unit
west build -t run
```

Or, if you'd rather use twister:

```powershell
west twister -p native_sim -T pet_tracker/tests -v
```

Either way, look for `PROJECT EXECUTION SUCCESSFUL` (twister) or a `PASSED -
<test name>` line per test plus a final `ZTEST SUITE PASSED` block (direct
`west build -t run`) in the output.

## What's covered here vs. not

| Suite | What it tests | Real code or fake? |
|---|---|---|
| `state_machine_tests` | Full motion→log→notify→idle cycle, repeated events, BLE-busy notify skip | state_machine.c + flash_log.c real; BLE notify + sensor sample faked |
| `flash_log_tests` | Save/read/erase, ring buffer wraparound, concurrent-thread safety | flash_log.c real |
| `power_ota_flag_tests` | `ota_active` flag set/clear | power_mgmt.c real |
| `ota_stub_tests` | Lock/unlock handshake completes | ota_stub.c + power_mgmt.c real |
| `haptic_stub_tests` | Smoke test only | haptic_stub.c real |

**Not covered here** - see `../../TEST_REPORT.md` for the full list and why:
BLE radio behavior (advertising, pairing, actual notify/NUS transfer over
the air), the real BMI270 sensor path, OTA image transfer/rollback (no DFU
backend exists yet to test), and the power timer's real 10-second
expiry/OTA-blocks-sleep timing. Those need the DK, and the bench procedure
in the test report walks through exercising them by hand.

## If the build fails

Paste the exact error and I'll fix it - this suite was written against the
production code in `../../src` but hasn't been build-verified in an actual
Zephyr/NCS environment (no toolchain available where it was written), the
same caveat as the RC firmware itself.
