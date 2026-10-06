#ifndef DATA_TRANSFER_H_
#define DATA_TRANSFER_H_

/*
 * Status: REAL.
 *
 * Ported from the NUS-based chunked-dump prototype, with one fix:
 * the original called the (blocking, k_msleep-paced) send loop
 * directly from the BLE RX callback. That callback runs in the BT
 * host's context, so a long blocking call there stalls other BLE
 * processing. Consolidated version only sets a flag and submits a
 * k_work item from the callback; the actual chunked send runs on
 * the system workqueue.
 */

void data_transfer_init(void);
void data_transfer_on_disconnected(void);

#endif /* DATA_TRANSFER_H_ */
