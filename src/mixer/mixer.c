#include <pulse/pulseaudio.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mixer.h"
#include "chatmix_volume.h"
#include "classified_volume_routing.h"
#include "pulse_event_drain.h"
#include "pulse_reconnect_state.h"
#include "sink_input_request_state.h"
#include "pulse_stream_lifecycle.h"
#include "../active_application_inventory.h"
#include "../application_classifier.h"
#include "../config.h"
#include "../pattern_matcher.h"

static pa_context *context = NULL;
static pa_mainloop *mainloop = NULL;
static chatmix_volume_targets_t last_chatmix_targets;
static int has_valid_chatmix = 0;
static audio_stream_inventory_t stream_inventory;
static active_application_inventory_t application_inventory;
static sink_input_request_tracker_t sink_input_request_tracker;
static derived_inventory_state_t application_inventory_state;
static pa_context_state_t observed_context_state = PA_CONTEXT_UNCONNECTED;

#define AUDIO_INIT_TIMEOUT_NS \
    (UINT64_C(5) * PULSE_RECONNECT_NANOSECONDS_PER_SECOND)
#define AUDIO_INIT_WAIT_NS UINT64_C(25000000)

struct sink_input_info_request {
    sink_input_request_token_t token;
    pa_operation *operation;
    int result_received;
    struct sink_input_info_request *next;
};

static struct sink_input_info_request *pending_sink_input_requests = NULL;

struct snapshot_state {
    int failed;
};

// Forward declarations for helpers used before their definitions
static int wait_for_operation(pa_operation *op);
static audio_init_result_t wait_for_initial_context(
    uint64_t deadline_ns,
    const audio_init_options_t *options);
static audio_init_result_t wait_for_initialization_operation(
    pa_operation *operation,
    uint64_t deadline_ns,
    const audio_init_options_t *options);
static void reap_sink_input_requests(void);
static void subscribe_callback(pa_context *c, pa_subscription_event_type_t t, uint32_t idx, void *userdata);
static void sink_input_event_info_cb(pa_context *ctx, const pa_sink_input_info *info, int eol, void *ud);
static void sink_input_snapshot_cb(pa_context *ctx, const pa_sink_input_info *info, int eol, void *ud);

static void context_state_callback(pa_context *c, void *userdata) {
    (void)userdata;
    observed_context_state = pa_context_get_state(c);
}

static int record_sink_input(const pa_sink_input_info *info) {
    if (!info) return -1;
    if (!pa_channels_valid(info->sample_spec.channels)) return -1;

    return pulse_stream_lifecycle_record(
        &stream_inventory,
        info->index,
        info->sample_spec.channels,
        info->proplist);
}

static void subscribe_success_callback(pa_context *c, int success, void *userdata) {
    (void)c;
    int *subscription_succeeded = userdata;
    *subscription_succeeded = success;
}

static const char *application_group_name(application_group_t group) {
    return group == APPLICATION_GROUP_CHAT ? "Chat" : "Game";
}

static void sink_input_volume_success_callback(pa_context *c,
                                               int success,
                                               void *userdata) {
    (void)userdata;
    if (success) return;

    int error = pa_context_errno(c);
    fprintf(stderr,
            "PulseAudio sink-input volume acknowledgement failed: %s\n",
            pa_strerror(error));
}

static int set_sink_input_volume_target(pa_context *c,
                                        uint32_t stream_index,
                                        unsigned int channel_count,
                                        pa_volume_t pulse_volume) {
    if (!c || channel_count == 0 || channel_count > PA_CHANNELS_MAX) {
        fprintf(stderr,
                "Failed to submit PulseAudio stream %u volume\n",
                stream_index);
        return -1;
    }

    pa_cvolume cvolume;
    pa_cvolume_init(&cvolume);
    pa_cvolume_set(&cvolume, channel_count, pulse_volume);

    pa_operation *operation = pa_context_set_sink_input_volume(
        c,
        stream_index,
        &cvolume,
        sink_input_volume_success_callback,
        NULL);
    if (!operation) {
        fprintf(stderr,
                "Failed to submit PulseAudio stream %u volume: %s\n",
                stream_index,
                pa_strerror(pa_context_errno(c)));
        return -1;
    }

    pa_operation_unref(operation);
    return 0;
}

