#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pet_tracker, LOG_LEVEL_DBG);

/* ──────────────────────────────────────────────────────────────────
 *  STATES
 * ────────────────────────────────────────────────────────────────── */
typedef enum {
    STATE_IDLE = 0,
    STATE_MOTION_DETECTED,
    STATE_LOGGING,
    STATE_BLE_NOTIFY,
} tracker_state_t;

static const char * const state_names[] = {
    [STATE_IDLE]            = "IDLE",
    [STATE_MOTION_DETECTED] = "MOTION_DETECTED",
    [STATE_LOGGING]         = "LOGGING",
    [STATE_BLE_NOTIFY]      = "BLE_NOTIFY",
};

/* ──────────────────────────────────────────────────────────────────
 *  STATE VARIABLE + SETUP GUARD  (atomic, same as Day 4)
 * ────────────────────────────────────────────────────────────────── */
static atomic_t fsm_state   = ATOMIC_INIT(STATE_IDLE);
static atomic_t setup_done  = ATOMIC_INIT(0);

/* ──────────────────────────────────────────────────────────────────
 *  WORK ITEMS — one per transition
 * ────────────────────────────────────────────────────────────────── */
static struct k_work motion_work;       /* IDLE            -> MOTION_DETECTED */
static struct k_work logging_work;      /* MOTION_DETECTED -> LOGGING         */
static struct k_work ble_notify_work;   /* LOGGING         -> BLE_NOTIFY      */
static struct k_work idle_work;         /* BLE_NOTIFY      -> IDLE            */

/* ──────────────────────────────────────────────────────────────────
 *  SIMULATED-MOTION TIMER — now PERIODIC
 * ────────────────────────────────────────────────────────────────── */
static struct k_timer motion_timer;

/* ──────────────────────────────────────────────────────────────────
 *  TRANSITION HELPER (unchanged logic from Day 4)
 *
 *  Logs every transition. The "[%datetime%]"-style prefix you see
 *  in the output comes from Zephyr's logging timestamp source,
 *  which is k_uptime_get() under the hood — monotonic since boot,
 *  never reset. We deliberately do NOT track our own "time since
 *  last transition" variable, which is what would cause the
 *  "timestamps reset" bug described in the edge cases.
 * ────────────────────────────────────────────────────────────────── */
static bool fsm_transition(tracker_state_t from, tracker_state_t to)
{
    if (atomic_cas(&fsm_state, (atomic_val_t)from, (atomic_val_t)to)) {
        LOG_INF("FSM  %-16s  -->  %-16s", state_names[from], state_names[to]);
        return true;
    }

    LOG_WRN("FSM  transition %s->%s rejected (current = %s)",
            state_names[from], state_names[to],
            state_names[(tracker_state_t)atomic_get(&fsm_state)]);
    return false;
}

/* ──────────────────────────────────────────────────────────────────
 *  WORK HANDLERS — each does ONE transition, then queues the next
 * ────────────────────────────────────────────────────────────────── */

/* IDLE -> MOTION_DETECTED */
static void motion_work_handler(struct k_work *work)
{
    if (!atomic_get(&setup_done)) {
        LOG_WRN("Motion event arrived before setup complete - dropped");
        return;
    }

    if (fsm_transition(STATE_IDLE, STATE_MOTION_DETECTED)) {
        k_work_submit(&logging_work);
    }
}

/* MOTION_DETECTED -> LOGGING */
static void logging_work_handler(struct k_work *work)
{
    if (fsm_transition(STATE_MOTION_DETECTED, STATE_LOGGING)) {
        LOG_INF("(LOGGING entry) recording GPS fix + accelerometer data...");
        k_work_submit(&ble_notify_work);
    }
}

/* LOGGING -> BLE_NOTIFY */
static void ble_notify_work_handler(struct k_work *work)
{
    if (fsm_transition(STATE_LOGGING, STATE_BLE_NOTIFY)) {
        LOG_INF("(BLE_NOTIFY entry) sending BLE notification to paired phone...");
        k_work_submit(&idle_work);
    }
}

/* BLE_NOTIFY -> IDLE  (closes the loop) */
static void idle_work_handler(struct k_work *work)
{
    if (fsm_transition(STATE_BLE_NOTIFY, STATE_IDLE)) {
        LOG_INF("(IDLE entry) back to sleep, waiting for next motion event");
    }
}

/* ──────────────────────────────────────────────────────────────────
 *  TIMER EXPIRY — runs in ISR context, only submits work
 * ────────────────────────────────────────────────────────────────── */
static void motion_timer_expiry_fn(struct k_timer *timer)
{
    k_work_submit(&motion_work);
}

/* ──────────────────────────────────────────────────────────────────
 *  MAIN
 * ────────────────────────────────────────────────────────────────── */
int main(void)
{
    LOG_INF("=== Pet Tracker FSM booting ===");
    LOG_INF("Initial state: %s",
            state_names[(tracker_state_t)atomic_get(&fsm_state)]);

    k_work_init(&motion_work,     motion_work_handler);
    k_work_init(&logging_work,    logging_work_handler);
    k_work_init(&ble_notify_work, ble_notify_work_handler);
    k_work_init(&idle_work,       idle_work_handler);

    /*
     * Periodic timer: first fire after 2s, then every 5s after that.
     * This is what makes the FULL LOOP repeat without any polling -
     * the timer itself is the only thing driving repetition.
     */
    k_timer_init(&motion_timer, motion_timer_expiry_fn, NULL);
    k_timer_start(&motion_timer, K_SECONDS(2), K_SECONDS(5));

    atomic_set(&setup_done, 1);
    LOG_INF("Setup complete. Simulated motion every 5s (first one in 2s)...");

    /*
     * Nothing left for this thread to do. All further behaviour is
     * event-driven (timer -> work queue -> transitions). Sleeping
     * forever here means: NO busy-wait, NO polling loop.
     */
    k_sleep(K_FOREVER);
    return 0;
}
