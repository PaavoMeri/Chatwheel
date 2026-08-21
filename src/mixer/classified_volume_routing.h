#ifndef CLASSIFIED_VOLUME_ROUTING_H
#define CLASSIFIED_VOLUME_ROUTING_H

#include <stddef.h>
#include <stdint.h>

#include "../active_application_inventory.h"
#include "../application_classifier.h"
#include "../audio_stream_inventory.h"
#include "../config.h"
#include "chatmix_volume.h"

typedef struct {
    uint32_t stream_index;
    unsigned int channel_count;
    application_group_t group;
    pa_volume_t pulse_volume;
} classified_volume_assignment_t;

typedef struct {
    classified_volume_assignment_t *assignments;
    size_t count;
    size_t capacity;
} classified_volume_plan_t;

/*
 * Initializes a new plan or one reset by clear(). Calling init() on a plan
 * that still owns storage would leak that storage.
 */
void classified_volume_plan_init(classified_volume_plan_t *plan);

/*
 * Builds assignments for every classified active application in inventory
 * order. The production classifier selects Game, Chat, or Unassigned; every
 * existing raw stream index of a classified application receives that group's
 * PulseAudio target while retaining its own channel count. Missing and
 * duplicate stream indexes are skipped.
 *
 * When inventory_available is zero, a successful build produces an empty plan
 * without classifying applications. All inputs are borrowed and no pointer is
 * retained. Destination must be initialized. Returns 0 on success and -1 for
 * invalid arguments or allocation failure. Failure leaves destination intact.
 * A raw channel count outside PulseAudio's supported range makes the complete
 * build fail atomically; malformed streams are never skipped into a partial
 * plan and their channel counts can never reach the PulseAudio setter.
 */
int classified_volume_plan_build_all(
    classified_volume_plan_t *destination,
    const active_application_inventory_t *applications,
    const audio_stream_inventory_t *streams,
    const config_t *configuration,
    const chatmix_volume_targets_t *targets,
    int inventory_available);

/*
 * Builds assignments only for the first active application containing
 * stream_index. If found and classified, all of that application's existing
 * raw stream indexes receive the selected target. Availability, ownership,
 * duplicate suppression, and failure semantics match build_all().
 */
int classified_volume_plan_build_for_stream(
    classified_volume_plan_t *destination,
    const active_application_inventory_t *applications,
    const audio_stream_inventory_t *streams,
    const config_t *configuration,
    const chatmix_volume_targets_t *targets,
    int inventory_available,
    uint32_t stream_index);

/* Frees owned storage. Repeated calls on an initialized plan are safe. */
void classified_volume_plan_clear(classified_volume_plan_t *plan);

#endif
