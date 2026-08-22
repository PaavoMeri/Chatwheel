#include "pulse_event_drain.h"

pulse_event_drain_result_t pulse_event_drain(
    pulse_event_iterate_fn iterate,
    void *iterate_userdata,
    pulse_event_reap_fn reap,
    void *reap_userdata) {
    if (!iterate || !reap) return PULSE_EVENT_DRAIN_ERROR;

    for (size_t i = 0; i < PULSE_EVENT_DRAIN_ITERATION_BUDGET; i++) {
        int dispatched = iterate(iterate_userdata, 0);
        reap(reap_userdata);

        if (dispatched < 0) return PULSE_EVENT_DRAIN_ERROR;
        if (dispatched == 0) return PULSE_EVENT_DRAIN_IDLE;
    }

    return PULSE_EVENT_DRAIN_BUDGET_EXHAUSTED;
}
