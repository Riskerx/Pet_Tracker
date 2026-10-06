# Pet Tracker — Behavioural State Machine (native_sim)

A pure-software prototype of the pet tracker's core behaviour loop,
built and tested on Zephyr's `native_sim/native/64` board before
moving to the real nRF52840 + BMI270 hardware.

## States

| State             | Meaning                                              |
|-------------------|------------------------------------------------------|
| `IDLE`            | Tracker is asleep, waiting for movement              |
| `MOTION_DETECTED` | Motion sensor has just triggered                     |
| `LOGGING`         | Recording a GPS fix + accelerometer reading          |
| `BLE_NOTIFY`      | Sending the logged data to the paired phone over BLE |

## State Diagram
IDLE ──(motion event)──► MOTION_DETECTED ──► LOGGING ──► BLE_NOTIFY ──► IDLE
The loop repeats indefinitely — once `BLE_NOTIFY` finishes, the
tracker returns to `IDLE` and waits for the next motion event.

## What Triggers Each Transition

| Transition                          | Trigger                                                        |
|--------------------------------------|-----------------------------------------------------------------|
| `IDLE → MOTION_DETECTED`             | A `k_timer` fires (simulates the BMI270 motion interrupt)       |
| `MOTION_DETECTED → LOGGING`          | Automatic — chained immediately after the previous transition   |
| `LOGGING → BLE_NOTIFY`               | Automatic — chained after the "logging" entry action runs       |
| `BLE_NOTIFY → IDLE`                  | Automatic — chained after the "BLE notify" entry action runs    |

On real hardware, only the first transition's trigger changes: the
`k_timer` is replaced by the BMI270's motion-interrupt GPIO callback.
Everything downstream (the chain of transitions) stays identical.

## How Zephyr Primitives Drive This

- **`k_timer`** — a periodic timer (first fire at 2s, then every 5s)
  stands in for the motion sensor. Its expiry function runs in ISR
  context, so it does the absolute minimum: submit one `k_work` item.

- **`k_work` (system work queue)** — each transition has its own
  work item. A work handler:
  1. Attempts its transition via `fsm_transition()`
  2. If successful, runs that state's "entry action" (a log message
     standing in for real GPS/IMU/BLE code)
  3. Submits the *next* work item, chaining the loop forward

  Running in the work queue thread (not an ISR) means we're free to
  log, and later, to call blocking BLE/sensor APIs safely.

- **`atomic_t` + `atomic_cas`** — the current state lives in an
  atomic variable. Every transition uses compare-and-swap: "move from
  state A to state B, but only if we're currently in A." This
  guarantees each transition fires **exactly once**, even if a work
  item were ever submitted twice.

- **No polling** — `main()` initializes everything, arms the timer,
  then calls `k_sleep(K_FOREVER)`. There is no loop anywhere checking
  "has the state changed yet?" — the system is entirely event-driven
  from the timer onward.

## Edge Cases Handled

- **Motion event before setup finishes**: a `setup_done` atomic flag
  is set only after all init is complete. If the motion work handler
  runs before that, it logs a warning and drops the event.

- **Duplicate/early transition triggers**: `atomic_cas` ensures a
  transition only succeeds if the FSM is in the expected starting
  state. A duplicate or out-of-order trigger is rejected and logged
  as a warning, never silently corrupting state.

- **Timestamps**: all log timestamps come from Zephyr's built-in
  logging timestamp source (monotonic time since boot). No custom
  "time since last transition" variable is used, so timestamps always
  count upward and never reset between transitions.

## Building & Running

```bash
west build -b native_sim/native/64 pet_tracker_fsm -p always
./build/zephyr/zephyr.exe
```

Expect a full `IDLE → MOTION_DETECTED → LOGGING → BLE_NOTIFY → IDLE`
cycle approximately every 5 seconds, with strictly increasing
timestamps on every log line.

## Next Steps (Hardware Port)

- Replace `motion_timer_expiry_fn` with the BMI270 motion-interrupt
  GPIO callback — the rest of the FSM requires no changes.
- Replace the placeholder log messages in each entry action with real
  GPS/accelerometer reads and a BLE GATT notification send.
