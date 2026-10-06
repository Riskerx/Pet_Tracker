#ifndef TRACKER_SERVICE_H
#define TRACKER_SERVICE_H
#include <stdint.h>
int tracker_service_init(void);
int tracker_service_set_value(uint8_t new_value);
#endif 