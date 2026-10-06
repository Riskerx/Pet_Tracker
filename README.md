# Tracker_X — Firmware Developer Guide

Authoritative onboarding + reference for firmware development on the **Tracker_X**
board. This guide is grounded in the actual current firmware
(`C:\ncs\tracker_x_bringup`), the Altium design netlist, and the hardware
bring-up performed on the bench (July 2026). Where it disagrees with the two
older companion docs, **this guide wins** — the deltas are called out inline.

> **Tracker_X = WAND Main Board Rev 1.0 + Bosch BMM350 magnetometer (IC5).**
> The BMM350 is the only intended electrical change; it forms a 9-axis IMU with
> the BMI270 for tilt-compensated heading (the core tracker feature). On the
> current bench unit the **BMM350 is not yet soldered** — it is fully documented
> here as "designed-in, fit later" (§9.5).

### Companion documents
| Doc | Role |
|-----|------|
| **This file** | Firmware developer guide — toolchain, build, pin map, per-peripheral bring-up, errata, tooling. Start here. |
| `TrackerX_PCB_Documentation.md` | Original reference guide (pin maps, register recipes). Still useful; some power/level guidance is **superseded** — see §4. |
| `TrackerX_BringUp_Procedure.md` | Bench bring-up procedure with pass/fail gates. |
| `Main_Board.pdf` / `TrackerX.pdf` | Schematic / PCB layout. |

---

## 1. Board overview

| Block | Part (refdes) | Interface | Address / Pins | Status |
|-------|---------------|-----------|----------------|--------|
| MCU | nRF52840-QIAA (aQFN73) | — | — | ✅ |
| PMIC | nPM1300-QEAA (IC1) | I²C | `0x6B` | ✅ |
| IMU | BMI270 (IC2, LGA-14) | I²C | `0x69` (or `0x68` via SB2), CHIP_ID `0x24` | ✅ |
| **Magnetometer** | **BMM350 (IC5, WLP)** | **I²C** | **`0x14`** (`0x15` if ADDR strapped high) | **fit later** |
| Haptic driver | DRV2605L (IC4) | I²C | `0x5A` | ✅ |
| Buzzer | magnetic/piezo (BZ1) | PWM | `P0.03` | ✅ |
| Storage | W25Q128JV 16 MB QSPI NOR (IC3) | QSPI | see §5 | ✅ single-line¹ |
| PPG AFE | MAX86140 + OSRAM SFH7050 (on FPC via **J1**) | SPI (SPIM2) | off-board | future |
| Status LED | green LED (LED2) | PWM | `P0.25` | ✅ |
| RGB indicator | RED/GRN/BLU (LED4/5/6) | via nPM1300 | nPM pins 27/26/25 (I²C) | ✅ |

¹ Quad-I/O silently fails on the bench unit (open IO2/IO3 ball) — see §9.8 / §11.3.

The nRF52840 runs a BLE peripheral exposing the **Nordic UART Service (NUS)** for
data + command streaming and the **Battery Service (BAS)**. All bring-up
telemetry is also emitted to **SEGGER RTT** for bench work (no phone needed).

Build target: **`nrf52840dk/nrf52840`** + the board overlay
`boards/nrf52840dk_nrf52840.overlay`, which repins the DK to the Tracker_X map.

---

## 2. Toolchain & environment

- **nRF Connect SDK v3.0.2** (Zephyr v4.0.x) at `C:\ncs\v3.0.2`.
- Toolchain bundle `C:\ncs\toolchains\0b393f9e1b` (its own west, CMake, Ninja,
  Zephyr SDK 0.17.0, Python 3.12).
- Debug probe: **SEGGER J-Link** (bench probe serial **69408534**), flashed with
  **nrfjprog** (`nrfutil` not required).

### 2.1 Build-environment quirks (this machine) — all handled by `build.ps1`

Three workarounds are required to build NCS here; they are already encoded in
`build.ps1`, but know them:

1. **git "dubious ownership".** The SDK repos under `C:\ncs\v3.0.2` are owned by
   a different Windows user (SID …-1004) than Administrator (…-500), so west/git
   refuse them and Zephyr module discovery fails (`cannot import contents of
   west.yml`). Fix: point **`GIT_CONFIG_GLOBAL`** at a file containing
   `[safe]\n\tdirectory = *` (git only honors `safe.directory` from a *global
   config file*, not from `-c` or env params). `build.ps1` sets this to
   `gitconfig-safe` in the project dir.
2. **cmake must run from the workspace cwd.** west finds the workspace by current
   directory, not `ZEPHYR_BASE`. `build.ps1` does `Push-Location C:\ncs\v3.0.2`
   around the `cmake` configure step. Plain `cmake -GNinja … && ninja -C …`
   works; no `west build` needed.
