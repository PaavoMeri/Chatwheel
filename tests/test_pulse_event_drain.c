#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "mixer/pulse_event_drain.h"

typedef struct {
    const int *results;
    size_t result_count;
    size_t iterate_count;
    size_t reap_count;
    int all_calls_non_blocking;
    char call_order[32];
    size_t call_order_count;
} scripted_drain_t;

static void record_call(scripted_drain_t *script, char call) {
    if (script->call_order_count < sizeof(script->call_order)) {
        script->call_order[script->call_order_count] = call;
    }
    script->call_order_count++;
}

static int scripted_iterate(void *userdata, int block) {
    scripted_drain_t *script = userdata;
    assert(script->iterate_count < script->result_count);
    script->all_calls_non_blocking &= block == 0;
    record_call(script, 'I');
    return script->results[script->iterate_count++];
}

static int continuously_dispatching_iterate(void *userdata, int block) {
    scripted_drain_t *script = userdata;
    script->all_calls_non_blocking &= block == 0;
    script->iterate_count++;
    return 1;
}

static void scripted_reap(void *userdata) {
    scripted_drain_t *script = userdata;
    script->reap_count++;
    record_call(script, 'R');
}

static void assert_reap_follows_every_iteration(
    const scripted_drain_t *script) {
    assert(script->call_order_count == script->iterate_count * 2);
    for (size_t i = 0; i < script->iterate_count; i++) {
        assert(script->call_order[i * 2] == 'I');
        assert(script->call_order[i * 2 + 1] == 'R');
    }
}

static void test_finite_burst_drains_until_idle(void) {
    const int results[] = {3, 2, 1, 0};
    scripted_drain_t script = {
        .results = results,
        .result_count = sizeof(results) / sizeof(results[0]),
        .all_calls_non_blocking = 1,
    };

    assert(pulse_event_drain(
               scripted_iterate,
               &script,
               scripted_reap,
               &script) == PULSE_EVENT_DRAIN_IDLE);
    assert(script.iterate_count == 4);
    assert(script.reap_count == 4);
    assert(script.all_calls_non_blocking);
    assert_reap_follows_every_iteration(&script);
}

static void test_initial_idle_stops_after_one_iteration(void) {
    const int results[] = {0};
    scripted_drain_t script = {
        .results = results,
        .result_count = sizeof(results) / sizeof(results[0]),
        .all_calls_non_blocking = 1,
    };

    assert(pulse_event_drain(
               scripted_iterate,
               &script,
               scripted_reap,
               &script) == PULSE_EVENT_DRAIN_IDLE);
    assert(script.iterate_count == 1);
    assert(script.reap_count == 1);
    assert(script.all_calls_non_blocking);
    assert_reap_follows_every_iteration(&script);
}

static void test_negative_result_reports_error_after_reap(void) {
    const int results[] = {-1};
    scripted_drain_t script = {
        .results = results,
        .result_count = sizeof(results) / sizeof(results[0]),
        .all_calls_non_blocking = 1,
    };

    assert(pulse_event_drain(
               scripted_iterate,
               &script,
               scripted_reap,
               &script) == PULSE_EVENT_DRAIN_ERROR);
    assert(script.iterate_count == 1);
    assert(script.reap_count == 1);
    assert(script.all_calls_non_blocking);
    assert_reap_follows_every_iteration(&script);
}

static void test_iteration_budget_stops_continuous_dispatch(void) {
    scripted_drain_t script = {
        .all_calls_non_blocking = 1,
    };

    assert(pulse_event_drain(
               continuously_dispatching_iterate,
               &script,
               scripted_reap,
               &script) == PULSE_EVENT_DRAIN_BUDGET_EXHAUSTED);
    assert(script.iterate_count == PULSE_EVENT_DRAIN_ITERATION_BUDGET);
    assert(script.reap_count == PULSE_EVENT_DRAIN_ITERATION_BUDGET);
    assert(script.all_calls_non_blocking);
}

static void test_invalid_callbacks_are_rejected(void) {
    scripted_drain_t script = {0};

    assert(pulse_event_drain(
               NULL,
               &script,
               scripted_reap,
               &script) == PULSE_EVENT_DRAIN_ERROR);
    assert(pulse_event_drain(
               continuously_dispatching_iterate,
               &script,
               NULL,
               &script) == PULSE_EVENT_DRAIN_ERROR);
    assert(script.iterate_count == 0);
    assert(script.reap_count == 0);
}

int main(void) {
    test_finite_burst_drains_until_idle();
    test_initial_idle_stops_after_one_iteration();
    test_negative_result_reports_error_after_reap();
    test_iteration_budget_stops_continuous_dispatch();
    test_invalid_callbacks_are_rejected();

    printf("pulse event drain tests passed\n");
    return 0;
}
