#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "mixer/mixer.h"
#include "mixer/pulse_reconnect_state.h"

static void test_backoff_sequence_and_single_due_attempt(void) {
    static const uint64_t expected_seconds[] = {1, 2, 4, 8, 16, 30, 30};
    pulse_reconnect_state_t state;
    uint64_t now_ns = UINT64_C(100000000000);
    pulse_reconnect_state_init(&state);

    for (size_t i = 0;
         i < sizeof(expected_seconds) / sizeof(expected_seconds[0]);
         i++) {
        uint64_t delay_ns = 0;
        pulse_reconnect_log_event_t event =
            pulse_reconnect_state_schedule_retry(&state, now_ns, &delay_ns);
        assert(event == (i == 0
            ? PULSE_RECONNECT_LOG_OUTAGE
            : PULSE_RECONNECT_LOG_NONE));
        assert(delay_ns ==
               expected_seconds[i] * PULSE_RECONNECT_NANOSECONDS_PER_SECOND);
        assert(pulse_reconnect_state_is_waiting(&state));
        assert(!pulse_reconnect_state_take_due_retry(
            &state,
            state.retry_deadline_ns - 1));
        assert(pulse_reconnect_state_take_due_retry(
            &state,
            state.retry_deadline_ns));
        assert(!pulse_reconnect_state_take_due_retry(
            &state,
            state.retry_deadline_ns));
        now_ns = state.retry_deadline_ns;
    }
}

static void test_success_resets_backoff_and_log_deduplication(void) {
    pulse_reconnect_state_t state;
    uint64_t delay_ns;
    pulse_reconnect_state_init(&state);

    assert(pulse_reconnect_state_schedule_retry(
               &state, 0, &delay_ns) == PULSE_RECONNECT_LOG_OUTAGE);
    assert(delay_ns == PULSE_RECONNECT_NANOSECONDS_PER_SECOND);
    assert(pulse_reconnect_state_schedule_retry(
               &state, delay_ns, &delay_ns) == PULSE_RECONNECT_LOG_NONE);
    assert(delay_ns == UINT64_C(2) * PULSE_RECONNECT_NANOSECONDS_PER_SECOND);

    assert(pulse_reconnect_state_mark_connected(&state) ==
           PULSE_RECONNECT_LOG_RECOVERY);
    assert(pulse_reconnect_state_is_connected(&state));
    assert(pulse_reconnect_state_mark_connected(&state) ==
           PULSE_RECONNECT_LOG_NONE);

    assert(pulse_reconnect_state_schedule_retry(
               &state, 0, &delay_ns) == PULSE_RECONNECT_LOG_OUTAGE);
    assert(delay_ns == PULSE_RECONNECT_NANOSECONDS_PER_SECOND);
}

static void test_deadline_overflow_saturates(void) {
    pulse_reconnect_state_t state;
    uint64_t delay_ns;
    pulse_reconnect_state_init(&state);

    assert(pulse_reconnect_deadline_after(UINT64_MAX - 5, 5) == UINT64_MAX);
    assert(pulse_reconnect_deadline_after(UINT64_MAX - 5, 6) == UINT64_MAX);
    assert(pulse_reconnect_state_schedule_retry(
               &state, UINT64_MAX - 10, &delay_ns) ==
           PULSE_RECONNECT_LOG_OUTAGE);
    assert(state.retry_deadline_ns == UINT64_MAX);
    assert(!pulse_reconnect_state_take_due_retry(&state, UINT64_MAX - 1));
    assert(pulse_reconnect_state_take_due_retry(&state, UINT64_MAX));
}

static void test_null_contracts(void) {
    uint64_t delay_ns = 123;
    pulse_reconnect_state_init(NULL);
    assert(pulse_reconnect_state_schedule_retry(NULL, 0, &delay_ns) ==
           PULSE_RECONNECT_LOG_NONE);
    assert(delay_ns == 123);
    pulse_reconnect_state_t state;
    pulse_reconnect_state_init(&state);
    assert(pulse_reconnect_state_schedule_retry(&state, 0, NULL) ==
           PULSE_RECONNECT_LOG_NONE);
    assert(!pulse_reconnect_state_take_due_retry(NULL, 0));
    assert(pulse_reconnect_state_mark_connected(NULL) ==
           PULSE_RECONNECT_LOG_NONE);
    assert(!pulse_reconnect_state_is_connected(NULL));
    assert(!pulse_reconnect_state_is_waiting(NULL));
}

