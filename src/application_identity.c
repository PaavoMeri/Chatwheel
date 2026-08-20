#include "application_identity.h"

static int is_available(const char *value) {
    return value && value[0] != '\0';
}

static application_identity_resolution_t make_resolution(
    application_identity_property_t property,
    const char *value) {
    application_identity_resolution_t resolution = {
        .property = property,
        .value = value,
    };
    return resolution;
}

application_identity_resolution_t application_identity_resolve(
    const audio_stream_t *stream) {
    if (!stream) {
        return make_resolution(APPLICATION_IDENTITY_PROPERTY_NONE, NULL);
    }
    if (is_available(stream->application_id)) {
        return make_resolution(
            APPLICATION_IDENTITY_PROPERTY_APPLICATION_ID,
            stream->application_id);
    }
    if (is_available(stream->application_name)) {
        return make_resolution(
            APPLICATION_IDENTITY_PROPERTY_APPLICATION_NAME,
            stream->application_name);
    }
    if (is_available(stream->process_binary)) {
        return make_resolution(
            APPLICATION_IDENTITY_PROPERTY_PROCESS_BINARY,
            stream->process_binary);
    }
    if (is_available(stream->node_name)) {
        return make_resolution(
            APPLICATION_IDENTITY_PROPERTY_NODE_NAME,
            stream->node_name);
    }

    return make_resolution(APPLICATION_IDENTITY_PROPERTY_NONE, NULL);
}

application_identity_resolution_t application_display_name_resolve(
    const audio_stream_t *stream) {
    if (!stream) {
        return make_resolution(APPLICATION_IDENTITY_PROPERTY_NONE, NULL);
    }
    if (is_available(stream->application_name)) {
        return make_resolution(
            APPLICATION_IDENTITY_PROPERTY_APPLICATION_NAME,
            stream->application_name);
    }
    if (is_available(stream->node_name)) {
        return make_resolution(
            APPLICATION_IDENTITY_PROPERTY_NODE_NAME,
            stream->node_name);
    }
    if (is_available(stream->process_binary)) {
        return make_resolution(
            APPLICATION_IDENTITY_PROPERTY_PROCESS_BINARY,
            stream->process_binary);
    }
    if (is_available(stream->application_id)) {
        return make_resolution(
            APPLICATION_IDENTITY_PROPERTY_APPLICATION_ID,
            stream->application_id);
    }

    return make_resolution(APPLICATION_IDENTITY_PROPERTY_NONE, NULL);
}
