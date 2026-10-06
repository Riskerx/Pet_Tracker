#ifndef PERIPH_MAG_IFACE_H_
#define PERIPH_MAG_IFACE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    struct mag_sample
    {
        int32_t x;
        int32_t y;
        int32_t z;
    };

    struct mag_dev;

    struct mag_api
    {
        int (*init)(struct mag_dev *dev);
        int (*sample_fetch)(struct mag_dev *dev);
        int (*get_data)(struct mag_dev *dev, struct mag_sample *sample);
    };

    struct mag_dev
    {
        const struct mag_api *api;
        void *ctx;
    };

    static inline int mag_init(struct mag_dev *dev)
    {
        return dev->api->init(dev);
    }

    static inline int mag_sample_fetch(struct mag_dev *dev)
    {
        return dev->api->sample_fetch(dev);
    }

    static inline int mag_get_data(struct mag_dev *dev, struct mag_sample *sample)
    {
        return dev->api->get_data(dev, sample);
    }

    struct mag_dev *mag_mock_get(void);
    struct mag_dev *mag_bmm350_get(void);

    struct mag_dev *mag_get_default(void);

#ifdef __cplusplus
}
#endif

#endif