static void test_audio_status_mapping(void) {
    assert(audio_event_status_from_observation(PA_CONTEXT_READY, 0) ==
           AUDIO_EVENTS_OK);
    assert(audio_event_status_from_observation(PA_CONTEXT_FAILED, 0) ==
           AUDIO_EVENTS_CONNECTION_LOST);
    assert(audio_event_status_from_observation(PA_CONTEXT_TERMINATED, 0) ==
           AUDIO_EVENTS_CONNECTION_LOST);
    assert(audio_event_status_from_observation(PA_CONTEXT_READY, 1) ==
           AUDIO_EVENTS_MAINLOOP_ERROR);
    assert(audio_event_status_from_observation(PA_CONTEXT_FAILED, 1) ==
           AUDIO_EVENTS_CONNECTION_LOST);
}

static void test_init_wait_decisions(void) {
    assert(audio_init_wait_decide(0, 0, 0, 99, 100) ==
           AUDIO_INIT_WAIT_PENDING);
    assert(audio_init_wait_decide(1, 0, 0, 99, 100) ==
           AUDIO_INIT_WAIT_SUCCESS);
    assert(audio_init_wait_decide(0, 1, 0, 99, 100) ==
           AUDIO_INIT_WAIT_FAILURE);
    assert(audio_init_wait_decide(0, 0, 0, 100, 100) ==
           AUDIO_INIT_WAIT_TIMEOUT);
    assert(audio_init_wait_decide(1, 0, 0, 100, 100) ==
           AUDIO_INIT_WAIT_TIMEOUT);
    assert(audio_init_wait_decide(1, 0, 0, 101, 100) ==
           AUDIO_INIT_WAIT_TIMEOUT);
    assert(audio_init_wait_decide(1, 0, 1, 99, 100) ==
           AUDIO_INIT_WAIT_SHUTDOWN);
    assert(audio_init_wait_decide(0, 0, 1, 99, 100) ==
           AUDIO_INIT_WAIT_SHUTDOWN);
    assert(audio_init_wait_decide(1, 1, 1, 100, 100) ==
           AUDIO_INIT_WAIT_SHUTDOWN);
    assert(audio_init_wait_decide(1, 1, 0, 100, 100) ==
           AUDIO_INIT_WAIT_TIMEOUT);
}

static void test_shared_deadline_across_init_phases(void) {
    const uint64_t deadline_ns = 500;

    assert(audio_init_wait_decide(1, 0, 0, 100, deadline_ns) ==
           AUDIO_INIT_WAIT_SUCCESS);
    assert(audio_init_wait_decide(1, 0, 0, 200, deadline_ns) ==
           AUDIO_INIT_WAIT_SUCCESS);
    assert(audio_init_wait_decide(1, 0, 0, 300, deadline_ns) ==
           AUDIO_INIT_WAIT_SUCCESS);

    /* A completed rebuild permits replay only while the shared deadline holds. */
    assert(audio_init_wait_decide(1, 0, 0, 499, deadline_ns) ==
           AUDIO_INIT_WAIT_SUCCESS);
    assert(audio_init_wait_decide(1, 0, 0, 500, deadline_ns) ==
           AUDIO_INIT_WAIT_TIMEOUT);
    assert(audio_init_wait_decide(1, 0, 1, 499, deadline_ns) ==
           AUDIO_INIT_WAIT_SHUTDOWN);
}

int main(void) {
    test_backoff_sequence_and_single_due_attempt();
    test_success_resets_backoff_and_log_deduplication();
    test_deadline_overflow_saturates();
    test_null_contracts();
    test_audio_status_mapping();
    test_init_wait_decisions();
    test_shared_deadline_across_init_phases();

    printf("pulse reconnect state tests passed\n");
    return 0;
}
