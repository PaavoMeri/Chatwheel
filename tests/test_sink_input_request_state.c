#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "mixer/sink_input_request_state.h"

static void assert_tracker_is_cleared(
    const sink_input_request_tracker_t *tracker) {
    assert(tracker->indexes == NULL);
    assert(tracker->index_count == 0);
    assert(tracker->index_capacity == 0);
    assert(tracker->requests == NULL);
}

static void test_supported_null_arguments(void) {
    sink_input_request_tracker_t tracker;
    sink_input_request_token_t token = {0};
    derived_inventory_state_t derived_state;
    sink_input_request_tracker_init(&tracker);
    derived_inventory_state_init(&derived_state);

    sink_input_request_tracker_init(NULL);
    assert(sink_input_request_tracker_begin(
               NULL, 1, SINK_INPUT_REQUEST_NEW, &token) == -1);
    assert(sink_input_request_tracker_begin(
               &tracker, 1, SINK_INPUT_REQUEST_NEW, NULL) == -1);
    sink_input_request_tracker_invalidate(NULL, 1);
    assert(!sink_input_request_tracker_is_current(NULL, &token));
    assert(!sink_input_request_tracker_is_current(&tracker, NULL));
    sink_input_request_tracker_finish(NULL, &token);
    sink_input_request_tracker_finish(&tracker, NULL);
    sink_input_request_tracker_clear(NULL);

    derived_inventory_state_init(NULL);
    derived_inventory_state_mark_initial_snapshot_complete(NULL);
    derived_inventory_state_set_rebuild_result(NULL, 1);
    assert(!derived_inventory_state_can_rebuild(NULL));
    assert(!derived_inventory_state_is_available(NULL));

    assert_tracker_is_cleared(&tracker);
    assert(!derived_inventory_state_can_rebuild(&derived_state));
    assert(!derived_inventory_state_is_available(&derived_state));
    sink_input_request_tracker_clear(&tracker);
}

static void test_request_result_is_accepted(void) {
    sink_input_request_tracker_t tracker;
    sink_input_request_token_t request;
    sink_input_request_tracker_init(&tracker);

    assert(sink_input_request_tracker_begin(
               &tracker, 10, SINK_INPUT_REQUEST_NEW, &request) == 0);
    assert(request.index == 10);
    assert(request.intent == SINK_INPUT_REQUEST_NEW);
    assert(sink_input_request_tracker_is_current(&tracker, &request));

    sink_input_request_tracker_finish(&tracker, &request);
    assert(!sink_input_request_tracker_is_current(&tracker, &request));
    sink_input_request_tracker_clear(&tracker);
}

static void test_remove_rejects_late_result(void) {
    sink_input_request_tracker_t tracker;
    sink_input_request_token_t request;
    sink_input_request_tracker_init(&tracker);

    assert(sink_input_request_tracker_begin(
               &tracker, 20, SINK_INPUT_REQUEST_CHANGE, &request) == 0);
    sink_input_request_tracker_invalidate(&tracker, 20);
    assert(!sink_input_request_tracker_is_current(&tracker, &request));

    sink_input_request_tracker_finish(&tracker, &request);
    sink_input_request_tracker_clear(&tracker);
}

static void test_new_after_index_reuse_is_accepted(void) {
    sink_input_request_tracker_t tracker;
    sink_input_request_token_t old_request;
    sink_input_request_token_t new_request;
    sink_input_request_tracker_init(&tracker);

    assert(sink_input_request_tracker_begin(
               &tracker, 30, SINK_INPUT_REQUEST_NEW, &old_request) == 0);
    sink_input_request_tracker_invalidate(&tracker, 30);
    assert(sink_input_request_tracker_begin(
               &tracker, 30, SINK_INPUT_REQUEST_NEW, &new_request) == 0);

    assert(!sink_input_request_tracker_is_current(&tracker, &old_request));
    assert(sink_input_request_tracker_is_current(&tracker, &new_request));
    assert(new_request.generation != old_request.generation);

    sink_input_request_tracker_finish(&tracker, &old_request);
    sink_input_request_tracker_finish(&tracker, &new_request);
    sink_input_request_tracker_clear(&tracker);
}

