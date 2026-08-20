#ifndef ACTIVE_APPLICATION_INVENTORY_H
#define ACTIVE_APPLICATION_INVENTORY_H

#include <stddef.h>
#include <stdint.h>

#include "application_identity.h"

typedef struct {
    application_identity_property_t identity_property;
    char *identity_value;
    char *display_name;
    uint32_t *stream_indexes;
    size_t stream_count;
    size_t stream_capacity;
} active_application_t;

typedef struct {
    active_application_t *applications;
    size_t count;
    size_t capacity;
} active_application_inventory_t;

/*
 * Initializes a new/uninitialized inventory or one reset by clear(). Calling
 * init on an inventory that owns resources loses those resources.
 */
void active_application_inventory_init(
    active_application_inventory_t *inventory);

/*
 * Rebuilds destination from the complete stream inventory. Applications own
 * their identity_value, display_name, and stream_indexes. Streams without a
 * canonical identity are skipped. Returns 0 on success and -1 for invalid
 * arguments or an allocation failure.
 *
 * Streams must be a valid initialized audio_stream_inventory_t with one entry
 * per stream index. Applications are grouped only when both identity property
 * and identity value match, using exact identity-value comparison. Applications
 * are ordered by the first occurrence of each canonical identity in raw stream
 * inventory order. Stream indexes within an application follow raw stream
 * inventory order, excluding duplicate indexes. The first raw stream for a
 * canonical identity supplies display_name; later grouped streams do not
 * replace it.
 *
 * The complete replacement is built before destination is changed. On
 * failure, destination and pointers borrowed from it remain unchanged. On
 * success, all pointers previously borrowed from destination are invalidated.
 * Destination must be initialized before this function is called.
 */
int active_application_inventory_rebuild(
    active_application_inventory_t *destination,
    const audio_stream_inventory_t *streams);

/*
 * Returns a borrowed read-only application matching both the identity
 * property and exact identity value, or NULL when no match exists or the
 * arguments are invalid. The caller must not modify or free the application,
 * its strings, or its stream indexes. A successful rebuild or clear()
 * invalidates the returned pointer and all data reachable through it.
 */
const active_application_t *active_application_inventory_find(
    const active_application_inventory_t *inventory,
    application_identity_property_t identity_property,
    const char *identity_value);

/*
 * Returns a borrowed read-only application at position, or NULL when the
 * inventory is NULL or position is out of bounds. Ownership and lifetime are
 * the same as for active_application_inventory_find().
 */
const active_application_t *active_application_inventory_get(
    const active_application_inventory_t *inventory,
    size_t position);

/* Frees all owned data. Repeated calls on an initialized inventory are safe. */
void active_application_inventory_clear(
    active_application_inventory_t *inventory);

#endif
