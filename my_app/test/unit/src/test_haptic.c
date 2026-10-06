#include <zephyr/ztest.h>
#include <errno.h>
#include <periph/haptic_iface.h>
#include <periph/haptic_mock.h>

static struct haptic_dev *hap;

static void *haptic_suite_setup(void)
{
    hap = haptic_mock_get();
    return NULL;
}

static void haptic_before(void *fixture)
{
    (void)fixture;
    zassert_ok(haptic_init(hap), "mock init should always succeed");
}

ZTEST_SUITE(haptic_mock, NULL, haptic_suite_setup, haptic_before, NULL, NULL);

ZTEST(haptic_mock, test_init_sets_known_register_state)
{
    const struct drv2605_mock_regs *regs = haptic_mock_peek_regs();

    zassert_equal(regs->go, 0, "GO bit must be clear after init");
    zassert_equal(regs->waveseq[0], 0, "sequencer must be empty after init");
}

ZTEST(haptic_mock, test_play_effect_sets_go_bit)
{
    zassert_ok(haptic_play_effect(hap, 47));

    const struct drv2605_mock_regs *regs = haptic_mock_peek_regs();

    zassert_equal(regs->waveseq[0], 47, "effect id should land in slot 0");
    zassert_equal(regs->waveseq[1], 0, "slot 1 should terminate the sequence");
    zassert_equal(regs->go, 1, "GO bit should be set to start playback");
    zassert_true(haptic_is_playing(hap));
}

ZTEST(haptic_mock, test_stop_clears_go_bit)
{
    zassert_ok(haptic_play_effect(hap, 1));
    zassert_true(haptic_is_playing(hap));

    zassert_ok(haptic_stop(hap));

    zassert_false(haptic_is_playing(hap));
    zassert_equal(haptic_mock_peek_regs()->go, 0);
}

ZTEST(haptic_mock, test_effect_id_zero_is_rejected)
{

    zassert_equal(haptic_play_effect(hap, 0), -EINVAL);
}

ZTEST(haptic_mock, test_effect_id_above_max_is_rejected)
{
    zassert_equal(haptic_play_effect(hap, HAPTIC_EFFECT_MAX + 1), -EINVAL);
}

ZTEST(haptic_mock, test_effect_id_boundaries_are_accepted)
{
    zassert_ok(haptic_play_effect(hap, HAPTIC_EFFECT_MIN));
    zassert_ok(haptic_play_effect(hap, HAPTIC_EFFECT_MAX));
}
