#ifndef PULSE_EVENT_DRAIN_H
#define PULSE_EVENT_DRAIN_H

#include <stddef.h>

/*
 * A high finite limit prevents a continuously ready event source from
 * starving the daemon's headset polling forever. Normal finite bursts are
 * drained until iterate() first reports that no work was dispatched.
 */
#define PULSE_EVENT_DRAIN_ITERATION_BUDGET 4096U

typedef int (*pulse_event_iterate_fn)(void *userdata, int block);
typedef void (*pulse_event_reap_fn)(void *userdata);

typedef enum {
    PULSE_EVENT_DRAIN_ERROR = -1,
    PULSE_EVENT_DRAIN_IDLE = 0,
    PULSE_EVENT_DRAIN_BUDGET_EXHAUSTED = 1
} pulse_event_drain_result_t;

/*
 * iterate and reap must be non-NULL; if either is NULL, returns
 * PULSE_EVENT_DRAIN_ERROR. iterate_userdata and reap_userdata are separate,
 * independent function parameters. Their pointer values may be equal, each may
 * be NULL, and each is passed unchanged only to its corresponding callback.
 * The caller owns them and is responsible for their valid lifetime;
 * this function does not interpret, retain, or free them. A callback may modify
 * the state referenced by its userdata according to that callback's contract.
 *
 * Callbacks run synchronously during this call. Each iterate call uses block 0
 * and is followed by exactly one reap call, including after a zero or negative
 * result. The first zero result returns PULSE_EVENT_DRAIN_IDLE, a negative
 * result returns PULSE_EVENT_DRAIN_ERROR, and 4096 consecutive positive results
 * return PULSE_EVENT_DRAIN_BUDGET_EXHAUSTED.
 */
pulse_event_drain_result_t pulse_event_drain(
    pulse_event_iterate_fn iterate,
    void *iterate_userdata,
    pulse_event_reap_fn reap,
    void *reap_userdata);

#endif