3. **Toolchain env** from `…\0b393f9e1b\environment.json`: PATH, PYTHONPATH,
   `ZEPHYR_TOOLCHAIN_VARIANT=zephyr`, `ZEPHYR_SDK_INSTALL_DIR`,
   `ZEPHYR_BASE=C:\ncs\v3.0.2\zephyr`.

> `nrfjprog`'s `JLinkARM.dll reported error -256` spam is **cosmetic** — ignore
> it as long as the operation ends "…flashed successfully" / "Run".

---

## 3. Building

```powershell
.\build.ps1                # Stage 2 (full) -> build_stage2\zephyr\zephyr.hex
.\build.ps1 -Stage 1       # BLE-only       -> build_stage1\...
.\build.ps1 -Flash         # build then flash (adds --recover unless -NoRecover)
.\build.ps1 -Stage 2 -Pristine   # wipe build dir first
```

### 3.1 Build stages (`WAND_STAGE`)

Compile-time selector. `build.ps1` exports it as both `-DWAND_STAGE=<n>` and the
`WAND_STAGE` env var; `CMakeLists.txt` resolves it (CMake var → env → default 2)
and forwards it to `src/main.c` via `target_compile_definitions`.

| Stage | Contents |
|-------|----------|
| `1` | BLE + NUS + BAS only, status-LED breathing, `alive` heartbeat (radio bring-up). Everything under `#if (WAND_STAGE >= 2)` is compiled out. |
| `2` | Stage 1 + I²C bus scan + BMI270 + nPM1300 (MFD/charger/LEDs/regulator) + W25Q128 QSPI + DRV2605 haptic + buzzer + flash R/W self-test. |

> Set the stage via the env var / `-Stage`, **not** a bare `-- -DWAND_STAGE=2` —
> PowerShell's `--` stop-parsing token swallows it.

### 3.2 Kconfig layering

