#ifndef APPLICATION_CLASSIFIER_H
#define APPLICATION_CLASSIFIER_H

#include "active_application_inventory.h"
#include "audio_stream_inventory.h"
#include "config.h"

typedef enum {
    APPLICATION_GROUP_UNASSIGNED,
    APPLICATION_GROUP_GAME,
    APPLICATION_GROUP_CHAT
} application_group_t;

typedef struct {
    application_group_t group;
    int matched_config_index;
} application_classification_t;

/*
 * Classifies one active application using raw stream properties and stored
 * configuration order. All inputs are borrowed and remain caller-owned; no
 * pointer is retained after the call. The application, stream inventory, and
 * configuration must be valid initialized structures.
 *
 * Configuration entries are checked in stored order, then the application's
 * stream indexes in stored order. Missing raw stream indexes are skipped. The
 * first config entry matching application.id, application.name,
 * application.process.binary, or node.name wins. Matching is case-insensitive
 * and supports '*' and '?' through the shared pattern matcher.
 *
 * NULL arguments, malformed inventory state, a negative configuration count,
 * a count greater than MAX_APPS, or no match return APPLICATION_GROUP_UNASSIGNED
 * with matched_config_index set to -1.
 */
application_classification_t application_classifier_classify(
    const active_application_t *application,
    const audio_stream_inventory_t *streams,
    const config_t *configuration);

#endif
