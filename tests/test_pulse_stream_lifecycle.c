#include <assert.h>
#include <pulse/proplist.h>
#include <stdio.h>
#include <string.h>

#include "audio_stream_inventory.h"
#include "mixer/pulse_stream_lifecycle.h"

static pa_proplist *create_properties(const char *application_name,
                                      const char *process_binary) {
    pa_proplist *properties = pa_proplist_new();
    assert(properties != NULL);

    if (application_name) {
        assert(pa_proplist_sets(
                   properties,
                   "application.name",
                   application_name) == 0);
    }
    if (process_binary) {
        assert(pa_proplist_sets(
                   properties,
                   "application.process.binary",
                   process_binary) == 0);
    }

    return properties;
}

static void test_initial_snapshot_copies_pulse_properties(void) {
    audio_stream_inventory_t inventory;
    audio_stream_inventory_init(&inventory);

    pa_proplist *properties = create_properties("Firefox", "firefox");
    assert(pulse_stream_lifecycle_record(&inventory, 42, properties) == 0);
    pa_proplist_free(properties);

    const audio_stream_t *stream = audio_stream_inventory_find(&inventory, 42);
    assert(stream != NULL);
    assert(strcmp(stream->application_name, "Firefox") == 0);
    assert(strcmp(stream->process_binary, "firefox") == 0);

    audio_stream_inventory_clear(&inventory);
}

static void test_snapshot_and_events_upsert_the_same_stream(void) {
    audio_stream_inventory_t inventory;
    audio_stream_inventory_init(&inventory);

    pa_proplist *snapshot_properties = create_properties("Discord", "Discord");
    assert(pulse_stream_lifecycle_record(
               &inventory, 10, snapshot_properties) == 0);
    pa_proplist_free(snapshot_properties);

    pa_proplist *new_properties = create_properties("Discord", "Discord");
    assert(pulse_stream_lifecycle_record(&inventory, 10, new_properties) == 0);
    pa_proplist_free(new_properties);
    assert(inventory.count == 1);

    pa_proplist *changed_properties = create_properties(
        "Discord Voice",
        "discord");
    assert(pulse_stream_lifecycle_record(
               &inventory, 10, changed_properties) == 0);
    pa_proplist_free(changed_properties);

    const audio_stream_t *stream = audio_stream_inventory_find(&inventory, 10);
    assert(stream != NULL);
    assert(strcmp(stream->application_name, "Discord Voice") == 0);
    assert(strcmp(stream->process_binary, "discord") == 0);
    assert(inventory.count == 1);

    assert(audio_stream_inventory_remove(&inventory, 10) == 1);
    assert(audio_stream_inventory_find(&inventory, 10) == NULL);

    audio_stream_inventory_clear(&inventory);
}

static void test_quickly_removed_unassigned_stream_does_not_remain(void) {
    audio_stream_inventory_t inventory;
    audio_stream_inventory_init(&inventory);

    pa_proplist *properties = create_properties("Unconfigured Player", NULL);
    assert(pulse_stream_lifecycle_record(&inventory, 77, properties) == 0);
    pa_proplist_free(properties);
    assert(audio_stream_inventory_find(&inventory, 77) != NULL);

    assert(audio_stream_inventory_remove(&inventory, 77) == 1);
    assert(inventory.count == 0);
    assert(audio_stream_inventory_find(&inventory, 77) == NULL);
    assert(audio_stream_inventory_remove(&inventory, 77) == 0);

    audio_stream_inventory_clear(&inventory);
}

static void test_missing_proplist_is_recorded_with_null_properties(void) {
    audio_stream_inventory_t inventory;
    audio_stream_inventory_init(&inventory);

    assert(pulse_stream_lifecycle_record(&inventory, 99, NULL) == 0);

    const audio_stream_t *stream = audio_stream_inventory_find(&inventory, 99);
    assert(stream != NULL);
    assert(stream->application_name == NULL);
    assert(stream->process_binary == NULL);

    audio_stream_inventory_clear(&inventory);
}

int main(void) {
    test_initial_snapshot_copies_pulse_properties();
    test_snapshot_and_events_upsert_the_same_stream();
    test_quickly_removed_unassigned_stream_does_not_remain();
    test_missing_proplist_is_recorded_with_null_properties();

    printf("pulse_stream_lifecycle tests passed\n");
    return 0;
}