static void apply_classified_volume_plan(
    pa_context *c,
    const classified_volume_plan_t *plan,
    const char *action) {
    for (size_t i = 0; i < plan->count; i++) {
        const classified_volume_assignment_t *assignment =
            &plan->assignments[i];
        if (set_sink_input_volume_target(
                c,
                assignment->stream_index,
                assignment->channel_count,
                assignment->pulse_volume) == 0) {
            printf("\n%s PulseAudio stream %u (%s)",
                   action,
                   assignment->stream_index,
                   application_group_name(assignment->group));
        }
    }
}

static void route_all_classified_applications(
    pa_context *c,
    const chatmix_volume_targets_t *targets) {
    classified_volume_plan_t plan;
    classified_volume_plan_init(&plan);

    if (classified_volume_plan_build_all(
            &plan,
            &application_inventory,
            &stream_inventory,
            &config,
            targets,
            derived_inventory_state_is_available(
                &application_inventory_state)) != 0) {
        fprintf(stderr, "Failed to plan classified application volumes\n");
        classified_volume_plan_clear(&plan);
        return;
    }

    apply_classified_volume_plan(c, &plan, "Submitted volume for");
    classified_volume_plan_clear(&plan);
}

static void route_classified_application_for_new_stream(
    pa_context *c,
    uint32_t stream_index) {
    classified_volume_plan_t plan;
    classified_volume_plan_init(&plan);

    if (classified_volume_plan_build_for_stream(
            &plan,
            &application_inventory,
            &stream_inventory,
            &config,
            &last_chatmix_targets,
            derived_inventory_state_is_available(
                &application_inventory_state),
            stream_index) != 0) {
        fprintf(stderr,
                "Failed to plan classified volume for new stream %u\n",
                stream_index);
        classified_volume_plan_clear(&plan);
        return;
    }

    apply_classified_volume_plan(c, &plan, "Submitted current mix for");
    classified_volume_plan_clear(&plan);
}

static int rebuild_active_applications_after_event(
    const char *event_description,
    uint32_t index) {
    if (!derived_inventory_state_can_rebuild(&application_inventory_state)) {
        return -1;
    }

    int succeeded = active_application_inventory_rebuild(
        &application_inventory,
        &stream_inventory) == 0;
    derived_inventory_state_set_rebuild_result(
        &application_inventory_state,
        succeeded);
    if (!succeeded) {
        fprintf(stderr,
                "Failed to rebuild active applications after %s stream %u\n",
                event_description,
                index);
        return -1;
    }

    return 0;
}

static void sink_input_event_info_cb(
    pa_context *ctx,
    const pa_sink_input_info *info,
    int eol,
    void *ud) {
    struct sink_input_info_request *request = ud;
    if (!request ||
        !sink_input_request_tracker_is_current(
            &sink_input_request_tracker,
            &request->token)) {
        return;
    }

    if (eol < 0) {
        int error = pa_context_errno(ctx);
        fprintf(stderr,
                "Failed to read %s PulseAudio stream information: %s\n",
                request->token.intent == SINK_INPUT_REQUEST_NEW
                    ? "new"
                    : "changed",
                pa_strerror(error));
        return;
    }
    if (eol > 0 || !info || request->result_received) return;
    if (info->index != request->token.index) return;

    int was_known =
        audio_stream_inventory_find(&stream_inventory, info->index) != NULL;
    request->result_received = 1;
    int rebuild_succeeded = 0;
    if (record_sink_input(info) != 0) {
        fprintf(stderr,
                "Failed to %s PulseAudio stream %u\n",
                request->token.intent == SINK_INPUT_REQUEST_NEW
                    ? "store"
                    : "update",
                info->index);
    } else {
        rebuild_succeeded = rebuild_active_applications_after_event(
            request->token.intent == SINK_INPUT_REQUEST_NEW
                ? "new"
                : "changed",
            info->index) == 0;
    }

    if (audio_new_stream_should_route_current_mix(
            request->token.intent == SINK_INPUT_REQUEST_NEW,
            was_known,
            rebuild_succeeded,
            derived_inventory_state_is_available(
                &application_inventory_state),
            has_valid_chatmix)) {
        route_classified_application_for_new_stream(ctx, info->index);
    }
}

