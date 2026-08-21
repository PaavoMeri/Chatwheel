#include "classified_volume_routing.h"

#include <pulse/sample.h>
#include <stdint.h>
#include <stdlib.h>

#define INITIAL_ASSIGNMENT_CAPACITY 4

static int inventories_are_valid(
    const active_application_inventory_t *applications,
    const audio_stream_inventory_t *streams) {
    if (!applications || !streams) return 0;
    if (applications->count > applications->capacity ||
        (applications->count > 0 && !applications->applications)) {
        return 0;
    }
    if (streams->count > streams->capacity ||
        (streams->count > 0 && !streams->streams)) {
        return 0;
    }
    for (size_t i = 0; i < streams->count; i++) {
        unsigned int channel_count = streams->streams[i].channel_count;
        if (channel_count == 0 || channel_count > PA_CHANNELS_MAX) return 0;
    }

    for (size_t i = 0; i < applications->count; i++) {
        const active_application_t *application =
            &applications->applications[i];
        if (application->stream_count > application->stream_capacity ||
            (application->stream_count > 0 &&
             !application->stream_indexes)) {
            return 0;
        }
    }

    return 1;
}

static int ensure_assignment_capacity(classified_volume_plan_t *plan) {
    if (plan->count < plan->capacity) return 0;

    size_t new_capacity = INITIAL_ASSIGNMENT_CAPACITY;
    if (plan->capacity > 0) {
        if (plan->capacity > SIZE_MAX / 2) return -1;
        new_capacity = plan->capacity * 2;
    }
    if (new_capacity > SIZE_MAX / sizeof(*plan->assignments)) return -1;

    classified_volume_assignment_t *resized = realloc(
        plan->assignments,
        new_capacity * sizeof(*plan->assignments));
    if (!resized) return -1;

    plan->assignments = resized;
    plan->capacity = new_capacity;
    return 0;
}

static int plan_contains_stream(const classified_volume_plan_t *plan,
                                uint32_t stream_index) {
    for (size_t i = 0; i < plan->count; i++) {
        if (plan->assignments[i].stream_index == stream_index) return 1;
    }
    return 0;
}

static const chatmix_volume_target_t *target_for_group(
    application_group_t group,
    const chatmix_volume_targets_t *targets) {
    if (group == APPLICATION_GROUP_GAME) return &targets->game;
    if (group == APPLICATION_GROUP_CHAT) return &targets->chat;
    return NULL;
}

static int add_application_assignments(
    classified_volume_plan_t *plan,
    const active_application_t *application,
    const audio_stream_inventory_t *streams,
    const config_t *configuration,
    const chatmix_volume_targets_t *targets) {
    application_classification_t classification =
        application_classifier_classify(
            application,
            streams,
            configuration);
    const chatmix_volume_target_t *target = target_for_group(
        classification.group,
        targets);
    if (!target) return 0;

    for (size_t i = 0; i < application->stream_count; i++) {
        uint32_t stream_index = application->stream_indexes[i];
        const audio_stream_t *stream = audio_stream_inventory_find(
            streams,
            stream_index);
        if (!stream || plan_contains_stream(plan, stream_index)) {
            continue;
        }
        if (ensure_assignment_capacity(plan) != 0) return -1;

        plan->assignments[plan->count] =
            (classified_volume_assignment_t){
                .stream_index = stream_index,
                .channel_count = stream->channel_count,
                .group = classification.group,
                .pulse_volume = target->pulse,
            };
        plan->count++;
    }

    return 0;
}

static int application_contains_stream(
    const active_application_t *application,
    uint32_t stream_index) {
    for (size_t i = 0; i < application->stream_count; i++) {
        if (application->stream_indexes[i] == stream_index) return 1;
    }
    return 0;
}

void classified_volume_plan_init(classified_volume_plan_t *plan) {
    if (!plan) return;
    *plan = (classified_volume_plan_t){0};
}

static int build_plan(
    classified_volume_plan_t *destination,
    const active_application_inventory_t *applications,
    const audio_stream_inventory_t *streams,
    const config_t *configuration,
    const chatmix_volume_targets_t *targets,
    int inventory_available,
    int limit_to_stream,
    uint32_t stream_index) {
    if (!destination || !applications || !streams || !configuration ||
        !targets || !inventories_are_valid(applications, streams)) {
        return -1;
    }

    classified_volume_plan_t replacement;
    classified_volume_plan_init(&replacement);

    if (inventory_available) {
        for (size_t i = 0; i < applications->count; i++) {
            const active_application_t *application =
                &applications->applications[i];
            if (limit_to_stream &&
                !application_contains_stream(application, stream_index)) {
                continue;
            }
            if (add_application_assignments(
                    &replacement,
                    application,
                    streams,
                    configuration,
                    targets) != 0) {
                classified_volume_plan_clear(&replacement);
                return -1;
            }
            if (limit_to_stream) break;
        }
    }

    classified_volume_plan_clear(destination);
    *destination = replacement;
    return 0;
}

int classified_volume_plan_build_all(
    classified_volume_plan_t *destination,
    const active_application_inventory_t *applications,
    const audio_stream_inventory_t *streams,
    const config_t *configuration,
    const chatmix_volume_targets_t *targets,
    int inventory_available) {
    return build_plan(
        destination,
        applications,
        streams,
        configuration,
        targets,
        inventory_available,
        0,
        0);
}

int classified_volume_plan_build_for_stream(
    classified_volume_plan_t *destination,
    const active_application_inventory_t *applications,
    const audio_stream_inventory_t *streams,
    const config_t *configuration,
    const chatmix_volume_targets_t *targets,
    int inventory_available,
    uint32_t stream_index) {
    return build_plan(
        destination,
        applications,
        streams,
        configuration,
        targets,
        inventory_available,
        1,
        stream_index);
}

void classified_volume_plan_clear(classified_volume_plan_t *plan) {
    if (!plan) return;
    free(plan->assignments);
    classified_volume_plan_init(plan);
}
