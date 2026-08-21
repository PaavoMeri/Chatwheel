#include "chatmix_volume.h"

#include <math.h>

#include "../headset/headset.h"

int chatmix_volume_target_calculate(float linear,
                                    chatmix_volume_target_t *output) {
    if (!output || !isfinite(linear) || linear < 0.0f || linear > 1.0f) {
        return -1;
    }

    chatmix_volume_target_t result = {
        .linear = linear,
    };

    if (linear < 0.01f) {
        result.logarithmic = 0.0f;
        result.pulse = PA_VOLUME_MUTED;
    } else {
        result.logarithmic = (powf(10.0f, linear) - 1.0f) / 9.0f;
        result.pulse =
            (pa_volume_t)(result.logarithmic * PA_VOLUME_NORM);
    }

    *output = result;
    return 0;
}

int chatmix_volume_targets_calculate(float raw,
                                     chatmix_volume_targets_t *output) {
    if (!output ||
        !isfinite(raw) ||
        raw < (float)CHATMIX_MIN ||
        raw > (float)CHATMIX_MAX) {
        return -1;
    }

    chatmix_volume_targets_t result = {0};
    result.normalized = raw / (float)CHATMIX_MAX;

    if (chatmix_volume_target_calculate(
            1.0f - result.normalized,
            &result.game) != 0 ||
        chatmix_volume_target_calculate(
            result.normalized,
            &result.chat) != 0) {
        return -1;
    }

    *output = result;
    return 0;
}