static void sink_input_snapshot_cb(pa_context *ctx, const pa_sink_input_info *info, int eol, void *ud) {
    (void)ctx;
    struct snapshot_state *state = ud;

    if (eol < 0) {
        state->failed = 1;
        return;
    }
    if (eol > 0 || !info) return;

    if (record_sink_input(info) != 0) {
        state->failed = 1;
    }
}

static void subscribe_callback(pa_context *c, pa_subscription_event_type_t t, uint32_t idx, void *userdata) {
    (void)userdata;
    // Only react to sink input events
    if ((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) != PA_SUBSCRIPTION_EVENT_SINK_INPUT) return;

    pa_subscription_event_type_t type = t & PA_SUBSCRIPTION_EVENT_TYPE_MASK;
    if (type == PA_SUBSCRIPTION_EVENT_REMOVE) {
        sink_input_request_tracker_invalidate(
            &sink_input_request_tracker,
            idx);
        for (struct sink_input_info_request *request =
                 pending_sink_input_requests;
             request;
             request = request->next) {
            if (request->token.index == idx &&
                pa_operation_get_state(request->operation) ==
                    PA_OPERATION_RUNNING) {
                pa_operation_cancel(request->operation);
            }
        }

        audio_stream_inventory_remove(&stream_inventory, idx);
        rebuild_active_applications_after_event("removed", idx);
        return;
    }

    sink_input_request_intent_t intent;
    if (type == PA_SUBSCRIPTION_EVENT_NEW) {
        intent = SINK_INPUT_REQUEST_NEW;
    } else if (type == PA_SUBSCRIPTION_EVENT_CHANGE) {
        intent = SINK_INPUT_REQUEST_CHANGE;
    } else {
        return;
    }

    struct sink_input_info_request *request = calloc(1, sizeof(*request));
    if (!request ||
        sink_input_request_tracker_begin(
            &sink_input_request_tracker,
            idx,
            intent,
            &request->token) != 0) {
        free(request);
        fprintf(stderr, "Failed to track PulseAudio stream request %u\n", idx);
        return;
    }

    request->next = pending_sink_input_requests;
    pending_sink_input_requests = request;
    request->operation = pa_context_get_sink_input_info(
        c,
        idx,
        sink_input_event_info_cb,
        request);
    if (!request->operation) {
        pending_sink_input_requests = request->next;
        sink_input_request_tracker_finish(
            &sink_input_request_tracker,
            &request->token);
        free(request);
        fprintf(stderr, "Failed to request PulseAudio stream %u\n", idx);
    }
}

static void reap_sink_input_requests(void) {
    struct sink_input_info_request **position =
        &pending_sink_input_requests;
    while (*position) {
        struct sink_input_info_request *request = *position;
        if (pa_operation_get_state(request->operation) ==
            PA_OPERATION_RUNNING) {
            position = &request->next;
            continue;
        }

        *position = request->next;
        sink_input_request_tracker_finish(
            &sink_input_request_tracker,
            &request->token);
        pa_operation_unref(request->operation);
        free(request);
    }
}

static void cancel_and_release_sink_input_requests(void) {
    struct sink_input_info_request *request = pending_sink_input_requests;
    pending_sink_input_requests = NULL;
    while (request) {
        struct sink_input_info_request *next = request->next;
        if (pa_operation_get_state(request->operation) ==
            PA_OPERATION_RUNNING) {
            pa_operation_cancel(request->operation);
        }
        sink_input_request_tracker_finish(
            &sink_input_request_tracker,
            &request->token);
        pa_operation_unref(request->operation);
        free(request);
        request = next;
    }
}

static int audio_init_shutdown_requested(
    const audio_init_options_t *options) {
    return options && options->running && !*options->running;
}

