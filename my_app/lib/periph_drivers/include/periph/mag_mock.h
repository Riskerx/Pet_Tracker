#ifndef PERIPH_MAG_MOCK_H_
#define PERIPH_MAG_MOCK_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    struct bmm350_mock_regs
    {
        uint8_t chip_id; 
        uint8_t pmu_cmd; 
        uint32_t fetch_count;
    };

    const struct bmm350_mock_regs *mag_mock_peek_regs(void);

#ifdef __cplusplus
}
#endif

#endif 
