#include "app_state.h"

struct k_mutex storage_lock;
atomic_t ota_active = ATOMIC_INIT(0);
atomic_t ble_transfer_active = ATOMIC_INIT(0);

void app_state_init(void)
{
    k_mutex_init(&storage_lock);
}