static int audio_monotonic_now_ns(uint64_t *now_ns) {
    struct timespec timestamp;
    if (!now_ns || clock_gettime(CLOCK_MONOTONIC, &timestamp) != 0 ||
        timestamp.tv_sec < (time_t)0 ||
        timestamp.tv_nsec < 0 ||
        timestamp.tv_nsec >= 1000000000L) {
        return -1;
    }

    uint64_t seconds = (uint64_t)timestamp.tv_sec;
    uint64_t partial_ns = (uint64_t)timestamp.tv_nsec;
    if (seconds > (UINT64_MAX - partial_ns) / UINT64_C(1000000000)) {
        return -1;
    }

    *now_ns = seconds * UINT64_C(1000000000) + partial_ns;
    return 0;
}

static audio_init_wait_result_t current_init_wait_result(
    int completed,
    int failed,
    uint64_t deadline_ns,
    const audio_init_options_t *options) {
    if (audio_init_shutdown_requested(options)) {
        return AUDIO_INIT_WAIT_SHUTDOWN;
    }

    uint64_t now_ns;
    if (audio_monotonic_now_ns(&now_ns) != 0) {
        return AUDIO_INIT_WAIT_FAILURE;
    }

    return audio_init_wait_decide(
        completed,
        failed,
        audio_init_shutdown_requested(options),
        now_ns,
        deadline_ns);
}

static audio_init_result_t init_result_from_wait(
    audio_init_wait_result_t result) {
    switch (result) {
        case AUDIO_INIT_WAIT_SUCCESS:
            return AUDIO_INIT_OK;
        case AUDIO_INIT_WAIT_TIMEOUT:
            return AUDIO_INIT_TIMEOUT;
        case AUDIO_INIT_WAIT_SHUTDOWN:
            return AUDIO_INIT_SHUTDOWN;
        case AUDIO_INIT_WAIT_PENDING:
        case AUDIO_INIT_WAIT_FAILURE:
        default:
            return AUDIO_INIT_FAILED;
    }
}

static int wait_before_next_init_pump(void) {
    const struct timespec wait_time = {
        .tv_sec = 0,
        .tv_nsec = (long)AUDIO_INIT_WAIT_NS,
    };

    if (nanosleep(&wait_time, NULL) == 0 || errno == EINTR) return 0;
    return -1;
}

static audio_init_result_t wait_for_initial_context(
    uint64_t deadline_ns,
    const audio_init_options_t *options) {
    for (;;) {
        audio_init_wait_result_t wait_result = current_init_wait_result(
            observed_context_state == PA_CONTEXT_READY,
            observed_context_state == PA_CONTEXT_FAILED ||
                observed_context_state == PA_CONTEXT_TERMINATED,
            deadline_ns,
            options);
        if (wait_result != AUDIO_INIT_WAIT_PENDING) {
            return init_result_from_wait(wait_result);
        }

        if (pa_mainloop_iterate(mainloop, 0, NULL) < 0) {
            return AUDIO_INIT_FAILED;
        }
        reap_sink_input_requests();

        wait_result = current_init_wait_result(
            observed_context_state == PA_CONTEXT_READY,
            observed_context_state == PA_CONTEXT_FAILED ||
                observed_context_state == PA_CONTEXT_TERMINATED,
            deadline_ns,
            options);
        if (wait_result != AUDIO_INIT_WAIT_PENDING) {
            return init_result_from_wait(wait_result);
        }
        if (wait_before_next_init_pump() != 0) return AUDIO_INIT_FAILED;
    }
}

