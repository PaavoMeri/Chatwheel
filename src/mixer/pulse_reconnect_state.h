#ifndef PULSE_RECONNECT_STATE_H
#define PULSE_RECONNECT_STATE_H

#include <stdint.h>

#define PULSE_RECONNECT_NANOSECONDS_PER_SECOND UINT64_C(1000000000)

typedef enum {
    PULSE_RECONNECT_CONNECTED,
    PULSE_RECONNECT_RETRY_WAIT
} pulse_reconnect_connection_state_t;

typedef enum {
    PULSE_RECONNECT_LOG_NONE,
    PULSE_RECONNECT_LOG_OUTAGE,
    PULSE_RECONNECT_LOG_RECOVERY
} pulse_reconnect_log_event_t;

typedef struct {
    pulse_reconnect_connection_state_t connection_state;
    unsigned int backoff_index;
    uint64_t retry_deadline_ns;
    int retry_pending;
    int outage_active;
} pulse_reconnect_state_t;

/* Initialize before the daemon's first connection attempt. */
void pulse_reconnect_state_init(pulse_reconnect_state_t *state);

/* Saturating deadline addition shared by reconnect and init timeouts. */
uint64_t pulse_reconnect_deadline_after(uint64_t now_ns, uint64_t delay_ns);

/*
 * Schedule the next retry after a failed initial attempt, lost connection, or
 * failed retry. delay_ns receives 1, 2, 4, 8, 16, or 30 seconds, capped at
 * 30 seconds. Only the first failure in an outage reports LOG_OUTAGE.
 */
pulse_reconnect_log_event_t pulse_reconnect_state_schedule_retry(
    pulse_reconnect_state_t *state,
    uint64_t now_ns,
    uint64_t *delay_ns);

/*
 * Returns nonzero once when the retry deadline has been reached. The retry is
 * consumed until schedule_retry() records its failure or mark_connected()
 * records its success.
 */
int pulse_reconnect_state_take_due_retry(
    pulse_reconnect_state_t *state,
    uint64_t now_ns);

/* Mark a complete connect+subscribe+snapshot+rebuild success. */
pulse_reconnect_log_event_t pulse_reconnect_state_mark_connected(
    pulse_reconnect_state_t *state);

int pulse_reconnect_state_is_connected(const pulse_reconnect_state_t *state);
int pulse_reconnect_state_is_waiting(const pulse_reconnect_state_t *state);

#endif