static void test_pending_new_and_change_preserve_intent(void) {
    sink_input_request_tracker_t tracker;
    sink_input_request_token_t new_request;
    sink_input_request_token_t change_request;
    sink_input_request_tracker_init(&tracker);

    assert(sink_input_request_tracker_begin(
               &tracker, 40, SINK_INPUT_REQUEST_NEW, &new_request) == 0);
    assert(sink_input_request_tracker_begin(
               &tracker, 40, SINK_INPUT_REQUEST_CHANGE, &change_request) == 0);

    assert(sink_input_request_tracker_is_current(&tracker, &new_request));
    assert(sink_input_request_tracker_is_current(&tracker, &change_request));
    assert(new_request.generation == change_request.generation);
    assert(new_request.intent == SINK_INPUT_REQUEST_NEW);
    assert(change_request.intent == SINK_INPUT_REQUEST_CHANGE);

    sink_input_request_tracker_finish(&tracker, &new_request);
    sink_input_request_tracker_finish(&tracker, &change_request);
    sink_input_request_tracker_clear(&tracker);
}

static void test_remove_invalidates_same_index_only(void) {
    sink_input_request_tracker_t tracker;
    sink_input_request_token_t first_new;
    sink_input_request_token_t first_change;
    sink_input_request_token_t second_index;
    sink_input_request_tracker_init(&tracker);

    assert(sink_input_request_tracker_begin(
               &tracker, 50, SINK_INPUT_REQUEST_NEW, &first_new) == 0);
    assert(sink_input_request_tracker_begin(
               &tracker, 50, SINK_INPUT_REQUEST_CHANGE, &first_change) == 0);
    assert(sink_input_request_tracker_begin(
               &tracker, 51, SINK_INPUT_REQUEST_NEW, &second_index) == 0);

    sink_input_request_tracker_invalidate(&tracker, 50);
    assert(!sink_input_request_tracker_is_current(&tracker, &first_new));
    assert(!sink_input_request_tracker_is_current(&tracker, &first_change));
    assert(sink_input_request_tracker_is_current(&tracker, &second_index));

    sink_input_request_tracker_finish(&tracker, &first_new);
    sink_input_request_tracker_finish(&tracker, &first_change);
    sink_input_request_tracker_finish(&tracker, &second_index);
    sink_input_request_tracker_clear(&tracker);
}

static void test_generation_wrap_keeps_old_request_invalid(void) {
    sink_input_request_tracker_t tracker;
    sink_input_request_token_t old_request;
    sink_input_request_token_t new_request;
    sink_input_request_tracker_init(&tracker);

    assert(sink_input_request_tracker_begin(
               &tracker, 60, SINK_INPUT_REQUEST_NEW, &old_request) == 0);
    assert(tracker.index_count == 1);
    tracker.indexes[0].generation = UINT64_MAX;
    old_request.generation = UINT64_MAX;
    assert(sink_input_request_tracker_is_current(&tracker, &old_request));

    sink_input_request_tracker_invalidate(&tracker, 60);
    assert(tracker.indexes[0].generation == 0);
    assert(sink_input_request_tracker_begin(
               &tracker, 60, SINK_INPUT_REQUEST_NEW, &new_request) == 0);
    assert(new_request.generation == 0);
    assert(!sink_input_request_tracker_is_current(&tracker, &old_request));
    assert(sink_input_request_tracker_is_current(&tracker, &new_request));

    sink_input_request_tracker_finish(&tracker, &old_request);
    sink_input_request_tracker_finish(&tracker, &new_request);
    sink_input_request_tracker_clear(&tracker);
}

static void test_clear_with_live_tokens_and_reuse(void) {
    sink_input_request_tracker_t tracker;
    sink_input_request_token_t first;
    sink_input_request_token_t second;
    sink_input_request_token_t third;
    sink_input_request_token_t reused;
    sink_input_request_tracker_init(&tracker);

    assert(sink_input_request_tracker_begin(
               &tracker, 70, SINK_INPUT_REQUEST_NEW, &first) == 0);
    assert(sink_input_request_tracker_begin(
               &tracker, 70, SINK_INPUT_REQUEST_CHANGE, &second) == 0);
    assert(sink_input_request_tracker_begin(
               &tracker, 71, SINK_INPUT_REQUEST_NEW, &third) == 0);

    sink_input_request_tracker_clear(&tracker);
    assert_tracker_is_cleared(&tracker);
    assert(first.invalidated);
    assert(second.invalidated);
    assert(third.invalidated);
    assert(first.next == NULL);
    assert(second.next == NULL);
    assert(third.next == NULL);
    assert(!sink_input_request_tracker_is_current(&tracker, &first));
    assert(!sink_input_request_tracker_is_current(&tracker, &second));
    assert(!sink_input_request_tracker_is_current(&tracker, &third));

    sink_input_request_tracker_clear(&tracker);
    assert_tracker_is_cleared(&tracker);

    assert(sink_input_request_tracker_begin(
               &tracker, 72, SINK_INPUT_REQUEST_NEW, &reused) == 0);
    assert(sink_input_request_tracker_is_current(&tracker, &reused));
    sink_input_request_tracker_finish(&tracker, &reused);
    sink_input_request_tracker_clear(&tracker);
    assert_tracker_is_cleared(&tracker);
}