static audio_init_result_t wait_for_initialization_operation(
    pa_operation *operation,
    uint64_t deadline_ns,
    const audio_init_options_t *options) {
    if (!operation) return AUDIO_INIT_FAILED;

    for (;;) {
        pa_operation_state_t operation_state =
            pa_operation_get_state(operation);
        audio_init_wait_result_t wait_result = current_init_wait_result(
            operation_state == PA_OPERATION_DONE,
            operation_state == PA_OPERATION_CANCELLED ||
                observed_context_state == PA_CONTEXT_FAILED ||
                observed_context_state == PA_CONTEXT_TERMINATED,
            deadline_ns,
            options);
        if (wait_result != AUDIO_INIT_WAIT_PENDING) {
            return init_result_from_wait(wait_result);
        }

        if (pa_mainloop_iterate(mainloop, 0, NULL) < 0) {
            return AUDIO_INIT_FAILED;
        }
        reap_sink_input_requests();

        operation_state = pa_operation_get_state(operation);
        wait_result = current_init_wait_result(
            operation_state == PA_OPERATION_DONE,
            operation_state == PA_OPERATION_CANCELLED ||
                observed_context_state == PA_CONTEXT_FAILED ||
                observed_context_state == PA_CONTEXT_TERMINATED,
            deadline_ns,
            options);
        if (wait_result != AUDIO_INIT_WAIT_PENDING) {
            return init_result_from_wait(wait_result);
        }
        if (wait_before_next_init_pump() != 0) return AUDIO_INIT_FAILED;
    }
}

audio_init_result_t initialize_audio_server(
    const audio_init_options_t *options) {
    audio_init_result_t result = AUDIO_INIT_FAILED;
    uint64_t start_ns;

    if (context || mainloop || pending_sink_input_requests ||
        sink_input_request_tracker.indexes ||
        sink_input_request_tracker.requests ||
        stream_inventory.streams ||
        application_inventory.applications) {
        return AUDIO_INIT_FAILED;
    }

    pending_sink_input_requests = NULL;
    sink_input_request_tracker_init(&sink_input_request_tracker);
    derived_inventory_state_init(&application_inventory_state);
    audio_stream_inventory_init(&stream_inventory);
    active_application_inventory_init(&application_inventory);
    observed_context_state = PA_CONTEXT_UNCONNECTED;

    if (audio_init_shutdown_requested(options)) {
        result = AUDIO_INIT_SHUTDOWN;
        goto fail;
    }
    if (audio_monotonic_now_ns(&start_ns) != 0) goto fail;
    uint64_t deadline_ns = pulse_reconnect_deadline_after(
        start_ns,
        AUDIO_INIT_TIMEOUT_NS);

    mainloop = pa_mainloop_new();
    if (!mainloop) goto fail;

    pa_mainloop_api *mainloop_api = pa_mainloop_get_api(mainloop);
    context = pa_context_new(mainloop_api, "chatwheel");
    if (!context) goto fail;

    pa_context_set_state_callback(context, context_state_callback, NULL);
    if (audio_init_shutdown_requested(options)) {
        result = AUDIO_INIT_SHUTDOWN;
        goto fail;
    }
    if (pa_context_connect(context, NULL, 0, NULL) < 0) goto fail;

    result = wait_for_initial_context(deadline_ns, options);
    if (result != AUDIO_INIT_OK) goto fail;

    // Subscribe before taking the snapshot so changes during it are not missed.
    pa_context_set_subscribe_callback(context, subscribe_callback, NULL);
    int subscription_succeeded = 0;
    result = AUDIO_INIT_FAILED;
    pa_operation *sub = pa_context_subscribe(context,
        (pa_subscription_mask_t)(PA_SUBSCRIPTION_MASK_SINK_INPUT),
        subscribe_success_callback,
        &subscription_succeeded);
    if (!sub) goto fail;

    result = wait_for_initialization_operation(sub, deadline_ns, options);
    pa_operation_state_t subscription_state = pa_operation_get_state(sub);
    if (subscription_state == PA_OPERATION_RUNNING) {
        pa_operation_cancel(sub);
    }
    pa_operation_unref(sub);
    if (result != AUDIO_INIT_OK ||
        subscription_state != PA_OPERATION_DONE ||
        !subscription_succeeded) {
        if (result == AUDIO_INIT_OK) result = AUDIO_INIT_FAILED;
        goto fail;
    }

    struct snapshot_state snapshot = {0};
    result = AUDIO_INIT_FAILED;
    pa_operation *snapshot_op = pa_context_get_sink_input_info_list(
        context,
        sink_input_snapshot_cb,
        &snapshot);
    if (!snapshot_op) goto fail;

    result = wait_for_initialization_operation(
        snapshot_op,
        deadline_ns,
        options);
    pa_operation_state_t snapshot_operation_state =
        pa_operation_get_state(snapshot_op);
    if (snapshot_operation_state == PA_OPERATION_RUNNING) {
        pa_operation_cancel(snapshot_op);
    }
    pa_operation_unref(snapshot_op);
    if (result != AUDIO_INIT_OK ||
        snapshot_operation_state != PA_OPERATION_DONE ||
        snapshot.failed) {
        if (result == AUDIO_INIT_OK) result = AUDIO_INIT_FAILED;
        goto fail;
    }

    derived_inventory_state_mark_initial_snapshot_complete(
        &application_inventory_state);
    int initial_rebuild_succeeded = active_application_inventory_rebuild(
        &application_inventory,
        &stream_inventory) == 0;
    derived_inventory_state_set_rebuild_result(
        &application_inventory_state,
        initial_rebuild_succeeded);
    if (!initial_rebuild_succeeded) {
        result = AUDIO_INIT_FAILED;
        fprintf(stderr,
                "Failed to build active application inventory from PulseAudio snapshot\n");
        goto fail;
    }

    result = init_result_from_wait(current_init_wait_result(
        1,
        observed_context_state == PA_CONTEXT_FAILED ||
            observed_context_state == PA_CONTEXT_TERMINATED,
        deadline_ns,
        options));
    if (result != AUDIO_INIT_OK) goto fail;

    if (has_valid_chatmix) {
        route_all_classified_applications(context, &last_chatmix_targets);
    }

    return AUDIO_INIT_OK;

fail:
    cleanup_audio_server();
    return result;
}

