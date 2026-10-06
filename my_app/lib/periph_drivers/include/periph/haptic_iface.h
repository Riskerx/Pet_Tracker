#ifndef PERIPH_HAPTIC_IFACE_H_
#define PERIPH_HAPTIC_IFACE_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define HAPTIC_EFFECT_MIN 1
#define HAPTIC_EFFECT_MAX 123

    struct haptic_dev;

    struct haptic_api
    {
        int (*init)(struct haptic_dev *dev);
        int (*play_effect)(struct haptic_dev *dev, uint8_t effect_id);
        int (*stop)(struct haptic_dev *dev);
        bool (*is_playing)(const struct haptic_dev *dev);
    };

    struct haptic_dev
    {
        const struct haptic_api *api;
        void *ctx;
    };

    static inline int haptic_init(struct haptic_dev *dev)
    {
        return dev->api->init(dev);
    }

    static inline int haptic_play_effect(struct haptic_dev *dev, uint8_t effect_id)
    {
        return dev->api->play_effect(dev, effect_id);
    }

    static inline int haptic_stop(struct haptic_dev *dev)
    {
        return dev->api->stop(dev);
    }

    static inline bool haptic_is_playing(const struct haptic_dev *dev)
    {
        return dev->api->is_playing(dev);
    }

    struct haptic_dev *haptic_mock_get(void);
    struct haptic_dev *haptic_drv2605_get(void);

    struct haptic_dev *haptic_get_default(void);

#ifdef __cplusplus
}
#endif

#endif
