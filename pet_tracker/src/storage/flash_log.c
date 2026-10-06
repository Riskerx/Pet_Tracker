#include <string.h>
#include <zephyr/logging/log.h>

#include "flash_log.h"
#include "../app_state.h"

LOG_MODULE_REGISTER(flash_log, LOG_LEVEL_INF);

#define LOG_CAPACITY 64  /* records */

static struct tracker_log_record ring[LOG_CAPACITY];
static size_t head;   /* next write index          */
static size_t count;  /* valid records, <= CAPACITY */

void storage_init(void)
{
    head = 0;
    count = 0;
    LOG_WRN("Flash logging is STUBBED - records live in a %d-entry RAM ring "
            "buffer and are lost on reset. TODO: back this with zephyr/fs/nvs.h "
            "once the flash partition layout is finalized.", LOG_CAPACITY);
}

int storage_save(const struct tracker_log_record *rec)
{
    k_mutex_lock(&storage_lock, K_FOREVER);

    ring[head] = *rec;
    head = (head + 1) % LOG_CAPACITY;
    if (count < LOG_CAPACITY) {
        count++;
    }

    k_mutex_unlock(&storage_lock);

    LOG_INF("Saved record (t=%u ms, x=%d y=%d z=%d) - %u/%u in buffer",
            rec->timestamp_ms, rec->sample.x, rec->sample.y, rec->sample.z,
            (unsigned)count, LOG_CAPACITY);
    return 0;
}

int storage_read(struct tracker_log_record *out, size_t max_records, size_t *out_count)
{
    k_mutex_lock(&storage_lock, K_FOREVER);

    size_t n = MIN(max_records, count);
    size_t oldest = (head + LOG_CAPACITY - count) % LOG_CAPACITY;

    for (size_t i = 0; i < n; i++) {
        out[i] = ring[(oldest + i) % LOG_CAPACITY];
    }

    k_mutex_unlock(&storage_lock);

    *out_count = n;
    LOG_INF("Read %u records from buffer", (unsigned)n);
    return 0;
}

int storage_erase(void)
{
    k_mutex_lock(&storage_lock, K_FOREVER);
    memset(ring, 0, sizeof(ring));
    head = 0;
    count = 0;
    k_mutex_unlock(&storage_lock);

    LOG_INF("Log buffer erased");
    return 0;
}