void cleanup_audio_server(void) {
    derived_inventory_state_init(&application_inventory_state);
    if (context) {
        pa_context_set_state_callback(context, NULL, NULL);
        pa_context_set_subscribe_callback(context, NULL, NULL);
    }
    cancel_and_release_sink_input_requests();
    if (context) {
        pa_context_disconnect(context);
        pa_context_unref(context);
        context = NULL;
    }
    if (mainloop) {
        pa_mainloop_free(mainloop);
        mainloop = NULL;
    }
    observed_context_state = PA_CONTEXT_UNCONNECTED;
    sink_input_request_tracker_clear(&sink_input_request_tracker);
    active_application_inventory_clear(&application_inventory);
    audio_stream_inventory_clear(&stream_inventory);
    /* The latest valid ChatMix target belongs to mixer policy, not a session. */
}

static int iterate_audio_mainloop(void *userdata, int block) {
    if (observed_context_state == PA_CONTEXT_FAILED ||
        observed_context_state == PA_CONTEXT_TERMINATED) {
        return 0;
    }
    return pa_mainloop_iterate(userdata, block, NULL);
}

static void reap_audio_requests(void *userdata) {
    (void)userdata;
    reap_sink_input_requests();
}

audio_event_status_t process_audio_events(void) {
    audio_event_status_t status = audio_event_status_from_observation(
        observed_context_state,
        0);
    if (status != AUDIO_EVENTS_OK) return status;
    if (!mainloop) return AUDIO_EVENTS_OK;

    pulse_event_drain_result_t result = pulse_event_drain(
        iterate_audio_mainloop,
        mainloop,
        reap_audio_requests,
        NULL);
    return audio_event_status_from_observation(
        observed_context_state,
        result == PULSE_EVENT_DRAIN_ERROR);
}

size_t get_active_audio_stream_count(void) {
    return stream_inventory.count;
}

int get_active_audio_stream(size_t position, audio_stream_view_t *stream) {
    if (!stream || position >= stream_inventory.count) return -1;

    const audio_stream_t *inventory_stream =
        &stream_inventory.streams[position];
    stream->index = inventory_stream->index;
    stream->application_id = inventory_stream->application_id;
    stream->application_name = inventory_stream->application_name;
    stream->process_binary = inventory_stream->process_binary;
    stream->node_name = inventory_stream->node_name;
    return 0;
}

size_t get_active_application_count(void) {
    return derived_inventory_state_is_available(&application_inventory_state)
        ? application_inventory.count
        : 0;
}

