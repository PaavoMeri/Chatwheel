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
    assert(sink_input_request_tracker_next_index_capacity(0, NULL) == -1);
    assert(sink_input_request_tracker_begin(
               NULL, 1, SINK_INPUT_REQUEST_NEW, &token) == -1);
    assert(sink_input_request_tracker_begin(
               &tracker, 1, SINK_INPUT_REQUEST_NEW, NULL) == -1);
    sink_input_request_tracker_invalidate(NULL, 1);
    assert(!sink_input_request_tracker_is_current(NULL, &token));
    assert(!sink_input_request_tracker_is_current(&tracker, NULL));
    assert(sink_input_request_tracker_observe_inventory_result(
               NULL, &token, 0) == -1);
    assert(sink_input_request_tracker_observe_inventory_result(
               &tracker, NULL, 0) == -1);
    assert(!sink_input_request_tracker_is_initial_route_pending(
        NULL,
        &token));
    assert(!sink_input_request_tracker_is_initial_route_pending(
        &tracker,
        NULL));
    assert(!sink_input_request_tracker_record_volume_submission(
        NULL,
        1,
        1));
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

static void test_unknown_new_submission_completes_pending(void) {
    sink_input_request_tracker_t tracker;
    sink_input_request_token_t request;
    sink_input_request_tracker_init(&tracker);

    assert(sink_input_request_tracker_begin(
               &tracker, 11, SINK_INPUT_REQUEST_NEW, &request) == 0);
    assert(sink_input_request_tracker_observe_inventory_result(
               &tracker, &request, 0) == 0);
    assert(sink_input_request_tracker_is_initial_route_pending(
        &tracker,
        &request));
    assert(sink_input_request_tracker_record_volume_submission(
        &tracker,
        11,
        1));
    assert(!sink_input_request_tracker_is_initial_route_pending(
        &tracker,
        &request));

    sink_input_request_tracker_finish(&tracker, &request);
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

static void test_change_before_new_preserves_initial_route(void) {
    sink_input_request_tracker_t tracker;
    sink_input_request_token_t new_request;
    sink_input_request_token_t change_request;
    sink_input_request_tracker_init(&tracker);

    assert(sink_input_request_tracker_begin(
               &tracker, 41, SINK_INPUT_REQUEST_NEW, &new_request) == 0);
    assert(sink_input_request_tracker_begin(
               &tracker, 41, SINK_INPUT_REQUEST_CHANGE, &change_request) == 0);

    assert(sink_input_request_tracker_observe_inventory_result(
               &tracker, &change_request, 0) == 0);
    assert(sink_input_request_tracker_is_initial_route_pending(
        &tracker,
        &change_request));
    assert(sink_input_request_tracker_is_initial_route_pending(
        &tracker,
        &new_request));
    assert(sink_input_request_tracker_observe_inventory_result(
               &tracker, &new_request, 1) == 0);
    assert(sink_input_request_tracker_is_initial_route_pending(
        &tracker,
        &new_request));

    assert(sink_input_request_tracker_record_volume_submission(
        &tracker,
        41,
        1));
    assert(!sink_input_request_tracker_is_initial_route_pending(
        &tracker,
        &new_request));
    assert(!sink_input_request_tracker_record_volume_submission(
        &tracker,
        41,
        1));

    sink_input_request_tracker_finish(&tracker, &change_request);
    sink_input_request_tracker_finish(&tracker, &new_request);
    sink_input_request_tracker_clear(&tracker);
}

static void test_new_before_change_routes_only_once(void) {
    sink_input_request_tracker_t tracker;
    sink_input_request_token_t new_request;
    sink_input_request_token_t change_request;
    sink_input_request_tracker_init(&tracker);

    assert(sink_input_request_tracker_begin(
               &tracker, 48, SINK_INPUT_REQUEST_NEW, &new_request) == 0);
    assert(sink_input_request_tracker_begin(
               &tracker, 48, SINK_INPUT_REQUEST_CHANGE, &change_request) == 0);
    assert(sink_input_request_tracker_observe_inventory_result(
               &tracker, &new_request, 0) == 0);
    assert(sink_input_request_tracker_record_volume_submission(
        &tracker,
        48,
        1));

    assert(sink_input_request_tracker_observe_inventory_result(
               &tracker, &change_request, 1) == 0);
    assert(!sink_input_request_tracker_is_initial_route_pending(
        &tracker,
        &change_request));
    assert(!sink_input_request_tracker_record_volume_submission(
        &tracker,
        48,
        1));

    sink_input_request_tracker_finish(&tracker, &new_request);
    sink_input_request_tracker_finish(&tracker, &change_request);
    sink_input_request_tracker_clear(&tracker);
}

static void test_duplicate_new_routes_only_once(void) {
    sink_input_request_tracker_t tracker;
    sink_input_request_token_t first_new;
    sink_input_request_token_t duplicate_new;
    sink_input_request_tracker_init(&tracker);

    assert(sink_input_request_tracker_begin(
               &tracker, 49, SINK_INPUT_REQUEST_NEW, &first_new) == 0);
    assert(sink_input_request_tracker_begin(
               &tracker, 49, SINK_INPUT_REQUEST_NEW, &duplicate_new) == 0);
    assert(sink_input_request_tracker_observe_inventory_result(
               &tracker, &first_new, 0) == 0);
    assert(sink_input_request_tracker_record_volume_submission(
        &tracker,
        49,
        1));

    assert(sink_input_request_tracker_observe_inventory_result(
               &tracker, &duplicate_new, 1) == 0);
    assert(!sink_input_request_tracker_is_initial_route_pending(
        &tracker,
        &duplicate_new));
    assert(!sink_input_request_tracker_record_volume_submission(
        &tracker,
        49,
        1));

    sink_input_request_tracker_finish(&tracker, &first_new);
    sink_input_request_tracker_finish(&tracker, &duplicate_new);
    sink_input_request_tracker_clear(&tracker);
}

static void test_change_only_unknown_stream_can_complete_initial_route(void) {
    sink_input_request_tracker_t tracker;
    sink_input_request_token_t change_request;
    sink_input_request_tracker_init(&tracker);

    assert(sink_input_request_tracker_begin(
               &tracker, 42, SINK_INPUT_REQUEST_CHANGE, &change_request) == 0);
    assert(sink_input_request_tracker_observe_inventory_result(
               &tracker, &change_request, 0) == 0);
    assert(sink_input_request_tracker_is_initial_route_pending(
        &tracker,
        &change_request));
    assert(sink_input_request_tracker_record_volume_submission(
        &tracker,
        42,
        1));
    assert(!sink_input_request_tracker_is_initial_route_pending(
        &tracker,
        &change_request));

    sink_input_request_tracker_finish(&tracker, &change_request);
    sink_input_request_tracker_clear(&tracker);
}

static void test_pending_survives_failed_or_missing_submission(void) {
    sink_input_request_tracker_t tracker;
    sink_input_request_token_t first_result;
    sink_input_request_token_t later_result;
    sink_input_request_tracker_init(&tracker);

    assert(sink_input_request_tracker_begin(
               &tracker, 43, SINK_INPUT_REQUEST_NEW, &first_result) == 0);
    assert(sink_input_request_tracker_observe_inventory_result(
               &tracker, &first_result, 0) == 0);

    /* Upsert/rebuild failure or an empty plan records no submission. */
    assert(sink_input_request_tracker_is_initial_route_pending(
        &tracker,
        &first_result));
    assert(!sink_input_request_tracker_record_volume_submission(
        &tracker,
        43,
        0));
    assert(sink_input_request_tracker_is_initial_route_pending(
        &tracker,
        &first_result));

    sink_input_request_tracker_finish(&tracker, &first_result);
    assert(tracker.index_count == 1);
    assert(sink_input_request_tracker_begin(
               &tracker, 43, SINK_INPUT_REQUEST_CHANGE, &later_result) == 0);
    assert(sink_input_request_tracker_observe_inventory_result(
               &tracker, &later_result, 1) == 0);
    assert(sink_input_request_tracker_is_initial_route_pending(
        &tracker,
        &later_result));

    /* A NULL pa_operation result also leaves pending set. */
    assert(!sink_input_request_tracker_record_volume_submission(
        &tracker,
        43,
        0));
    assert(sink_input_request_tracker_is_initial_route_pending(
        &tracker,
        &later_result));
    assert(sink_input_request_tracker_record_volume_submission(
        &tracker,
        43,
        1));
    assert(!sink_input_request_tracker_is_initial_route_pending(
        &tracker,
        &later_result));

    sink_input_request_tracker_finish(&tracker, &later_result);
    assert(tracker.index_count == 0);
    sink_input_request_tracker_clear(&tracker);
}

static void test_other_stream_submission_does_not_complete_trigger(void) {
    sink_input_request_tracker_t tracker;
    sink_input_request_token_t trigger;
    sink_input_request_token_t same_application_stream;
    sink_input_request_tracker_init(&tracker);

    assert(sink_input_request_tracker_begin(
               &tracker, 44, SINK_INPUT_REQUEST_NEW, &trigger) == 0);
    assert(sink_input_request_tracker_begin(
               &tracker, 45, SINK_INPUT_REQUEST_NEW,
               &same_application_stream) == 0);
    assert(sink_input_request_tracker_observe_inventory_result(
               &tracker, &trigger, 0) == 0);
    assert(sink_input_request_tracker_observe_inventory_result(
               &tracker, &same_application_stream, 0) == 0);

    assert(sink_input_request_tracker_record_volume_submission(
        &tracker,
        45,
        1));
    assert(sink_input_request_tracker_is_initial_route_pending(
        &tracker,
        &trigger));
    assert(!sink_input_request_tracker_is_initial_route_pending(
        &tracker,
        &same_application_stream));

    sink_input_request_tracker_finish(&tracker, &same_application_stream);
    assert(sink_input_request_tracker_record_volume_submission(
        &tracker,
        44,
        1));
    sink_input_request_tracker_finish(&tracker, &trigger);
    sink_input_request_tracker_clear(&tracker);
}

static void test_snapshot_known_events_do_not_start_pending(void) {
    sink_input_request_tracker_t tracker;
    sink_input_request_token_t duplicate_new;
    sink_input_request_token_t property_change;
    sink_input_request_tracker_init(&tracker);

    assert(sink_input_request_tracker_begin(
               &tracker, 46, SINK_INPUT_REQUEST_NEW, &duplicate_new) == 0);
    assert(sink_input_request_tracker_observe_inventory_result(
               &tracker, &duplicate_new, 1) == 0);
    assert(!sink_input_request_tracker_is_initial_route_pending(
        &tracker,
        &duplicate_new));
    assert(sink_input_request_tracker_begin(
               &tracker, 46, SINK_INPUT_REQUEST_CHANGE, &property_change) == 0);
    assert(sink_input_request_tracker_observe_inventory_result(
               &tracker, &property_change, 1) == 0);
    assert(!sink_input_request_tracker_is_initial_route_pending(
        &tracker,
        &property_change));

    sink_input_request_tracker_finish(&tracker, &duplicate_new);
    sink_input_request_tracker_finish(&tracker, &property_change);
    sink_input_request_tracker_clear(&tracker);
}

static void test_init_replay_completes_pending_without_duplicate(void) {
    sink_input_request_tracker_t tracker;
    sink_input_request_token_t subscription_result;
    sink_input_request_token_t later_event;
    sink_input_request_tracker_init(&tracker);

    assert(sink_input_request_tracker_begin(
               &tracker, 52, SINK_INPUT_REQUEST_NEW,
               &subscription_result) == 0);
    assert(sink_input_request_tracker_observe_inventory_result(
               &tracker, &subscription_result, 0) == 0);
    sink_input_request_tracker_finish(&tracker, &subscription_result);
    assert(tracker.index_count == 1);

    /* Successful all-stream init replay records the submitted raw index. */
    assert(sink_input_request_tracker_record_volume_submission(
        &tracker,
        52,
        1));
    assert(tracker.index_count == 0);

    assert(sink_input_request_tracker_begin(
               &tracker, 52, SINK_INPUT_REQUEST_CHANGE, &later_event) == 0);
    assert(sink_input_request_tracker_observe_inventory_result(
               &tracker, &later_event, 1) == 0);
    assert(!sink_input_request_tracker_is_initial_route_pending(
        &tracker,
        &later_event));

    sink_input_request_tracker_finish(&tracker, &later_event);
    sink_input_request_tracker_clear(&tracker);
}

static void test_remove_clears_pending_and_stale_request_cannot_restore_it(void) {
    sink_input_request_tracker_t tracker;
    sink_input_request_token_t old_request;
    sink_input_request_token_t reused_index;
    sink_input_request_tracker_init(&tracker);

    assert(sink_input_request_tracker_begin(
               &tracker, 47, SINK_INPUT_REQUEST_NEW, &old_request) == 0);
    assert(sink_input_request_tracker_observe_inventory_result(
               &tracker, &old_request, 0) == 0);
    sink_input_request_tracker_invalidate(&tracker, 47);
    assert(!sink_input_request_tracker_is_initial_route_pending(
        &tracker,
        &old_request));
    assert(sink_input_request_tracker_observe_inventory_result(
               &tracker, &old_request, 0) == -1);

    assert(sink_input_request_tracker_begin(
               &tracker, 47, SINK_INPUT_REQUEST_NEW, &reused_index) == 0);
    assert(!sink_input_request_tracker_is_initial_route_pending(
        &tracker,
        &reused_index));
    assert(sink_input_request_tracker_observe_inventory_result(
               &tracker, &reused_index, 0) == 0);
    assert(sink_input_request_tracker_is_initial_route_pending(
        &tracker,
        &reused_index));

    sink_input_request_tracker_finish(&tracker, &old_request);
    sink_input_request_tracker_finish(&tracker, &reused_index);
    sink_input_request_tracker_invalidate(&tracker, 47);
    assert(tracker.index_count == 0);
    sink_input_request_tracker_clear(&tracker);
}

static void test_cleanup_releases_pending_indexes(void) {
    sink_input_request_tracker_t tracker;
    sink_input_request_token_t requests[10];
    sink_input_request_tracker_init(&tracker);

    for (size_t i = 0; i < 10; i++) {
        assert(sink_input_request_tracker_begin(
                   &tracker,
                   (uint32_t)(80 + i),
                   SINK_INPUT_REQUEST_NEW,
                   &requests[i]) == 0);
        assert(sink_input_request_tracker_observe_inventory_result(
                   &tracker, &requests[i], 0) == 0);
        sink_input_request_tracker_finish(&tracker, &requests[i]);
    }

    assert(tracker.index_count == 10);
    assert(tracker.index_capacity >= 10);
    sink_input_request_tracker_clear(&tracker);
    assert_tracker_is_cleared(&tracker);
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

static void test_capacity_growth_and_overflow_boundaries(void) {
    size_t next_capacity = 0;

    assert(sink_input_request_tracker_next_index_capacity(
               0, &next_capacity) == 0);
    assert(next_capacity == 4);
    assert(sink_input_request_tracker_next_index_capacity(
               next_capacity, &next_capacity) == 0);
    assert(next_capacity == 8);
    assert(sink_input_request_tracker_next_index_capacity(
               SIZE_MAX, &next_capacity) == -1);
    assert(sink_input_request_tracker_next_index_capacity(
               SIZE_MAX / 2 + 1, &next_capacity) == -1);
    size_t byte_overflow_capacity =
        SIZE_MAX / sizeof(sink_input_index_generation_t) / 2 + 1;
    assert(sink_input_request_tracker_next_index_capacity(
               byte_overflow_capacity, &next_capacity) == -1);
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
    test_unknown_new_submission_completes_pending();
    test_remove_rejects_late_result();
    test_new_after_index_reuse_is_accepted();
    test_pending_new_and_change_preserve_intent();
    test_change_before_new_preserves_initial_route();
    test_new_before_change_routes_only_once();
    test_duplicate_new_routes_only_once();
    test_change_only_unknown_stream_can_complete_initial_route();
    test_pending_survives_failed_or_missing_submission();
    test_other_stream_submission_does_not_complete_trigger();
    test_snapshot_known_events_do_not_start_pending();
    test_init_replay_completes_pending_without_duplicate();
    test_remove_clears_pending_and_stale_request_cannot_restore_it();
    test_cleanup_releases_pending_indexes();
    test_remove_invalidates_same_index_only();
    test_generation_wrap_keeps_old_request_invalid();
    test_clear_with_live_tokens_and_reuse();
    test_index_growth_and_nontrivial_finish_order();
    test_capacity_growth_and_overflow_boundaries();
    test_repeated_invalidation_and_finish();
    test_repeated_tracker_lifecycle_cycles();
    test_derived_inventory_failure_and_recovery();
    test_repeated_derived_inventory_cycles();

    printf("sink_input_request_state tests passed\n");
    return 0;
}
