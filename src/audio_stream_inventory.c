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
                                  const char *application_name,
                                  const char *process_binary) {
    if (!inventory) return -1;

    char *application_name_copy = NULL;
    char *process_binary_copy = NULL;

    if (duplicate_nullable_string(application_name, &application_name_copy) != 0) {
        return -1;
    }
    if (duplicate_nullable_string(process_binary, &process_binary_copy) != 0) {
        free(application_name_copy);
        return -1;
    }

    for (size_t i = 0; i < inventory->count; i++) {
        audio_stream_t *stream = &inventory->streams[i];
        if (stream->index != index) continue;

        free(stream->application_name);
        free(stream->process_binary);
        stream->application_name = application_name_copy;
        stream->process_binary = process_binary_copy;
        return 0;
    }

    if (ensure_capacity(inventory) != 0) {
        free(application_name_copy);
        free(process_binary_copy);
        return -1;
    }

    audio_stream_t *stream = &inventory->streams[inventory->count];
    stream->index = index;
    stream->application_name = application_name_copy;
    stream->process_binary = process_binary_copy;
    inventory->count++;
    return 0;
}

int audio_stream_inventory_remove(audio_stream_inventory_t *inventory,
                                  uint32_t index) {
    if (!inventory) return 0;

    for (size_t i = 0; i < inventory->count; i++) {
        audio_stream_t *stream = &inventory->streams[i];
        if (stream->index != index) continue;

        free(stream->application_name);
        free(stream->process_binary);

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
        free(inventory->streams[i].application_name);
        free(inventory->streams[i].process_binary);
    }

    free(inventory->streams);
    audio_stream_inventory_init(inventory);
}
