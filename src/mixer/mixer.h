#ifndef MIXER_H
#define MIXER_H

#include <stddef.h>
#include <stdint.h>
#include <signal.h>
#include <pulse/pulseaudio.h> // Include PulseAudio or PipeWire headers as needed
#include "../application_classifier.h"
#include "../application_identity.h"

typedef struct {
    uint32_t index;
    const char *application_id;
    const char *application_name;
    const char *process_binary;
    const char *node_name;
} audio_stream_view_t;

typedef struct {
    application_identity_property_t identity_property;
    const char *identity_value;
    const char *display_name;
    const uint32_t *stream_indexes;
    size_t stream_count;
    application_group_t group;
    int matched_config_index;
} active_application_view_t;

typedef enum {
    AUDIO_INIT_OK = 0,
    AUDIO_INIT_FAILED,
    AUDIO_INIT_TIMEOUT,
    AUDIO_INIT_SHUTDOWN
} audio_init_result_t;

typedef struct {
    const volatile sig_atomic_t *running;
} audio_init_options_t;

typedef enum {
    AUDIO_EVENTS_OK,
    AUDIO_EVENTS_CONNECTION_LOST,
    AUDIO_EVENTS_MAINLOOP_ERROR
} audio_event_status_t;

typedef enum {
    AUDIO_INIT_WAIT_PENDING,
    AUDIO_INIT_WAIT_SUCCESS,
    AUDIO_INIT_WAIT_FAILURE,
    AUDIO_INIT_WAIT_TIMEOUT,
    AUDIO_INIT_WAIT_SHUTDOWN
} audio_init_wait_result_t;

/*
 * Shared by production waits and unit tests. Decision priority is shutdown,
 * inclusive deadline, failure, completion, and finally continued waiting.
 */
static inline audio_init_wait_result_t audio_init_wait_decide(
    int completed,
    int failed,
    int shutdown_requested,
    uint64_t now_ns,
    uint64_t deadline_ns) {
    if (shutdown_requested) return AUDIO_INIT_WAIT_SHUTDOWN;
    if (now_ns >= deadline_ns) return AUDIO_INIT_WAIT_TIMEOUT;
    if (failed) return AUDIO_INIT_WAIT_FAILURE;
    if (completed) return AUDIO_INIT_WAIT_SUCCESS;
    return AUDIO_INIT_WAIT_PENDING;
}

/* Connection loss takes precedence when the same dispatch also fails. */
static inline audio_event_status_t audio_event_status_from_observation(
    pa_context_state_t context_state,
    int mainloop_failed) {
    if (context_state == PA_CONTEXT_FAILED ||
        context_state == PA_CONTEXT_TERMINATED) {
        return AUDIO_EVENTS_CONNECTION_LOST;
    }
    return mainloop_failed
        ? AUDIO_EVENTS_MAINLOOP_ERROR
        : AUDIO_EVENTS_OK;
}

// Initialize and cleanup
/*
 * Performs one bounded connect+subscribe+snapshot attempt. The complete
 * attempt has one five-second monotonic deadline. When options->running is
 * non-NULL, a zero value interrupts the attempt promptly. On every non-OK
 * result all partially initialized PulseAudio resources are cleaned up.
 */
audio_init_result_t initialize_audio_server(
    const audio_init_options_t *options);
void cleanup_audio_server(void);

/*
 * Pumps currently pending events without blocking. This function never frees
 * the context or mainloop; the caller must run cleanup_audio_server() after a
 * CONNECTION_LOST or MAINLOOP_ERROR result has returned.
 */
audio_event_status_t process_audio_events(void);

size_t get_active_audio_stream_count(void);

/*
 * Copies one stream's read-only view to stream. The view struct is owned by
 * the caller, but its string pointers are borrowed from the audio server's
 * inventory. They must not be modified or freed and may be invalidated by any
 * stream lifecycle change or cleanup_audio_server(). Returns 0 on success and
 * -1 when stream is NULL or position is out of bounds.
 */
int get_active_audio_stream(size_t position, audio_stream_view_t *stream);

/*
 * Returns the number of active applications while the derived inventory is
 * synchronized with the raw stream inventory. Returns 0 before initial
 * synchronization and after a rebuild failure, until a later rebuild succeeds.
 */
size_t get_active_application_count(void);

/*
 * Copies one application's read-only view to view. The view struct is owned by
 * the caller, but all pointer fields are borrowed from the private application
 * inventory. The caller must not modify or free borrowed data. Any successful
 * application-inventory rebuild or cleanup_audio_server() invalidates it, so
 * the view must not be retained while audio events are processed. Classification
 * fields are copied by value and describe the configuration loaded when this
 * function is called. Returns 0 on success and -1 when view is NULL, position
 * is out of bounds, or the derived inventory is not currently synchronized.
 */
int get_active_application(size_t position, active_application_view_t *view);

// Volume control functions
void adjust_volume_based_on_chatmix(float chatmix_value);

// Application listing
void list_applications(void);
void list_unconfigured_applications(void);

#endif // MIXER_H
