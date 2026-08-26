#include "sink_input_request_state.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_INDEX_CAPACITY 4

static sink_input_index_generation_t *find_index(
    const sink_input_request_tracker_t *tracker,
    uint32_t index) {
    for (size_t i = 0; i < tracker->index_count; i++) {
        if (tracker->indexes[i].index == index) return &tracker->indexes[i];
    }

    return NULL;
}

int sink_input_request_tracker_next_index_capacity(
    size_t current_capacity,
    size_t *next_capacity) {
    if (!next_capacity) return -1;

    size_t new_capacity = INITIAL_INDEX_CAPACITY;
    if (current_capacity > 0) {
        if (current_capacity > SIZE_MAX / 2) return -1;
        new_capacity = current_capacity * 2;
    }
    if (new_capacity > SIZE_MAX / sizeof(sink_input_index_generation_t)) {
        return -1;
    }

    *next_capacity = new_capacity;
    return 0;
}

static int ensure_index_capacity(sink_input_request_tracker_t *tracker) {
    if (tracker->index_count < tracker->index_capacity) return 0;

    size_t new_capacity;
    if (sink_input_request_tracker_next_index_capacity(
            tracker->index_capacity,
            &new_capacity) != 0) {
        return -1;
    }

    sink_input_index_generation_t *resized = realloc(
        tracker->indexes,
        new_capacity * sizeof(*tracker->indexes));
    if (!resized) return -1;

    tracker->indexes = resized;
    tracker->index_capacity = new_capacity;
    return 0;
}

static int token_is_registered(
    const sink_input_request_tracker_t *tracker,
    const sink_input_request_token_t *token) {
    for (const sink_input_request_token_t *current = tracker->requests;
         current;
         current = current->next) {
        if (current == token) return 1;
    }

    return 0;
}

static int index_has_requests(
    const sink_input_request_tracker_t *tracker,
    uint32_t index) {
    for (const sink_input_request_token_t *token = tracker->requests;
         token;
         token = token->next) {
        if (token->index == index) return 1;
    }

    return 0;
}

static void remove_unused_index(
    sink_input_request_tracker_t *tracker,
    uint32_t index) {
    if (index_has_requests(tracker, index)) return;

    for (size_t i = 0; i < tracker->index_count; i++) {
        if (tracker->indexes[i].index != index) continue;
        if (tracker->indexes[i].initial_route_pending) return;

        if (i + 1 < tracker->index_count) {
            memmove(
                &tracker->indexes[i],
                &tracker->indexes[i + 1],
                (tracker->index_count - i - 1) * sizeof(*tracker->indexes));
        }
        tracker->index_count--;
        return;
    }
}

void sink_input_request_tracker_init(sink_input_request_tracker_t *tracker) {
    if (!tracker) return;
    *tracker = (sink_input_request_tracker_t){0};
}

int sink_input_request_tracker_begin(
    sink_input_request_tracker_t *tracker,
    uint32_t index,
    sink_input_request_intent_t intent,
    sink_input_request_token_t *token) {
    if (!tracker || !token ||
        (intent != SINK_INPUT_REQUEST_NEW &&
         intent != SINK_INPUT_REQUEST_CHANGE) ||
        token_is_registered(tracker, token)) {
        return -1;
    }

    sink_input_index_generation_t *index_state = find_index(tracker, index);
    if (!index_state) {
        if (ensure_index_capacity(tracker) != 0) return -1;
        index_state = &tracker->indexes[tracker->index_count];
        *index_state = (sink_input_index_generation_t){
            .index = index,
            .generation = 0,
            .initial_route_pending = 0,
        };
        tracker->index_count++;
    }

    *token = (sink_input_request_token_t){
        .index = index,
        .generation = index_state->generation,
        .intent = intent,
        .next = tracker->requests,
    };
    tracker->requests = token;
    return 0;
}