static void test_index_growth_and_nontrivial_finish_order(void) {
    enum { REQUEST_COUNT = 10 };
    sink_input_request_tracker_t tracker;
    sink_input_request_token_t requests[REQUEST_COUNT];
    int finished[REQUEST_COUNT] = {0};
    const size_t finish_order[REQUEST_COUNT] = {
        4, 0, 8, 2, 9, 1, 6, 3, 7, 5
    };
    sink_input_request_tracker_init(&tracker);

    for (size_t i = 0; i < REQUEST_COUNT; i++) {
        assert(sink_input_request_tracker_begin(
                   &tracker,
                   (uint32_t)(100 + i),
                   i % 2 == 0
                       ? SINK_INPUT_REQUEST_NEW
                       : SINK_INPUT_REQUEST_CHANGE,
                   &requests[i]) == 0);

        for (size_t j = 0; j <= i; j++) {
            assert(requests[j].index == (uint32_t)(100 + j));
            assert(sink_input_request_tracker_is_current(
                &tracker,
                &requests[j]));
        }
    }

    assert(tracker.index_count == REQUEST_COUNT);
    assert(tracker.index_capacity >= REQUEST_COUNT);

    for (size_t i = 0; i < REQUEST_COUNT; i++) {
        size_t finished_index = finish_order[i];
        sink_input_request_tracker_finish(
            &tracker,
            &requests[finished_index]);
        finished[finished_index] = 1;
        assert(!sink_input_request_tracker_is_current(
            &tracker,
            &requests[finished_index]));
        assert(tracker.index_count == REQUEST_COUNT - i - 1);

        for (size_t j = 0; j < REQUEST_COUNT; j++) {
            if (!finished[j]) {
                assert(sink_input_request_tracker_is_current(
                    &tracker,
                    &requests[j]));
            }
        }
    }

    sink_input_request_tracker_clear(&tracker);
    assert_tracker_is_cleared(&tracker);
}

static void test_repeated_invalidation_and_finish(void) {
    sink_input_request_tracker_t tracker;
    sink_input_request_token_t first;
    sink_input_request_token_t second;
    sink_input_request_token_t other_index;
    sink_input_request_token_t later;
    sink_input_request_token_t unknown = {0};
    sink_input_request_token_t detached;
    sink_input_request_tracker_init(&tracker);

    assert(sink_input_request_tracker_begin(
               &tracker, 200, SINK_INPUT_REQUEST_NEW, &first) == 0);
    assert(sink_input_request_tracker_begin(
               &tracker, 200, SINK_INPUT_REQUEST_CHANGE, &second) == 0);
    assert(sink_input_request_tracker_begin(
               &tracker, 201, SINK_INPUT_REQUEST_NEW, &other_index) == 0);

    sink_input_request_tracker_invalidate(&tracker, 200);
    sink_input_request_tracker_invalidate(&tracker, 200);
    assert(!sink_input_request_tracker_is_current(&tracker, &first));
    assert(!sink_input_request_tracker_is_current(&tracker, &second));
    assert(sink_input_request_tracker_is_current(&tracker, &other_index));

    assert(sink_input_request_tracker_begin(
               &tracker, 200, SINK_INPUT_REQUEST_NEW, &later) == 0);
    assert(later.generation == first.generation + 2);
    assert(sink_input_request_tracker_is_current(&tracker, &later));

    sink_input_request_tracker_finish(&tracker, &second);
    sink_input_request_tracker_finish(&tracker, &second);
    sink_input_request_tracker_finish(&tracker, &unknown);
    assert(sink_input_request_tracker_is_current(&tracker, &later));
    assert(sink_input_request_tracker_is_current(&tracker, &other_index));

    sink_input_request_tracker_finish(&tracker, &first);
    sink_input_request_tracker_finish(&tracker, &later);
    sink_input_request_tracker_finish(&tracker, &other_index);

    assert(sink_input_request_tracker_begin(
               &tracker, 202, SINK_INPUT_REQUEST_NEW, &detached) == 0);
    sink_input_request_tracker_clear(&tracker);
    sink_input_request_tracker_finish(&tracker, &detached);
    sink_input_request_tracker_finish(&tracker, &detached);
    assert_tracker_is_cleared(&tracker);
}

