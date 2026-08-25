#include "pulse_reconnect_state.h"

#include <stddef.h>
#include <stdint.h>

static const uint64_t retry_delays_ns[] = {
    UINT64_C(1) * PULSE_RECONNECT_NANOSECONDS_PER_SECOND,
    UINT64_C(2) * PULSE_RECONNECT_NANOSECONDS_PER_SECOND,
    UINT64_C(4) * PULSE_RECONNECT_NANOSECONDS_PER_SECOND,
    UINT64_C(8) * PULSE_RECONNECT_NANOSECONDS_PER_SECOND,
    UINT64_C(16) * PULSE_RECONNECT_NANOSECONDS_PER_SECOND,
    UINT64_C(30) * PULSE_RECONNECT_NANOSECONDS_PER_SECOND,
};

void pulse_reconnect_state_init(pulse_reconnect_state_t *state) {
    if (!state) return;
    *state = (pulse_reconnect_state_t){
        .connection_state = PULSE_RECONNECT_CONNECTED,
    };
}

uint64_t pulse_reconnect_deadline_after(uint64_t now_ns, uint64_t delay_ns) {
    if (delay_ns > UINT64_MAX - now_ns) return UINT64_MAX;
    return now_ns + delay_ns;
}

pulse_reconnect_log_event_t pulse_reconnect_state_schedule_retry(
    pulse_reconnect_state_t *state,
    uint64_t now_ns,
    uint64_t *delay_ns) {
    if (!state || !delay_ns) return PULSE_RECONNECT_LOG_NONE;

    size_t delay_count = sizeof(retry_delays_ns) / sizeof(retry_delays_ns[0]);
    size_t index = state->backoff_index;
    if (index >= delay_count) index = delay_count - 1;

    *delay_ns = retry_delays_ns[index];
    state->retry_deadline_ns = pulse_reconnect_deadline_after(
        now_ns,
        *delay_ns);
    state->retry_pending = 1;
    state->connection_state = PULSE_RECONNECT_RETRY_WAIT;
    if (state->backoff_index < delay_count - 1) {
        state->backoff_index++;
    }

    if (state->outage_active) return PULSE_RECONNECT_LOG_NONE;
    state->outage_active = 1;
    return PULSE_RECONNECT_LOG_OUTAGE;
}

int pulse_reconnect_state_take_due_retry(
    pulse_reconnect_state_t *state,
    uint64_t now_ns) {
    if (!state ||
        state->connection_state != PULSE_RECONNECT_RETRY_WAIT ||
        !state->retry_pending ||
        now_ns < state->retry_deadline_ns) {
        return 0;
    }

    state->retry_pending = 0;
    return 1;
}

pulse_reconnect_log_event_t pulse_reconnect_state_mark_connected(
    pulse_reconnect_state_t *state) {
    if (!state) return PULSE_RECONNECT_LOG_NONE;

    int recovered = state->outage_active;
    pulse_reconnect_state_init(state);
    return recovered
        ? PULSE_RECONNECT_LOG_RECOVERY
        : PULSE_RECONNECT_LOG_NONE;
}

int pulse_reconnect_state_is_connected(const pulse_reconnect_state_t *state) {
    return state &&
        state->connection_state == PULSE_RECONNECT_CONNECTED;
}

int pulse_reconnect_state_is_waiting(const pulse_reconnect_state_t *state) {
    return state &&
        state->connection_state == PULSE_RECONNECT_RETRY_WAIT;
}
