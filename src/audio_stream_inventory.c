#include "audio_stream_inventory.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_STREAM_CAPACITY 4

static int duplicate_nullable_string(const char *source, char **copy) {
    if (!source) {
        *copy = NULL;
        return 0;
    }

    *copy = strdup(source);
    return *copy ? 0 : -1;
}

static void free_stream_properties(audio_stream_t *stream) {
    free(stream->application_id);
    free(stream->application_name);
    free(stream->process_binary);
    free(stream->node_name);
    stream->application_id = NULL;
    stream->application_name = NULL;
    stream->process_binary = NULL;
    stream->node_name = NULL;
}

static int copy_stream_properties(audio_stream_t *stream,
                                  const char *application_id,
                                  const char *application_name,
                                  const char *process_binary,
                                  const char *node_name) {
    if (duplicate_nullable_string(application_id, &stream->application_id) != 0) {
        goto fail;
    }
    if (duplicate_nullable_string(application_name, &stream->application_name) != 0) {
        goto fail;
    }
    if (duplicate_nullable_string(process_binary, &stream->process_binary) != 0) {
        goto fail;
    }
    if (duplicate_nullable_string(node_name, &stream->node_name) != 0) {
        goto fail;
    }

    return 0;

fail:
    free_stream_properties(stream);
    return -1;
}

static int ensure_capacity(audio_stream_inventory_t *inventory) {
    if (inventory->count < inventory->capacity) return 0;

    size_t new_capacity = INITIAL_STREAM_CAPACITY;
    if (inventory->capacity > 0) {
        if (inventory->capacity > SIZE_MAX / 2) return -1;
        new_capacity = inventory->capacity * 2;
    }

    if (new_capacity > SIZE_MAX / sizeof(*inventory->streams)) return -1;

    audio_stream_t *resized = realloc(
        inventory->streams,
        new_capacity * sizeof(*inventory->streams));
    if (!resized) return -1;

    inventory->streams = resized;
    inventory->capacity = new_capacity;
    return 0;
}

void audio_stream_inventory_init(audio_stream_inventory_t *inventory) {
    if (!inventory) return;

    inventory->streams = NULL;
    inventory->count = 0;
    inventory->capacity = 0;
}

const audio_stream_t *audio_stream_inventory_find(
    const audio_stream_inventory_t *inventory,
    uint32_t index) {
    if (!inventory) return NULL;

    for (size_t i = 0; i < inventory->count; i++) {
        if (inventory->streams[i].index == index) {
            return &inventory->streams[i];
        }
    }

    return NULL;
}

int audio_stream_inventory_upsert(audio_stream_inventory_t *inventory,
                                  uint32_t index,
                                  const char *application_id,
                                  const char *application_name,
                                  const char *process_binary,
                                  const char *node_name) {
    if (!inventory) return -1;

    audio_stream_t replacement = {.index = index};
    if (copy_stream_properties(
            &replacement,
            application_id,
            application_name,
            process_binary,
            node_name) != 0) {
        return -1;
    }

    for (size_t i = 0; i < inventory->count; i++) {
        audio_stream_t *stream = &inventory->streams[i];
        if (stream->index != index) continue;

        free_stream_properties(stream);
        *stream = replacement;
        return 0;
    }

    if (ensure_capacity(inventory) != 0) {
        free_stream_properties(&replacement);
        return -1;
    }

    inventory->streams[inventory->count] = replacement;
    inventory->count++;
    return 0;
}

int audio_stream_inventory_remove(audio_stream_inventory_t *inventory,
                                  uint32_t index) {
    if (!inventory) return 0;

    for (size_t i = 0; i < inventory->count; i++) {
        audio_stream_t *stream = &inventory->streams[i];
        if (stream->index != index) continue;

        free_stream_properties(stream);

        if (i + 1 < inventory->count) {
            memmove(stream,
                    stream + 1,
                    (inventory->count - i - 1) * sizeof(*stream));
        }

        inventory->count--;
        memset(&inventory->streams[inventory->count],
               0,
               sizeof(*inventory->streams));
        return 1;
    }

    return 0;
}

void audio_stream_inventory_clear(audio_stream_inventory_t *inventory) {
    if (!inventory) return;

    for (size_t i = 0; i < inventory->count; i++) {
        free_stream_properties(&inventory->streams[i]);
    }

    free(inventory->streams);
    audio_stream_inventory_init(inventory);
}