int get_active_application(size_t position, active_application_view_t *view) {
    if (!derived_inventory_state_is_available(&application_inventory_state) ||
        !view ||
        position >= application_inventory.count) {
        return -1;
    }

    const active_application_t *application =
        &application_inventory.applications[position];
    application_classification_t classification =
        application_classifier_classify(
            application,
            &stream_inventory,
            &config);
    view->identity_property = application->identity_property;
    view->identity_value = application->identity_value;
    view->display_name = application->display_name;
    view->stream_indexes = application->stream_indexes;
    view->stream_count = application->stream_count;
    view->group = classification.group;
    view->matched_config_index = classification.matched_config_index;
    return 0;
}

static int wait_for_operation(pa_operation *op) {
    if (!op) return -1;

    while (op && pa_operation_get_state(op) == PA_OPERATION_RUNNING) {
        int iterate_result = pa_mainloop_iterate(mainloop, 1, NULL);
        reap_sink_input_requests();
        if (iterate_result < 0) return -1;
    }

    reap_sink_input_requests();

    return 0;
}

static void list_apps_callback(pa_context *c, const pa_sink_input_info *i, int eol, void *userdata) {
    (void)c;      // Suppress warning
    (void)userdata; // Suppress warning
    
    if (eol > 0 || !i) return;

    const char *app_name = pa_proplist_gets(i->proplist, "application.name");
    const char *binary = pa_proplist_gets(i->proplist, "application.process.binary");
    float volume = pa_cvolume_avg(&i->volume) * 100.0f / PA_VOLUME_NORM;
    
    if (app_name) {
        printf("Application: %-20s Volume: %.0f%% [Index: %u]", app_name, volume, i->index);
        if (binary) {
            printf(" (Binary: %s)", binary);
        }
        printf("\n");
    }
}

void adjust_volume_based_on_chatmix(float chatmix_value) {
    chatmix_volume_targets_t targets;
    if (chatmix_volume_targets_calculate(chatmix_value, &targets) != 0) {
        fprintf(stderr, "Invalid ChatMix value: %.0f\n", chatmix_value);
        return;
    }

    last_chatmix_targets = targets;
    has_valid_chatmix = 1;

    printf("\nChatmix position: %.0f%%", targets.normalized * 100);
    printf("\nTarget volumes - Game: %.0f%% (%.0f%% logarithmic), Chat: %.0f%% (%.0f%% logarithmic)", 
           targets.game.linear * 100, targets.game.logarithmic * 100,
           targets.chat.linear * 100, targets.chat.logarithmic * 100);
    
    if (context && observed_context_state == PA_CONTEXT_READY &&
        derived_inventory_state_is_available(&application_inventory_state)) {
        route_all_classified_applications(context, &targets);
    }
    printf("\n");
}

void list_applications(void) {
    if (!context) {
        printf("No PulseAudio context available\n");
        return;
    }

    pa_operation *op = pa_context_get_sink_input_info_list(context, list_apps_callback, NULL);
    if (op) {
        wait_for_operation(op);
        pa_operation_unref(op);
    }
}

static int is_app_configured(const char* app_name) {
    for (int i = 0; i < config.count; i++) {
        if (pattern_matches_text(config.apps[i].name, app_name)) {
            return 1;
        }
    }
    return 0;
}

static void list_unconfigured_callback(pa_context *c, const pa_sink_input_info *i, int eol, void *userdata) {
    (void)c;
    (void)userdata;
    
    if (eol > 0 || !i) return;
    const char *app_name = pa_proplist_gets(i->proplist, "application.name");
    if (app_name && !is_app_configured(app_name)) {
        printf("%s\n", app_name);
    }
}

void list_unconfigured_applications(void) {
    if (!context) {
        printf("No PulseAudio context available\n");
        return;
    }

    printf("Unconfigured applications:\n");
    pa_operation *op = pa_context_get_sink_input_info_list(context, 
                                                          list_unconfigured_callback, 
                                                          NULL);
    if (op) {
        wait_for_operation(op);
        pa_operation_unref(op);
    }
}