static void test_repeated_tracker_lifecycle_cycles(void) {
    sink_input_request_tracker_t tracker;
    sink_input_request_tracker_init(&tracker);

    for (uint32_t cycle = 0; cycle < 4; cycle++) {
        sink_input_request_token_t token;
        assert(sink_input_request_tracker_begin(
                   &tracker,
                   300 + cycle,
                   SINK_INPUT_REQUEST_NEW,
                   &token) == 0);
        assert(sink_input_request_tracker_is_current(&tracker, &token));
        sink_input_request_tracker_finish(&tracker, &token);
        sink_input_request_tracker_clear(&tracker);
        assert_tracker_is_cleared(&tracker);
    }
}

static void test_derived_inventory_failure_and_recovery(void) {
    derived_inventory_state_t state;
    derived_inventory_state_init(&state);

    assert(!derived_inventory_state_can_rebuild(&state));
    assert(!derived_inventory_state_is_available(&state));

    derived_inventory_state_mark_initial_snapshot_complete(&state);
    assert(derived_inventory_state_can_rebuild(&state));
    assert(!derived_inventory_state_is_available(&state));

    derived_inventory_state_set_rebuild_result(&state, 1);
    assert(derived_inventory_state_is_available(&state));

    derived_inventory_state_set_rebuild_result(&state, 0);
    assert(derived_inventory_state_can_rebuild(&state));
    assert(!derived_inventory_state_is_available(&state));

    derived_inventory_state_set_rebuild_result(&state, 1);
    assert(derived_inventory_state_can_rebuild(&state));
    assert(derived_inventory_state_is_available(&state));
}

static void test_repeated_derived_inventory_cycles(void) {
    derived_inventory_state_t state;

    for (int cycle = 0; cycle < 4; cycle++) {
        derived_inventory_state_init(&state);
        assert(!derived_inventory_state_can_rebuild(&state));
        assert(!derived_inventory_state_is_available(&state));

        derived_inventory_state_set_rebuild_result(&state, 1);
        assert(!derived_inventory_state_can_rebuild(&state));
        assert(!derived_inventory_state_is_available(&state));

        derived_inventory_state_mark_initial_snapshot_complete(&state);
        assert(derived_inventory_state_can_rebuild(&state));
        assert(!derived_inventory_state_is_available(&state));

        derived_inventory_state_set_rebuild_result(&state, 1);
        assert(derived_inventory_state_is_available(&state));
        derived_inventory_state_set_rebuild_result(&state, 0);
        assert(!derived_inventory_state_is_available(&state));
        derived_inventory_state_set_rebuild_result(&state, 1);
        assert(derived_inventory_state_is_available(&state));

        derived_inventory_state_init(&state);
        assert(!derived_inventory_state_can_rebuild(&state));
        assert(!derived_inventory_state_is_available(&state));
    }
}

int main(void) {
    test_supported_null_arguments();
    test_request_result_is_accepted();
    test_remove_rejects_late_result();
    test_new_after_index_reuse_is_accepted();
    test_pending_new_and_change_preserve_intent();
    test_remove_invalidates_same_index_only();
    test_generation_wrap_keeps_old_request_invalid();
    test_clear_with_live_tokens_and_reuse();
    test_index_growth_and_nontrivial_finish_order();
    test_repeated_invalidation_and_finish();
    test_repeated_tracker_lifecycle_cycles();
    test_derived_inventory_failure_and_recovery();
    test_repeated_derived_inventory_cycles();

    printf("sink_input_request_state tests passed\n");
    return 0;
}
