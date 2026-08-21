#ifndef CHATMIX_VOLUME_H
#define CHATMIX_VOLUME_H

#include <pulse/volume.h>

typedef struct {
    float linear;
    float logarithmic;
    pa_volume_t pulse;
} chatmix_volume_target_t;

typedef struct {
    float normalized;
    chatmix_volume_target_t game;
    chatmix_volume_target_t chat;
} chatmix_volume_targets_t;

/*
 * Converts one finite linear target in the inclusive range 0.0 to 1.0 using
 * Chatwheel's existing volume curve. The calculation has no side effects and
 * does not retain output. Returns 0 on success and -1 for an invalid value or
 * NULL output. On failure, output remains unchanged.
 */
int chatmix_volume_target_calculate(float linear,
                                    chatmix_volume_target_t *output);

/*
 * Calculates Game and Chat targets for one finite raw ChatMix value. The
 * accepted range is defined by CHATMIX_MIN and CHATMIX_MAX in headset.h. The
 * calculation has no side effects and does not retain output. Returns 0 on
 * success and -1 for an out-of-range value or NULL output. On failure, output
 * remains unchanged.
 */
int chatmix_volume_targets_calculate(float raw,
                                     chatmix_volume_targets_t *output);

#endif
