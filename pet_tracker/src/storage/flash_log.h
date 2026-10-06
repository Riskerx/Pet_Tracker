#ifndef FLASH_LOG_H_
#define FLASH_LOG_H_

#include <stdint.h>
#include <stddef.h>
#include "../sensor/motion_sensor.h"

/* Status: STUB. No module in the original set implemented a real
 * flash/NVS backend, so this is a RAM-backed ring buffer standing in
 * behind the real API. Swapping the body of these three functions
 * for zephyr/fs/nvs.h calls is the only change needed elsewhere in
 * the project - callers only ever see this header. */

struct tracker_log_record {
    uint32_t timestamp_ms;
    struct motion_sample sample;
};

void storage_init(void);

/* Returns 0 on success, negative errno on failure. Thread-safe
 * (guarded by app_state's storage_lock). */
int storage_save(const struct tracker_log_record *rec);

/* Copies up to max_records into out, oldest-first. *out_count is set
 * to however many were actually copied. */
int storage_read(struct tracker_log_record *out, size_t max_records, size_t *out_count);

int storage_erase(void);

#endif /* FLASH_LOG_H_ */
