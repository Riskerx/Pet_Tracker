#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>

struct storage_record
{
    uint32_t uptime_ms;
    uint8_t event_type;
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
};

int storage_init(void);

int storage_save(const struct storage_record *rec);

uint16_t storage_get_count(void);

#endif