#include "active_application_inventory.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_APPLICATION_CAPACITY 4
#define INITIAL_STREAM_INDEX_CAPACITY 4

static void clear_application(active_application_t *application) {
    free(application->identity_value);
    free(application->display_name);
    free(application->stream_indexes);
    *application = (active_application_t){0};
}

static int ensure_application_capacity(
    active_application_inventory_t *inventory) {
    if (inventory->count < inventory->capacity) return 0;

    size_t new_capacity = INITIAL_APPLICATION_CAPACITY;
    if (inventory->capacity > 0) {
        if (inventory->capacity > SIZE_MAX / 2) return -1;
        new_capacity = inventory->capacity * 2;
    }
    if (new_capacity > SIZE_MAX / sizeof(*inventory->applications)) return -1;

    active_application_t *resized = realloc(
        inventory->applications,
        new_capacity * sizeof(*inventory->applications));
    if (!resized) return -1;

    inventory->applications = resized;
    inventory->capacity = new_capacity;
    return 0;
}

static int ensure_stream_capacity(active_application_t *application) {
    if (application->stream_count < application->stream_capacity) return 0;

    size_t new_capacity = INITIAL_STREAM_INDEX_CAPACITY;
    if (application->stream_capacity > 0) {
        if (application->stream_capacity > SIZE_MAX / 2) return -1;
        new_capacity = application->stream_capacity * 2;
    }
    if (new_capacity > SIZE_MAX / sizeof(*application->stream_indexes)) {
        return -1;
    }

    uint32_t *resized = realloc(
        application->stream_indexes,
        new_capacity * sizeof(*application->stream_indexes));
    if (!resized) return -1;

    application->stream_indexes = resized;
    application->stream_capacity = new_capacity;
    return 0;
}

static int add_stream_index(active_application_t *application,
                            uint32_t stream_index) {
    for (size_t i = 0; i < application->stream_count; i++) {
        if (application->stream_indexes[i] == stream_index) return 0;
    }

    if (ensure_stream_capacity(application) != 0) return -1;
    application->stream_indexes[application->stream_count] = stream_index;
    application->stream_count++;
    return 0;
}

static active_application_t *find_mutable_application(
    active_application_inventory_t *inventory,
    application_identity_property_t identity_property,
    const char *identity_value) {
    for (size_t i = 0; i < inventory->count; i++) {
        active_application_t *application = &inventory->applications[i];
        if (application->identity_property == identity_property &&
            strcmp(application->identity_value, identity_value) == 0) {
            return application;
        }
    }

    return NULL;
}

static int initialize_application(
    active_application_t *application,
    application_identity_resolution_t identity,
    application_identity_resolution_t display_name) {
    *application = (active_application_t){
        .identity_property = identity.property,
    };

    application->identity_value = strdup(identity.value);
    if (!application->identity_value) return -1;

    application->display_name = strdup(display_name.value);
    if (!application->display_name) {
        clear_application(application);
        return -1;
    }

    return 0;
}

void active_application_inventory_init(
    active_application_inventory_t *inventory) {
    if (!inventory) return;
    inventory->applications = NULL;
    inventory->count = 0;
    inventory->capacity = 0;
}

int active_application_inventory_rebuild(
    active_application_inventory_t *destination,
    const audio_stream_inventory_t *streams) {
    if (!destination || !streams) return -1;
    if (streams->count > 0 && !streams->streams) return -1;

    active_application_inventory_t replacement;
    active_application_inventory_init(&replacement);

    for (size_t i = 0; i < streams->count; i++) {
        const audio_stream_t *stream = &streams->streams[i];
        application_identity_resolution_t identity =
            application_identity_resolve(stream);
        if (identity.property == APPLICATION_IDENTITY_PROPERTY_NONE) continue;
        if (!identity.value) goto fail;

        application_identity_resolution_t display_name =
            application_display_name_resolve(stream);
        if (display_name.property == APPLICATION_IDENTITY_PROPERTY_NONE ||
            !display_name.value) {
            goto fail;
        }

        active_application_t *application = find_mutable_application(
            &replacement,
            identity.property,
            identity.value);
        if (application) {
            if (add_stream_index(application, stream->index) != 0) goto fail;
            continue;
        }

        active_application_t new_application;
        if (initialize_application(
                &new_application,
                identity,
                display_name) != 0) {
            goto fail;
        }
        if (add_stream_index(&new_application, stream->index) != 0) {
            clear_application(&new_application);
            goto fail;
        }
        if (ensure_application_capacity(&replacement) != 0) {
            clear_application(&new_application);
            goto fail;
        }

        replacement.applications[replacement.count] = new_application;
        replacement.count++;
    }

    active_application_inventory_clear(destination);
    *destination = replacement;
    return 0;

fail:
    active_application_inventory_clear(&replacement);
    return -1;
}

const active_application_t *active_application_inventory_find(
    const active_application_inventory_t *inventory,
    application_identity_property_t identity_property,
    const char *identity_value) {
    if (!inventory ||
        identity_property == APPLICATION_IDENTITY_PROPERTY_NONE ||
        !identity_value ||
        identity_value[0] == '\0') {
        return NULL;
    }

    for (size_t i = 0; i < inventory->count; i++) {
        const active_application_t *application = &inventory->applications[i];
        if (application->identity_property == identity_property &&
            strcmp(application->identity_value, identity_value) == 0) {
            return application;
        }
    }

    return NULL;
}

const active_application_t *active_application_inventory_get(
    const active_application_inventory_t *inventory,
    size_t position) {
    if (!inventory || position >= inventory->count) return NULL;
    return &inventory->applications[position];
}

void active_application_inventory_clear(
    active_application_inventory_t *inventory) {
    if (!inventory) return;

    for (size_t i = 0; i < inventory->count; i++) {
        clear_application(&inventory->applications[i]);
    }
    free(inventory->applications);
    active_application_inventory_init(inventory);
}