void sink_input_request_tracker_invalidate(
    sink_input_request_tracker_t *tracker,
    uint32_t index) {
    if (!tracker) return;

    for (sink_input_request_token_t *token = tracker->requests;
         token;
         token = token->next) {
        if (token->index == index) token->invalidated = 1;
    }

    sink_input_index_generation_t *index_state = find_index(tracker, index);
    if (index_state) {
        /* Unsigned wrap is safe because all older live tokens are invalidated. */
        index_state->generation++;
        index_state->initial_route_pending = 0;
        remove_unused_index(tracker, index);
    }
}

int sink_input_request_tracker_is_current(
    const sink_input_request_tracker_t *tracker,
    const sink_input_request_token_t *token) {
    if (!tracker || !token || token->invalidated ||
        !token_is_registered(tracker, token)) {
        return 0;
    }

    sink_input_index_generation_t *index_state = find_index(
        tracker,
        token->index);
    return index_state && index_state->generation == token->generation;
}

int sink_input_request_tracker_observe_inventory_result(
    sink_input_request_tracker_t *tracker,
    const sink_input_request_token_t *token,
    int was_known) {
    if (!sink_input_request_tracker_is_current(tracker, token)) return -1;

    sink_input_index_generation_t *index_state = find_index(
        tracker,
        token->index);
    if (!index_state) return -1;

    if (!was_known) index_state->initial_route_pending = 1;
    return 0;
}

int sink_input_request_tracker_is_initial_route_pending(
    const sink_input_request_tracker_t *tracker,
    const sink_input_request_token_t *token) {
    if (!sink_input_request_tracker_is_current(tracker, token)) return 0;

    sink_input_index_generation_t *index_state = find_index(
        tracker,
        token->index);
    return index_state && index_state->initial_route_pending;
}

int sink_input_request_tracker_record_volume_submission(
    sink_input_request_tracker_t *tracker,
    uint32_t index,
    int operation_created) {
    if (!tracker || !operation_created) return 0;

    sink_input_index_generation_t *index_state = find_index(tracker, index);
    if (!index_state || !index_state->initial_route_pending) return 0;

    index_state->initial_route_pending = 0;
    remove_unused_index(tracker, index);
    return 1;
}

void sink_input_request_tracker_finish(
    sink_input_request_tracker_t *tracker,
    sink_input_request_token_t *token) {
    if (!tracker || !token) return;

    sink_input_request_token_t **position = &tracker->requests;
    while (*position && *position != token) {
        position = &(*position)->next;
    }
    if (!*position) return;

    uint32_t index = token->index;
    *position = token->next;
    token->invalidated = 1;
    token->next = NULL;
    remove_unused_index(tracker, index);
}

void sink_input_request_tracker_clear(sink_input_request_tracker_t *tracker) {
    if (!tracker) return;

    sink_input_request_token_t *token = tracker->requests;
    while (token) {
        sink_input_request_token_t *next = token->next;
        token->invalidated = 1;
        token->next = NULL;
        token = next;
    }

    free(tracker->indexes);
    sink_input_request_tracker_init(tracker);
}

void derived_inventory_state_init(derived_inventory_state_t *state) {
    if (!state) return;
    *state = (derived_inventory_state_t){0};
}

void derived_inventory_state_mark_initial_snapshot_complete(
    derived_inventory_state_t *state) {
    if (!state) return;
    state->initial_snapshot_complete = 1;
    state->inventory_synchronized = 0;
}

void derived_inventory_state_set_rebuild_result(
    derived_inventory_state_t *state,
    int succeeded) {
    if (!state || !state->initial_snapshot_complete) return;
    state->inventory_synchronized = succeeded ? 1 : 0;
}

int derived_inventory_state_can_rebuild(
    const derived_inventory_state_t *state) {
    return state && state->initial_snapshot_complete;
}

int derived_inventory_state_is_available(
    const derived_inventory_state_t *state) {
    return state &&
        state->initial_snapshot_complete &&
        state->inventory_synchronized;
}
