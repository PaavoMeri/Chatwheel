#ifndef SINK_INPUT_REQUEST_STATE_H
#define SINK_INPUT_REQUEST_STATE_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    SINK_INPUT_REQUEST_NEW,
    SINK_INPUT_REQUEST_CHANGE
} sink_input_request_intent_t;

typedef struct sink_input_request_token {
    uint32_t index;
    uint64_t generation;
    sink_input_request_intent_t intent;
    int invalidated;
    struct sink_input_request_token *next;
} sink_input_request_token_t;

typedef struct {
    uint32_t index;
    uint64_t generation;
} sink_input_index_generation_t;

/*
 * Tracks caller-owned request tokens. Tokens must remain alive while they are
 * registered, until finish() or clear() detaches them. Index-generation storage
 * is owned by the tracker and released by clear().
 *
 * A tracker must be initialized before use. Its fields are implementation
 * state; manually constructed or otherwise malformed states are unsupported.
 */
typedef struct {
    sink_input_index_generation_t *indexes;
    size_t index_count;
    size_t index_capacity;
    sink_input_request_token_t *requests;
} sink_input_request_tracker_t;

/*
 * Initializes a new tracker or one already reset by clear(). Calling init() on
 * a tracker that still owns storage is invalid and would leak that storage.
 */
void sink_input_request_tracker_init(sink_input_request_tracker_t *tracker);

/*
 * Registers token for the index's current generation. The caller must keep
 * token alive until sink_input_request_tracker_finish() or clear() is called.
 * Returns 0 on success and -1 for invalid arguments or allocation failure.
 */
int sink_input_request_tracker_begin(
    sink_input_request_tracker_t *tracker,
    uint32_t index,
    sink_input_request_intent_t intent,
    sink_input_request_token_t *token);

/*
 * Invalidates every registered request for index and advances its generation.
 * Repeated invalidation of the same index is safe.
 */
void sink_input_request_tracker_invalidate(
    sink_input_request_tracker_t *tracker,
    uint32_t index);

/* Returns nonzero only while token is registered and current for its index. */
int sink_input_request_tracker_is_current(
    const sink_input_request_tracker_t *tracker,
    const sink_input_request_token_t *token);

/*
 * Unregisters token. Repeated calls, a token detached by clear(), and an
 * unknown token are safe no-ops.
 */
void sink_input_request_tracker_finish(
    sink_input_request_tracker_t *tracker,
    sink_input_request_token_t *token);

/*
 * Releases tracker-owned storage, invalidates and detaches all registered
 * caller-owned tokens, and leaves the tracker immediately reusable. Repeated
 * calls are safe.
 */
void sink_input_request_tracker_clear(sink_input_request_tracker_t *tracker);

typedef struct {
    int initial_snapshot_complete;
    int inventory_synchronized;
} derived_inventory_state_t;

void derived_inventory_state_init(derived_inventory_state_t *state);
void derived_inventory_state_mark_initial_snapshot_complete(
    derived_inventory_state_t *state);
void derived_inventory_state_set_rebuild_result(
    derived_inventory_state_t *state,
    int succeeded);
int derived_inventory_state_can_rebuild(
    const derived_inventory_state_t *state);
int derived_inventory_state_is_available(
    const derived_inventory_state_t *state);

#endif
