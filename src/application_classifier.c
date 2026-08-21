#include "application_classifier.h"

#include "pattern_matcher.h"

static application_classification_t unassigned_classification(void) {
    return (application_classification_t){
        .group = APPLICATION_GROUP_UNASSIGNED,
        .matched_config_index = -1,
    };
}

static int stream_matches_pattern(const audio_stream_t *stream,
                                  const char *pattern) {
    if (stream->application_id &&
        pattern_matches_text(pattern, stream->application_id)) {
        return 1;
    }
    if (stream->application_name &&
        pattern_matches_text(pattern, stream->application_name)) {
        return 1;
    }
    if (stream->process_binary &&
        pattern_matches_text(pattern, stream->process_binary)) {
        return 1;
    }
    if (stream->node_name &&
        pattern_matches_text(pattern, stream->node_name)) {
        return 1;
    }
    return 0;
}

application_classification_t application_classifier_classify(
    const active_application_t *application,
    const audio_stream_inventory_t *streams,
    const config_t *configuration) {
    application_classification_t result = unassigned_classification();

    if (!application || !streams || !configuration) return result;
    if (configuration->count < 0 || configuration->count > MAX_APPS) {
        return result;
    }
    if (streams->count > streams->capacity ||
        (streams->count > 0 && !streams->streams)) {
        return result;
    }
    if (application->stream_count > application->stream_capacity ||
        (application->stream_count > 0 && !application->stream_indexes)) {
        return result;
    }

    for (int config_index = 0;
         config_index < configuration->count;
         config_index++) {
        const app_config_t *config_entry =
            &configuration->apps[config_index];

        for (size_t stream_position = 0;
             stream_position < application->stream_count;
             stream_position++) {
            const audio_stream_t *stream = audio_stream_inventory_find(
                streams,
                application->stream_indexes[stream_position]);
            if (!stream) continue;

            if (stream_matches_pattern(stream, config_entry->name)) {
                result.group = config_entry->is_chat != 0
                    ? APPLICATION_GROUP_CHAT
                    : APPLICATION_GROUP_GAME;
                result.matched_config_index = config_index;
                return result;
            }
        }
    }

    return result;
}
