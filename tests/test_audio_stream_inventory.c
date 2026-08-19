#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "audio_stream_inventory.h"

static void test_init_and_find_empty_inventory(void) {
    audio_stream_inventory_t inventory;

    audio_stream_inventory_init(&inventory);

    assert(inventory.streams == NULL);
    assert(inventory.count == 0);
    assert(inventory.capacity == 0);
    assert(audio_stream_inventory_find(&inventory, 1) == NULL);

    audio_stream_inventory_clear(&inventory);
}

static void test_null_inventory_contract(void) {
    audio_stream_inventory_init(NULL);
    assert(audio_stream_inventory_find(NULL, 1) == NULL);
    assert(audio_stream_inventory_upsert(NULL, 1, "Application", "binary") == -1);
    assert(audio_stream_inventory_remove(NULL, 1) == 0);
    audio_stream_inventory_clear(NULL);
}

static void test_upsert_adds_and_updates_owned_strings(void) {
    audio_stream_inventory_t inventory;
    char application_name[] = "Firefox";
    char process_binary[] = "firefox";

    audio_stream_inventory_init(&inventory);

    assert(audio_stream_inventory_upsert(
               &inventory, 42, application_name, process_binary) == 0);
    assert(inventory.count == 1);

    application_name[0] = 'X';
    process_binary[0] = 'X';

    const audio_stream_t *stream = audio_stream_inventory_find(&inventory, 42);
    assert(stream != NULL);
    assert(strcmp(stream->application_name, "Firefox") == 0);
    assert(strcmp(stream->process_binary, "firefox") == 0);

    assert(audio_stream_inventory_upsert(
               &inventory, 42, "Discord", "Discord") == 0);
    assert(inventory.count == 1);

    stream = audio_stream_inventory_find(&inventory, 42);
    assert(stream != NULL);
    assert(strcmp(stream->application_name, "Discord") == 0);
    assert(strcmp(stream->process_binary, "Discord") == 0);

    assert(audio_stream_inventory_upsert(
               &inventory, 42, NULL, "discord-bin") == 0);
    assert(inventory.count == 1);

    stream = audio_stream_inventory_find(&inventory, 42);
    assert(stream != NULL);
    assert(stream->application_name == NULL);
    assert(strcmp(stream->process_binary, "discord-bin") == 0);

    audio_stream_inventory_clear(&inventory);
}

static void test_null_properties_are_supported(void) {
    audio_stream_inventory_t inventory;

    audio_stream_inventory_init(&inventory);

    assert(audio_stream_inventory_upsert(&inventory, 7, NULL, NULL) == 0);

    const audio_stream_t *stream = audio_stream_inventory_find(&inventory, 7);
    assert(stream != NULL);
    assert(stream->application_name == NULL);
    assert(stream->process_binary == NULL);

    assert(audio_stream_inventory_upsert(
               &inventory, 7, "Music Player", NULL) == 0);
    stream = audio_stream_inventory_find(&inventory, 7);
    assert(stream != NULL);
    assert(strcmp(stream->application_name, "Music Player") == 0);
    assert(stream->process_binary == NULL);

    audio_stream_inventory_clear(&inventory);
}

static void test_inventory_grows(void) {
    audio_stream_inventory_t inventory;

    audio_stream_inventory_init(&inventory);

    for (uint32_t index = 0; index < 20; index++) {
        assert(audio_stream_inventory_upsert(
                   &inventory, index, "Test application", "test-binary") == 0);
    }

    assert(inventory.count == 20);
    assert(inventory.capacity >= inventory.count);
    for (uint32_t index = 0; index < 20; index++) {
        assert(audio_stream_inventory_find(&inventory, index) != NULL);
    }

    audio_stream_inventory_clear(&inventory);
}

static void test_remove_releases_entry_and_preserves_others(void) {
    audio_stream_inventory_t inventory;

    audio_stream_inventory_init(&inventory);
    assert(audio_stream_inventory_upsert(&inventory, 10, "One", "one") == 0);
    assert(audio_stream_inventory_upsert(&inventory, 20, "Two", "two") == 0);
    assert(audio_stream_inventory_upsert(&inventory, 30, "Three", "three") == 0);

    assert(audio_stream_inventory_remove(&inventory, 20) == 1);
    assert(inventory.count == 2);
    assert(audio_stream_inventory_find(&inventory, 10) != NULL);
    assert(audio_stream_inventory_find(&inventory, 20) == NULL);
    assert(audio_stream_inventory_find(&inventory, 30) != NULL);

    assert(audio_stream_inventory_remove(&inventory, 20) == 0);
    assert(inventory.count == 2);

    audio_stream_inventory_clear(&inventory);
}

static void test_clear_resets_inventory(void) {
    audio_stream_inventory_t inventory;

    audio_stream_inventory_init(&inventory);
    assert(audio_stream_inventory_upsert(
               &inventory, 5, "Cleanup test", "cleanup-test") == 0);

    audio_stream_inventory_clear(&inventory);

    assert(inventory.streams == NULL);
    assert(inventory.count == 0);
    assert(inventory.capacity == 0);

    audio_stream_inventory_clear(&inventory);
}

int main(void) {
    test_init_and_find_empty_inventory();
    test_null_inventory_contract();
    test_upsert_adds_and_updates_owned_strings();
    test_null_properties_are_supported();
    test_inventory_grows();
    test_remove_releases_entry_and_preserves_others();
    test_clear_resets_inventory();

    printf("audio_stream_inventory tests passed\n");
    return 0;
}
