#ifndef HEADSET_POLL_H
#define HEADSET_POLL_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* Tunable interval between a completed query and the next eligible query. */
#define HEADSET_POLL_INTERVAL_MS UINT64_C(500)
#define HEADSET_POLL_INTERVAL_NS \
    (HEADSET_POLL_INTERVAL_MS * UINT64_C(1000000))

typedef struct {
    bool has_completion;
    uint64_t last_completion_ns;
} headset_poll_state_t;

void headset_poll_state_init(headset_poll_state_t *state);

/* An initialized state is due immediately, before its first completion. */
bool headset_poll_is_due(const headset_poll_state_t *state, uint64_t now_ns);

/* Schedule from completion time, so delayed polls never create catch-up bursts. */
void headset_poll_mark_completed(
    headset_poll_state_t *state,
    uint64_t completion_ns);

/*
 * Convert a normalized non-negative monotonic timespec to nanoseconds.
 * NULL, negative, out-of-range nanoseconds and arithmetic overflow fail.
 */
bool headset_monotonic_timespec_ns(
    const struct timespec *timestamp,
    uint64_t *nanoseconds);

#endif // HEADSET_POLL_H
