#ifndef AUDIO_STREAM_INVENTORY_H
#define AUDIO_STREAM_INVENTORY_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t index;
    /* These strings are owned by the containing inventory. */
    char *application_name;
    char *process_binary;
} audio_stream_t;

typedef struct {
    audio_stream_t *streams;
    size_t count;
    size_t capacity;
} audio_stream_inventory_t;

/*
 * Initializes a new/uninitialized inventory or one reset by clear(). Calling
 * init on an inventory that owns resources loses those resources.
 */
void audio_stream_inventory_init(audio_stream_inventory_t *inventory);

/*
 * Returns a borrowed stream owned by the inventory. The caller must not free
 * or modify the stream or its strings. Any upsert(), remove(), or clear() call
 * may invalidate the returned pointer.
 */
const audio_stream_t *audio_stream_inventory_find(
    const audio_stream_inventory_t *inventory,
    uint32_t index);

/* Returns 0 on success and -1 for an invalid inventory or allocation failure. */
int audio_stream_inventory_upsert(audio_stream_inventory_t *inventory,
                                  uint32_t index,
                                  const char *application_name,
                                  const char *process_binary);

/*
 * Returns 1 when the index was found and removed. Returns 0 when the index was
 * not found or inventory is NULL.
 */
int audio_stream_inventory_remove(audio_stream_inventory_t *inventory,
                                  uint32_t index);
void audio_stream_inventory_clear(audio_stream_inventory_t *inventory);

#endif
