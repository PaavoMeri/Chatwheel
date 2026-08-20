#ifndef APPLICATION_IDENTITY_H
#define APPLICATION_IDENTITY_H

#include "audio_stream_inventory.h"

typedef enum {
    APPLICATION_IDENTITY_PROPERTY_NONE = 0,
    APPLICATION_IDENTITY_PROPERTY_APPLICATION_ID,
    APPLICATION_IDENTITY_PROPERTY_APPLICATION_NAME,
    APPLICATION_IDENTITY_PROPERTY_PROCESS_BINARY,
    APPLICATION_IDENTITY_PROPERTY_NODE_NAME
} application_identity_property_t;

typedef struct {
    application_identity_property_t property;
    const char *value;
} application_identity_resolution_t;

/*
 * Selects a canonical identity in this order: non-empty application.id,
 * application.name, application.process.binary, then node.name.
 *
 * The returned value is borrowed from stream. The caller must not modify or
 * free it. It remains valid only while the selected stream property remains
 * valid; updating or removing the stream, or clearing its inventory, may
 * invalidate it. A NULL stream or a stream without a non-empty property
 * returns PROPERTY_NONE with a NULL value.
 */
application_identity_resolution_t application_identity_resolve(
    const audio_stream_t *stream);

/*
 * Selects a display name in this order: non-empty application.name, node.name,
 * application.process.binary, then application.id. Ownership and lifetime are
 * the same as for application_identity_resolve(). A NULL stream returns
 * APPLICATION_IDENTITY_PROPERTY_NONE with a NULL value.
 */
application_identity_resolution_t application_display_name_resolve(
    const audio_stream_t *stream);

#endif
