#ifndef PERIPH_HAPTIC_MOCK_H_
#define PERIPH_HAPTIC_MOCK_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    struct drv2605_mock_regs
    {
        uint8_t mode;   
        uint8_t library;   
        uint8_t waveseq[8]; 
        uint8_t go;       
    };

    const struct drv2605_mock_regs *haptic_mock_peek_regs(void);

#ifdef __cplusplus
}
#endif

#endif 
