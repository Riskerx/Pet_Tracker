#ifndef POWER_MGMT_H_
#define POWER_MGMT_H_

/* Status: REAL. Inactivity sleep timer ported from the driver
 * prototype, plus two guards added while consolidating (see
 * architecture.md -> Integration Conflicts):
 *   - OTA in progress blocks sleep.
 *   - An active BLE connection blocks/cancels the sleep timer so
 *     advertising doesn't get shut off out from under a live link. */

void power_mgmt_init(void);

/* Call whenever there's BLE or motion activity - cancels any pending
 * sleep countdown and, if idle again, restarts it. */
void power_notify_activity(void);

/* Wraps the ota_active flag from app_state.h. */
void power_ota_lock(void);
void power_ota_unlock(void);

#endif /* POWER_MGMT_H_ */
