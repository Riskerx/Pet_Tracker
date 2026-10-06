

#include <periph/haptic_iface.h>
#include <periph/mag_iface.h>

struct haptic_dev *haptic_get_default(void)
{
#if defined(CONFIG_PERIPH_HAPTIC_USE_REAL)
    return haptic_drv2605_get();
#else
    return haptic_mock_get();
#endif
}

struct mag_dev *mag_get_default(void)
{
#if defined(CONFIG_PERIPH_MAG_USE_REAL)
    return mag_bmm350_get();
#else
    return mag_mock_get();
#endif
}
