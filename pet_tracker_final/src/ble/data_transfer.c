#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/conn.h>
#include <bluetooth/services/nus.h>
#include <zephyr/logging/log.h>

#include "data_transfer.h"
#include "ble_adv.h"
#include "../storage/flash_log.h"
#include "../app_state.h"

LOG_MODULE_REGISTER(data_transfer, LOG_LEVEL_INF);

#define CHUNK_SIZE     20
#define MAX_DUMP_RECS  64  /* matches storage's LOG_CAPACITY */

static struct k_work transfer_work;

static void transfer_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    struct bt_conn *conn = ble_adv_get_conn();
    if (!conn) {
        LOG_WRN("Dump requested but no active connection - aborting");
        return;
    }

    atomic_set(&ble_transfer_active, 1);

    static struct tracker_log_record recs[MAX_DUMP_RECS];
    size_t n = 0;

    storage_read(recs, MAX_DUMP_RECS, &n);

    size_t total_bytes = n * sizeof(struct tracker_log_record);
    LOG_INF("Starting data transfer (%u records, %u bytes)", (unsigned)n, (unsigned)total_bytes);

    const uint8_t *bytes = (const uint8_t *)recs;
    size_t offset = 0;

    while (offset < total_bytes) {
        size_t remaining = total_bytes - offset;
        size_t chunk_len = remaining > CHUNK_SIZE ? CHUNK_SIZE : remaining;

        int err = bt_nus_send(conn, &bytes[offset], chunk_len);
        if (err) {
            LOG_ERR("Failed to send chunk at offset %u (err %d)", (unsigned)offset, err);
            break;
        }

        offset += chunk_len;
        k_msleep(20); /* simple pacing so we don't outrun the connection interval */
    }

    LOG_INF("Data transfer complete (%u/%u bytes sent)", (unsigned)offset, (unsigned)total_bytes);
    atomic_set(&ble_transfer_active, 0);
}

static void nus_received_cb(struct bt_conn *conn, const uint8_t *data, uint16_t len)
{
    ARG_UNUSED(conn);
    LOG_INF("Received %u bytes over NUS", len);

    if (len >= 4 && memcmp(data, "DUMP", 4) == 0) {
        LOG_INF("DUMP command received - queuing transfer");
        k_work_submit(&transfer_work);
    } else {
        LOG_WRN("Unrecognized command received");
    }
}

static struct bt_nus_cb nus_cb = {
    .received = nus_received_cb,
};

void data_transfer_init(void)
{
    k_work_init(&transfer_work, transfer_work_handler);

    int err = bt_nus_init(&nus_cb);
    if (err) {
        LOG_ERR("bt_nus_init failed (err %d)", err);
        return;
    }
    LOG_INF("NUS data transfer service ready");
}

void data_transfer_on_disconnected(void)
{
    /* transfer_work checks ble_adv_get_conn() itself and bails out
     * if it's NULL, so nothing else to clean up here beyond letting
     * any already-queued work run its (harmless) course. */
}
