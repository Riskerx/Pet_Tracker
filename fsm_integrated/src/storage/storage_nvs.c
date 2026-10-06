#include <zephyr/kernel.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/logging/log.h>

#include "storage/storage.h"

LOG_MODULE_REGISTER(storage, LOG_LEVEL_INF);

static struct nvs_fs nvs;
static uint16_t record_count = 0;

int storage_init(void)
{
    int rc;

    nvs.flash_device =
        FIXED_PARTITION_DEVICE(storage_partition);

    nvs.offset =
        FIXED_PARTITION_OFFSET(storage_partition);

    nvs.sector_size = 4096;
    nvs.sector_count = 2;

    rc = nvs_mount(&nvs);

    if (rc)
    {
        LOG_ERR("NVS mount failed");
        return rc;
    }

    nvs_read(&nvs,
             1,
             &record_count,
             sizeof(record_count));

    LOG_INF("Storage Ready");
    LOG_INF("Previous Records = %d",
            record_count);

    return 0;
}

int storage_save(const struct storage_record *rec)
{
    record_count++;

    nvs_write(&nvs,
              record_count + 1,
              rec,
              sizeof(*rec));

    nvs_write(&nvs,
              1,
              &record_count,
              sizeof(record_count));

    LOG_INF("Record %d Saved",
            record_count);

    return 0;
}

uint16_t storage_get_count(void)
{
    return record_count;
}