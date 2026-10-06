#include <periph/haptic_iface.h>
#include <periph/haptic_mock.h>
#include <errno.h>
#include <string.h>

static struct drv2605_mock_regs regs;
static struct haptic_dev handle;

static int mock_init(struct haptic_dev *dev)
{
    (void)dev;

    memset(&regs, 0, sizeof(regs));
    regs.mode = 0x00;   
    regs.library = 0x01; 
    return 0;
}

static int mock_play_effect(struct haptic_dev *dev, uint8_t effect_id)
{
    (void)dev;

    if (effect_id < HAPTIC_EFFECT_MIN || effect_id > HAPTIC_EFFECT_MAX)
    {
        return -EINVAL;
    }

  
    regs.waveseq[0] = effect_id;
    regs.waveseq[1] = 0x00;

 
    regs.go = 0x01;

    return 0;
}

static int mock_stop(struct haptic_dev *dev)
{
    (void)dev;
    regs.go = 0x00;
    return 0;
}

static bool mock_is_playing(const struct haptic_dev *dev)
{
    (void)dev;
    return regs.go != 0;
}

static const struct haptic_api mock_api = {
    .init = mock_init,
    .play_effect = mock_play_effect,
    .stop = mock_stop,
    .is_playing = mock_is_playing,
};

struct haptic_dev *haptic_mock_get(void)
{
    handle.api = &mock_api;
    handle.ctx = &regs;
    return &handle;
}

const struct drv2605_mock_regs *haptic_mock_peek_regs(void)
{
    return &regs;
}