- `prj.conf` — Stage-1 base (BLE/NUS/BAS, RTT logging, PWM, LFCLK source).
- `stage2.conf` — layered via `-DEXTRA_CONF_FILE=stage2.conf` **only when Stage ≥ 2**
  (I²C, sensors, BMI270, nPM MFD/LED/**regulator**, QSPI flash, per-driver debug logs).
- Diagnostic overlays combine, e.g. `-DEXTRA_CONF_FILE="stage2.conf;skip_bmi.conf"`
  (isolation build) or `"stage2.conf;rcclock.conf"` (RC-clock BLE test).

Full Kconfig tables are in §8.4.

---

## 4. Power architecture ⚠️ (read this before touching rails)

**This section corrects the mental model in the older docs.** The nRF52840 runs
in **high-voltage (VDDH) mode**, and the I²C bus spans a 3.0 V logic domain — not
the 1.8 V the DK defaults and the original bring-up doc assume.

| Rail / node | Source | Voltage | Feeds |
|-------------|--------|---------|-------|
| **VDDH** (nRF HV input) | nPM1300 **BUCK2** (= VDD_HV) | **3.0 V** | nRF52840 VDDH |
| **VDD_nRF** (nRF I/O rail) | nRF **internal REG0** | set by **UICR REGOUT0** (2.4 V) | all nRF GPIO / I²C / QSPI pin logic levels |
| **VOUT1** (sensor rail) | nPM1300 **BUCK1** | **2.5 V** | BMI270, BMM350 (when fitted), I²C pull-ups |
| **VOUT2 / VDD_HV** | nPM1300 **BUCK2** | **3.0 V** | flash VCC (IC3 pin 8), DRV2605 VDD, **nPM VDDIO**, RGB LED anodes, J2 pin 1 |
| **VLED** | nPM1300 **LDO1** | 3.3 V (0 V until FW enables) | J1 → MAX86140 sensor board |

Key consequences for firmware and bring-up:

- **Killing the nPM kills the nRF.** BUCK2 → VDDH, so pulling IC1 removes nRF
  power entirely (SWD goes dead). Confirmed on the bench.
- **The I²C bus logic level is the nPM's VDDIO = 3.0 V domain.** The nPM1300 and
  W25Q128 have a **worst-case V_IH ≈ 2.1 V (0.7 × 3.0)**. A 1.8 V bus is *below
  spec* for them — this was the root cause of chronic nPM I²C flakiness and
  intermittent QSPI errors on this board.
- **Fix (applied):** VOUT1 raised to **2.5 V** and **REGOUT0 = 2.4 V**, so the
  bus idles ~2.4–2.5 V and clears every device threshold with margin. VOUT1 is
  currently set **in firmware** (BUCK1 `regulator-init-microvolt`, §9.3); the
  **permanent hardware fix is the VSET1 resistor** to strap VOUT1 = 2.5 V even
  before firmware runs. *Do this before the board runs unattended* — any image
  that omits the regulator node drops VOUT1 back to the strapped 1.8 V silently.

> **Superseded guidance:** the original reference doc says "keep the I²C bus
> ≤ 1.8 V" and "pull-ups 4.7–5.1 kΩ." Both are wrong for this board. The sensor
> parts (BMI270/BMM350) are rated to 3.6 V VDDIO, so 2.5 V is safe; the correct
> pull-ups are **~10 kΩ to VOUT1** (the design's R3/R4 150 Ω placeholders have a
> floating second pad and are *not* pull-ups — see §11.1).

### 4.1 UICR REGOUT0 — operational hazard

REGOUT0 lives in **UICR** and is **wiped to 1.8 V by `--chiperase` and
`--recover`.**

| Field | Value |
|-------|-------|
| REGOUT0 register | `0x10001304` |
| `0xFFFFFFFA` | **2.4 V** (current setting) |
| `0xFFFFFFF9` | 2.1 V |
| `0xFFFFFFFF` | 1.8 V (erased default) |

**Rule: flash the application with `--sectorerase` (never `--chiperase`), and
rewrite REGOUT0 after any `--recover`.** See §6.

---

## 5. Pin map (nRF52840 → board)

Authoritative, cross-checked against the WAND netlist and the current overlay.

### 5.1 I²C0 (`&i2c0`, TWIM) — the sensor bus
| Signal | Pin | Ball | On the bus |
|--------|-----|------|-----------|
| SDA | **P1.08** | P2 | nPM1300 p13, BMI270 p14, DRV2605 p3, BMM350 (p—), J2 p3, SDA TP |
| SCL | **P1.09** | R1 | nPM1300 p14, BMI270 p13, DRV2605 p2, BMM350, J2 p4, SCL TP |

100 kHz (`I2C_BITRATE_STANDARD`); `zephyr,concat-buf-size = <512>` (needed for
the BMI270 256-byte config-blob upload); internal `bias-pull-up` set in pinctrl
+ external ~10 kΩ to VOUT1. The bus is also exposed on the **J2 FPC** (I²C
expansion). *The "SCL on P0.04" jumper from the WAND bring-up board was a
unit-specific damage workaround and does NOT apply here.*

### 5.2 QSPI (`&qspi`) — W25Q128 (IC3)
| Signal | Pin | Ball | IC3 pin |
|--------|-----|------|---------|
| CS  | P0.17 | AD12 | 1 |
| CLK | P0.19 | AC15 | 6 |
| IO0 (DI)   | P0.20 | AD16 | 5 |
| IO1 (DO)   | P0.21 | AC17 | 2 |
| IO2 (/WP)  | P0.22 | AD18 | 3 |
| IO3 (/HOLD)| P0.23 | AC19 | 7 |

IC3 VCC (pin 8) = **VDD_HV (3.0 V)**. Matches Nordic's reference QSPI assignment.
(P0.18 = nRESET, not QSPI.)

### 5.3 PWM
| Function | Pin | Ball | Peripheral |
|----------|-----|------|-----------|
| Status LED (breathing) | P0.25 | AC21 | PWM1 / OUT0 — active-high (anode on pin, cathode→R2→GND) |
| Buzzer | P0.03 | B13 | PWM2 / OUT0 — single-ended to BZ1 |

### 5.4 Discrete control / interrupt lines (from netlist — not all used by FW yet)
| Pin | Ball | Connects to | Note |
|-----|------|-------------|------|
| P0.26 | G1 | BMI270 **INT1** | available for data-ready IRQ |
| P0.27 | H2 | BMI270 **INT2** | available |
| P0.28 | B11 | DRV2605 **EN** | if held low the DRV is in shutdown — confirm it's driven/pulled high |
| P0.30 | B9  | DRV2605 **IN/TRIG** | external trigger option (FW uses I²C GO instead) |
| P0.29 | A10 | button SW2 (→GND) | needs internal pull-up |
| P0.31 | A8  | button SW1 (→GND) | needs internal pull-up |

### 5.5 Sensor-board SPI (J1 → MAX86140, off-board, future)
| Pin | Ball | J1 pin |
|-----|------|--------|
| P0.15 | AD10 | 2 |
| P0.13 | AD8  | 3 |
| P0.12 | U1   | 4 |
| P0.11 | T2   | 5 |

J1: `1=VLED  2=P0.15  3=P0.13  4=P0.12  5=P0.11  6=GND`. **The exact
SCLK/CS/MOSI/MISO role of each line is not in the netlist** — get it from the
sensor-board schematic before writing the SPI driver (§9.9).

### 5.6 Clock / debug
- **LFCLK 32.768 kHz crystal (XL1)** on P0.00/P0.01 (balls D2/F2) → LFCLK source
  = XTAL. **HFCLK 32 MHz (Y1)** on A23/B24.
- **SWDIO** = ball AC24, **SWDCLK** = ball AA24. **nRESET** = P0.18 (AC13).
- UART broken out to test points only: **TX = P0.08, RX = P0.06** (console
  disabled in firmware; logging is over RTT).

---

## 6. Flashing & debugging

### 6.1 Flash (preserving UICR / REGOUT0)

```powershell
# Preferred: sector erase keeps UICR REGOUT0 (2.4 V) intact
nrfjprog --program build_stage2\zephyr\zephyr.hex --sectorerase --verify -f NRF52
nrfjprog --reset -f NRF52

# First flash of a factory-locked part (wipes UICR — REGOUT0 reverts to 1.8 V!)
nrfjprog --recover -f NRF52
# ...then REWRITE REGOUT0 before relying on the 3.0 V-domain devices:
nrfjprog --memwr 0x10001304 --val 0xFFFFFFFA -f NRF52   # 2.4 V
```

> `build.ps1 -Flash` currently uses `--chiperase` — fine for a first program of
> a fresh part, but **it wipes REGOUT0**. For iterative flashing on a
> configured board, use the explicit `--sectorerase` command above.

### 6.2 Logs over SEGGER RTT

```powershell
& "C:\Program Files\SEGGER\JLink\JLinkRTTLogger.exe" `
    -Device NRF52840_XXAA -If SWD -Speed 4000 -RTTChannel 0 out.log
```

Boot banner, I²C scan, sensor IDs, and per-cycle telemetry appear here with no
BLE client connected. RTT is configured for a 4096-byte up-buffer in block mode
(`CONFIG_SEGGER_RTT_BUFFER_SIZE_UP=4096`, `CONFIG_LOG_BACKEND_RTT_MODE_BLOCK=y`)
so logs don't drop under load.

### 6.3 SWD recovery
If the core won't `connect`, drop to 100 kHz (`slowconnect.jlink`) or read the DP
IDCODE raw (`rawswd.jlink`); see the diagnostic tooling in §10.

---

## 7. Firmware architecture (`src/main.c`)

Single-file bring-up firmware (~645 lines). Log module `trackerx`, level INF.
Central helper **`emit(fmt, …)`** formats into a `static char[160]` and mirrors
every line to **both RTT (`LOG_INF`) and NUS** (`nus_tx`, 20-byte chunks, no-op
when no client). *`emit` is not reentrant — main-thread/boot use only.*

### 7.1 Boot sequence (`main`, WAND_STAGE ≥ 2)
1. Banner (`WAND_STAGE`, build date/time).
2. `bt_enable` → `bt_nus_init` → start connectable advertising as **`WAND_TEST`**
   (`BT_LE_ADV_CONN_FAST_2`; NUS UUID in scan response).
3. `k_msleep(500)` settle delay.
4. `i2c_report()` — bus scan (§9).
5. `init_diag()` — per-device `init_res` + a raw nPM register read.
6. `bmi270_report_id()` → `npm_led_test()` → `bmi270_configure()`
   → `npm_telemetry()` → `w25q128_check()` → `w25q128_rw_test()`
   → `drv2605_haptic(3)` → `buzzer_beep(3)`.
7. `emit("Boot sequence complete")`.

A separate `K_THREAD_DEFINE(led_tid, …)` runs the P0.25 breathing LED
independently ("firmware alive" indicator).

### 7.2 Main loop (2 s tick)
- Drains NUS command flags (see §7.3).
- `bmi270_sample()` every tick.
- Every 5th tick (~10 s): `npm_telemetry()` + `w25q128_check()`.
- Every tick: `emit("alive %u", n)` heartbeat.

### 7.3 NUS command interface (phone → device)
The RX callback only sets atomic flags; work runs in the main loop (never in BLE
context). Send a single character on the NUS RX characteristic:

| Char | Action |
|------|--------|
| `h` | DRV2605 haptic (3 GO pulses) |
| `b` | Buzzer (3 beeps @ 2.7 kHz) |
| `i` | Re-run I²C scan |
| `j` | Re-run W25Q128 JEDEC check |
| `t` | Re-run nPM telemetry |

Unrecognized bytes are ignored.

### 7.4 CONFIG_BMI270 isolation guard
The BMI270 device pointer, scan-table entry, and all `bmi270_*` functions are
wrapped in `#ifdef CONFIG_BMI270`; when undefined, stub versions print
`"BMI270: SKIPPED (isolation build)"` and **address `0x69` is never driven**.
Build the isolation image with `skip_bmi.conf` (`CONFIG_BMI270=n`) to debug bus
wedges without the IMU on the wire.

---

## 8. Configuration reference

### 8.1 `prj.conf` (Stage-1 base)
| Kconfig | Value | Purpose |
|---------|-------|---------|
| `CONFIG_BT` / `BT_PERIPHERAL` | y | BLE stack, peripheral role |
| `CONFIG_BT_DEVICE_NAME` | `"WAND_TEST"` | advertised name (rename for production) |
| `CONFIG_BT_MAX_CONN` | 1 | single connection |
| `CONFIG_BT_NUS` / `BT_BAS` | y | Nordic UART Service, Battery Service |
| `CONFIG_LOG` / `USE_SEGGER_RTT` / `LOG_BACKEND_RTT` | y | RTT logging |
| `CONFIG_UART_CONSOLE` / `SERIAL` | n | console off (RTT only) |
| `CONFIG_SEGGER_RTT_BUFFER_SIZE_UP` | 4096 | bigger up-buffer |
| `CONFIG_LOG_BACKEND_RTT_MODE_BLOCK` | y | queue, don't drop |
| `CONFIG_CLOCK_CONTROL_NRF_K32SRC_XTAL` | y | **LFCLK = 32.768 kHz crystal** |
| `CONFIG_PWM` / `GPIO` | y | status LED / buzzer |
| `CONFIG_HEAP_MEM_POOL_SIZE` | 8192 | kernel heap |

### 8.2 `stage2.conf` (Stage-2 additions)
| Kconfig | Value | Purpose |
|---------|-------|---------|
| `CONFIG_I2C` / `SENSOR` | y | bus + sensor subsystem |
| `CONFIG_BMI270` | y | IMU driver (DT-enabled) |
| `CONFIG_MFD` / `LED` | y | nPM1300 MFD core + RGB LED (host mode) |
| `CONFIG_REGULATOR` | y | **activates BUCK1 → VOUT1 = 2.5 V at boot** (§4) |
| `CONFIG_LED_GPIO` / `LED_PWM` | n | keep DK LED nodes inert |
| `CONFIG_FLASH` / `NORDIC_QSPI_NOR` / `FLASH_JESD216_API` | y | W25Q128 (last is required for `flash_read_jedec_id`) |
| `CONFIG_*_LOG_LEVEL_DBG` (MFD/LED/SENSOR/FLASH/I2C) | y | surface driver init failures over RTT |

### 8.3 Overlay (`boards/nrf52840dk_nrf52840.overlay`) highlights
- `&i2c0` TWIM okay, 100 kHz, concat-buf 512; children `bmi270@69`, `pmic@6b`.
- `pmic@6b` → `regulators/BUCK1` = 2.5 V (min/max/init), `leds` (3× host mode),
  `charger` (4.2 V term, 100 mA, 10 k NTC β3380, **charging disabled by default**).
- `&qspi` → `w25q128@0`: `nordic,qspi-nor`, `jedec-id [ef 40 18]`,
  `size <134217728>` (bits), `quad-enable-requirements "S2B1v1"`, **`writeoc "pp"`
  / `readoc "fastread"` (single-line — quad disabled pending the IO2/IO3 fix)**,
  `sck-frequency 8 MHz`.
- PWM: `status_led` on PWM1/P0.25 (20 ms period), `buzzer` on PWM2/P0.03.
- Disabled DK nodes: `uart0/1`, `pwm0` (its P0.13 = MAX86140 SDI), `spi1/3`,
  `ieee802154`.

---

## 9. Peripheral bring-up & control

### 9.1 BLE + NUS + BAS
Advertises as `WAND_TEST`. NUS **TX** = device→phone notifications (banner,
`alive`, telemetry, all `emit` output); NUS **RX** = single-char commands (§7.3).
Battery % published to standard **BAS (0x180F)**, mapped from VBAT
(3.0 V = 0 %, 4.2 V = 100 %). Connect with **nRF Connect for Mobile**.

### 9.2 Status LED
Breathing green LED on **P0.25 / PWM1** via a dedicated thread — if it breathes,
the firmware is alive.

### 9.3 nPM1300 PMIC (`0x6B`)
Zephyr MFD + regulator + LED + charger-sensor stack.
- **Rails:** BUCK1 → VOUT1 (2.5 V sensor rail, set in overlay), BUCK2 → VDD_HV
  (3.0 V), LDO1 → VLED (3.3 V, off until enabled). See §4.
- **RGB LEDs:** 3× nPM LED drivers in host mode, on/off over I²C. `npm_led_test()`
  walks R/G/B at boot (300 ms each).
- **Telemetry (`npm_telemetry`):** reads `GAUGE_VOLTAGE`, `GAUGE_TEMP`,
  `NPM1300_CHARGER_STATUS`, `NPM1300_CHARGER_VBUS_STATUS`; emits
  `nPM VBAT=<mV> die=<°C> chg_status=0x.. vbus=..`; updates BAS.
- **`init_diag`** also does a raw `i2c_write_read(0x6B, {0x02,0x07})`
  (VBUSIN.VBUSINSTATUS) — a register-level liveness probe independent of the driver.
- **USB charging is OFF by default.** Add `charging-enable;` to the charger node
  to charge from USB (VBUS→VSYS path is automatic in HW).

### 9.4 BMI270 IMU (`0x69`)
- CHIP_ID `0x24` (read from reg `0x00`). Address = 0x69 with SDO high (SB2);
  confirm the solder bridge if you see 0x68.
- Configured ±2 g / ±500 dps / 100 Hz via the Zephyr sensor API; accel magnitude
  ≈ 9.81 m/s² (`|a|` printed in milli-m/s²) at rest, changing with motion.
- INT1/INT2 wired to P0.26/P0.27 — available for data-ready interrupts (FW
  currently polls).

### 9.5 BMM350 magnetometer (IC5) — **designed-in, fit later**
Not on the current bench unit; a NACK at `0x14` in the scan is expected and
printed as `(expected)`. To bring it up once soldered:

1. **Kconfig:** add `CONFIG_BMM350=y` to `stage2.conf` (I2C/SENSOR already on).
2. **Overlay:** add under `&i2c0`:
   ```dts
   bmm350: bmm350@14 {
       compatible = "bosch,bmm350-i2c";
       reg = <0x14>;                 /* 0x15 if ADDR strapped high — confirm */
       /* optional: drdy-gpios = <&gpio0 PIN GPIO_ACTIVE_HIGH>; */
   };
   ```
   NCS v3.0.2 ships the driver at `drivers/sensor/bosch/bmm350`.
3. **Firmware:** add `BMM350_ADDR`/entry to the scan table as *expected*, and a
   sampler:
   ```c
   const struct device *mag = DEVICE_DT_GET(DT_NODELABEL(bmm350));
   struct sensor_value m[3];
   sensor_sample_fetch(mag);
   sensor_channel_get(mag, SENSOR_CHAN_MAGN_XYZ, m);   /* Gauss */
   ```
4. **Sanity:** |X,Y,Z| ≈ Earth's field **0.25–0.65 Gauss**, roughly constant as
   the board rotates, vector swings toward magnetic north / when a magnet nears.
5. **Confirm the ADDR strap** (0x14 vs 0x15) on the assembled board.

> **Level note (corrected):** the BMM350 sits on VOUT1 = 2.5 V with the BMI270.
> The old doc's "keep bus ≤ 1.8 V for the BMM350" is **superseded** — 2.5 V is
> within the part's rating and is required for reliable nPM/flash I²C (§4).

Fused with the BMI270 accel → tilt-compensated **heading / e-compass**, the core
tracker function.

### 9.6 DRV2605L haptic (`0x5A`)
Reads STATUS (reg `0x00`; DEVICE_ID = `STATUS >> 5`), then plays ROM effect #1:
```c
0x01 = 0x00   /* MODE: internal trigger (exit standby) */
0x03 = 0x01   /* LIBRARY: ERM A  (use 0x06 for an LRA motor) */
0x04 = 0x01   /* WAVESEQ1: effect #1 */
0x05 = 0x00   /* WAVESEQ2: terminator */
0x0C = 0x01   /* GO  (pulsed 3× at boot / on 'h') */
```
Motor M1 on OUT±. **EN = P0.28**, TRIG = P0.30 (FW triggers over I²C, not the
pins). The `OC_DETECT` bit in STATUS can latch at boot (STATUS toggles
`0xE0`/`0xE1`) — **benign if the motor physically buzzes**; if not, check the
motor wiring. Match `LIBRARY` (0x03) to the actual motor type (ERM vs LRA).

### 9.7 Buzzer (P0.03)
Magnetic/piezo buzzer via PWM2. `buzzer_beep(n)` drives `PWM_HZ(2700)` at 50 %
duty, 150 ms on / 150 ms off. Tune to the buzzer's resonant frequency.

### 9.8 W25Q128 QSPI flash (IC3)
- JEDEC ID **`EF 40 18`** via `flash_read_jedec_id()` (`w25q128_check`).
- **`w25q128_rw_test()`** exercises the storage path at boot: reads, erases the
  4 KB sector at **`0xFFF000`** (top of part, clear of any future NVS/LittleFS
  region at the bottom), verifies `0xFF`, writes 32 bytes (`0xA5 ^ i*7`), reads
  back, and verifies + retention across reboot. **PASS** =
  `FLASH RW: PASS (erase+write+readback 32 bytes @0xfff000)`.
- **Single-line only right now.** Quad (`pp4io`/`read4io`) silently fails on the
  bench unit — `flash_write` returns 0 but nothing lands and quad reads float
  `0xFF`. The QE bit is confirmed set (RDSR2 = 0x02) and IO2/IO3 are routed
  (P0.22→IC3 p3, P0.23→IC3 p7), so it is a **physical open on IO2/IO3** on this
  unit (§11.3). Single-line `pp`/`fastread` @ 8 MHz (~1 MB/s) is plenty for NVS.
- Storage: layer **NVS** or **LittleFS** on a `fixed-partitions` region at the
  bottom of the part.

### 9.9 MAX86140 PPG (future)
On the FPC daughterboard behind **J1** (not on the main board). SPI on SPIM2,
PART_ID (reg `0xFF`) = `0x24`. VLED (3.3 V, nPM LDO1) feeds the board; an on-board
1.8 V LDO derives the MAX supply. SFH7050 emitter map: LED1(0x23)=green,
LED2(0x24)=red, LED3(0x25)=IR; HR uses red+IR. SPI frame `[reg][cmd][data]`,
`cmd 0x00`=write / `0x80`=read; FIFO drain via `FIFO_DATA_COUNT(0x07)` +
`FIFO_DATA(0x08)`, 3-byte samples (`tag=b0>>3`, 19-bit count). **Get the exact
SPI pin roles from the sensor-board schematic** before writing the driver (§5.5).

---

## 10. Golden boot log (Stage 2, healthy board)

```
*** Booting nRF Connect SDK v3.0.2 ***
trackerx: Tracker_X bring-up firmware, WAND_STAGE=2
trackerx: Advertising as "WAND_TEST"
trackerx: I2C scan (P1.08 SDA / P1.09 SCL):
trackerx:   0x6B nPM1300                  ACK (ret 0)
trackerx:   0x69 BMI270                   ACK (ret 0)
trackerx:   0x14 BMM350 (IC5 not fitted)  NACK (ret -5) (expected)
trackerx:   0x5A DRV2605                  ACK (ret 0)
trackerx: init: i2c0/npm1300-mfd/npm-leds/npm-charger/w25q128  init_res=0
trackerx: BMI270 CHIP_ID=0x24 (OK)
trackerx: nPM VBAT=3891mV die=~30C chg_status=0x00 vbus=12
trackerx: W25Q128 JEDEC=ef 40 18 (OK)
trackerx: FLASH RW: PASS (erase+write+readback 32 bytes @0xfff000)
trackerx: DRV2605 STATUS=0xE0 (DEVICE_ID=7)
trackerx: Buzzer: 3 beeps @2.7kHz on P0.03
trackerx: Boot sequence complete
trackerx: IMU acc mm/s2 X=.. Y=.. Z=.. |a|=~9800 (~9810 at rest)
trackerx: alive 1
```
When a device that should ACK NACKs, its line ends `  <-- FAIL`. A bus with zero
ACKs prints `bus dead (no ACKs) - skipping full sweep`.

---

## 11. Hardware errata & known issues

### 11.1 No real I²C pull-ups on the main board
Schematic R3 (SCL) / R4 (SDA) are 150 Ω with a **floating second pad** — series
placeholders, not pull-ups; the nPM has no internal I²C pulls. **Fit ~10 kΩ to
VOUT1** (done on the bench board). Flag the "150R" pull-up footprints for
correction at the next design review.

### 11.2 I²C level margin (§4)
1.8 V bus is below the nPM/flash V_IH (2.1 V). Fix = VOUT1 2.5 V + REGOUT0 2.4 V.
Permanent fix is the **VSET1 resistor**; firmware currently sets VOUT1 in
software, which any non-Stage-2 image would miss.

### 11.3 QSPI quad open (per-unit)
Quad fails; single-line works. Suspected open ball on **IO2 (P0.22)** or **IO3
(P0.23)** at the nRF. GPIO drive tests *cannot* see an open ball (the pin reads
back its own driver); a diode test at IC3 pins is *also* masked by the flash's
ESD diodes. **Discriminator:** slow-toggle P0.22/P0.23 as GPIO while metering
IC3 pin 3/7 — follows = OK, static = the break. (Quad was never exercised on any
Tracker_X board, since JEDEC-ID is single-line — worth checking other units too.)

### 11.4 BLE advertises but won't connect → suspect LFCLK crystal
Advertising needs no time sync; a connection requires waking precisely in each
window, which a bad **32.768 kHz crystal (XL1 / load caps / joints)** breaks.
Diagnostic: build with `rcclock.conf` (LFCLK = internal RC + calibration); if it
then connects, the crystal is the fault. XL1 sits next to the frequently-reworked
nRF, so a disturbed joint is plausible. *(Open as of this writing — retest.)*

### 11.5 Other
- **SWD outage during rework:** core went mute even to raw DP IDCODE; recovered
  after reseating leads / clearing a bridge near SWDCLK. If SWD dies right after
  an nRF reflow, re-check SWDIO/SWDCLK before suspecting the die.
- **VTref on the bench jig reads ~3.3 V** (it senses VSYS/VBAT, not VDD_nRF) —
  marginal vs a 2.4 V I/O high but works.
- **VDD_nRF source not captured in the flat netlist** — verify what actually
  feeds the nRF VDD balls (expected: the nPM domain) if you revisit the design.
- **Open-ball / open-trace faults are invisible to GPIO drive tests** — use a
  diode test at the *chip* pin, or a driven-line-vs-DMM check, to catch them.

---

## 12. Diagnostic tooling — J-Link scripts

In `C:\ncs\tracker_x_bringup\`, run `JLink.exe -CommandFile <name>.jlink`. These
drive raw peripheral/GPIO registers so a bus can be exercised **independently of
the Zephyr drivers** — invaluable for isolating solder faults.

**Register key (nRF52840):** `0x50000510` = P0.IN; `…508`/`…50C` = P0.OUTSET/CLR;
PIN_CNF[P0.n] = `0x50000700+4n`. `0x50000810` = P1.IN (bit8=SCL P1.08,
bit9=SDA P1.09); PIN_CNF[P1.08]=`0x50000A20`, [P1.09]=`0x50000A24`. Peripheral
ENABLE: `0x40003500` = TWI0, `0x40029500` = QSPI (write 0 to release pins).
PIN_CNF values: `0x0`=input(buf connected), `0x601`=open-drain S0D1,
`0x701`=high-drive open-drain (~10 mA), `0x4`=input+pulldown, `0xC`=input+pullup.

| Script | Purpose |
|--------|---------|
| `vtref.jlink` | Report VTref + confirm target power / SWD present. |
| `rawswd.jlink` | Read DP IDCODE at 100 kHz when the core is mute. |
| `slowconnect.jlink` | `connect` at 100 kHz to recover a marginal SWD link. |
| `drivetest2.jlink` | I²C pin-drive test — toggle SDA/SCL open-drain, read P1.IN. |
| `drivetest3.jlink` | Same at high-drive `0x701` (~10 mA) — the "unpullable even at 10 mA" test. |
| `qspi_drivetest.jlink` | Walk each QSPI pin as output, read P0.IN (drive-side quad check). |
| `spi_bitbang_rdid.jlink` | Bit-bang JEDEC RDID (0x9F) — end-to-end proof of CS/CLK/IO0/IO1 (`EF 40 18`). |
| `spi_bitbang_rdsr2.jlink` | Bit-bang RDSR2 (0x35) — confirm QE bit (returned SR2=0x02). |
| `i2c_unwedge.jlink` | 9 SCL pulses + STOP to release a slave holding SDA low. |
| `i2c_bb_probe.jlink` | Bit-bang I²C START/addr/clock, sample ACK without TWIM. |
| `buspeek.jlink` / `buspeek2.jlink` | Read idle I²C levels (buspeek2 first connects the input buffers — required after a Stage-1 image). |
| `sda_mystery.jlink` | Fight SDA with the internal ~13 kΩ pulldown to tell a real pull-up from flux leakage. |
| `sda_pull_test.jlink` | Enable internal pull-up on SDA/SCL, check each rises. |

Diagnostic firmware variants: **`skip_bmi.conf`** (isolation build — never
addresses 0x69) and **`rcclock.conf`** (LFCLK = RC oscillator, for the BLE-connect
crystal test).

---

## 13. Firmware roadmap / TODO

1. **BMM350 bring-up** once IC5 is soldered (§9.5): Kconfig + overlay node + scan
   entry + magnetometer sampler.
2. **Heading / e-compass fusion** (BMI270 + BMM350) — the tracker feature.
3. **Reclaim QSPI quad** after the IO2/IO3 open is fixed (§11.3): restore
   `writeoc "pp4io"` / `readoc "read4io"`.
4. **Move VOUT1 to the VSET1 resistor** so the 2.5 V rail is correct before
   firmware runs (§4); keep the firmware override as belt-and-suspenders.
5. **NVS/LittleFS** partition at the bottom of the W25Q128.
6. **Wire the BMI270 DRDY interrupt** (P0.26/P0.27) instead of polling.
7. **MAX86140 PPG** when the J1 sensor board is populated (§9.9).
8. Rename `BT_DEVICE_NAME` from `WAND_TEST` to a Tracker_X identity for production.
9. Resolve the BLE-connect / LFCLK-crystal item (§11.4).

---

*Firmware: `C:\ncs\tracker_x_bringup` (NCS v3.0.2, board `nrf52840dk/nrf52840` +
overlay). Design: `D:\Altium\Tracker_X`. This guide reflects the board state and
firmware as of the July 2026 bring-up; the corrected power/level model in §4 and
the errata in §11 supersede the older companion docs where they conflict.*
