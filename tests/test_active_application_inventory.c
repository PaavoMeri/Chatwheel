#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "active_application_inventory.h"

static void add_stream(audio_stream_inventory_t *streams,
                       uint32_t index,
                       const char *application_id,
                       const char *application_name,
                       const char *process_binary,
                       const char *node_name) {
    assert(audio_stream_inventory_upsert(
               streams,
               index,
               application_id,
               application_name,
               process_binary,
               node_name) == 0);
}

static int has_stream_index(const active_application_t *application,
                            uint32_t stream_index) {
    for (size_t i = 0; i < application->stream_count; i++) {
        if (application->stream_indexes[i] == stream_index) return 1;
    }
    return 0;
}

static const active_application_t *find_application(
    const active_application_inventory_t *inventory,
    application_identity_property_t property,
    const char *value) {
    const active_application_t *application =
        active_application_inventory_find(inventory, property, value);
    assert(application != NULL);
    return application;
}

static void test_empty_stream_inventory_builds_empty_application_inventory(void) {
    audio_stream_inventory_t streams;
    active_application_inventory_t applications;
    audio_stream_inventory_init(&streams);
    active_application_inventory_init(&applications);

    assert(active_application_inventory_rebuild(&applications, &streams) == 0);
    assert(applications.applications == NULL);
    assert(applications.count == 0);
    assert(applications.capacity == 0);
    assert(active_application_inventory_get(&applications, 0) == NULL);

    active_application_inventory_clear(&applications);
    audio_stream_inventory_clear(&streams);
}

static void test_one_stream_creates_owned_application(void) {
    audio_stream_inventory_t streams;
    active_application_inventory_t applications;
    audio_stream_inventory_init(&streams);
    active_application_inventory_init(&applications);

    add_stream(
        &streams,
        7,
        "org.example.Player",
        "Example Player",
        "example-player",
        "example-node");
    assert(active_application_inventory_rebuild(&applications, &streams) == 0);

    assert(applications.count == 1);
    const active_application_t *application = find_application(
        &applications,
        APPLICATION_IDENTITY_PROPERTY_APPLICATION_ID,
        "org.example.Player");
    assert(strcmp(application->identity_value, "org.example.Player") == 0);
    assert(strcmp(application->display_name, "Example Player") == 0);
    assert(application->stream_count == 1);
    assert(application->stream_indexes[0] == 7);

    audio_stream_inventory_clear(&streams);
    assert(strcmp(application->identity_value, "org.example.Player") == 0);
    assert(strcmp(application->display_name, "Example Player") == 0);
    assert(application->stream_indexes[0] == 7);

    active_application_inventory_clear(&applications);
}

static void test_get_returns_borrowed_application(void) {
    audio_stream_inventory_t streams;
    active_application_inventory_t applications;
    audio_stream_inventory_init(&streams);
    active_application_inventory_init(&applications);

    add_stream(
        &streams,
        8,
        "org.example.GetTest",
        "Get Test",
        "get-test",
        "get-test-node");
    assert(active_application_inventory_rebuild(&applications, &streams) == 0);

    const active_application_t *application =
        active_application_inventory_get(&applications, 0);
    assert(application != NULL);
    assert(application->identity_property ==
           APPLICATION_IDENTITY_PROPERTY_APPLICATION_ID);
    assert(strcmp(application->identity_value, "org.example.GetTest") == 0);
    assert(strcmp(application->display_name, "Get Test") == 0);
    assert(application->stream_count == 1);
    assert(application->stream_indexes[0] == 8);
    assert(active_application_inventory_get(&applications, 1) == NULL);

    active_application_inventory_clear(&applications);
    audio_stream_inventory_clear(&streams);
}

static void test_torchlight_streams_aggregate(void) {
    audio_stream_inventory_t streams;
    active_application_inventory_t applications;
    audio_stream_inventory_init(&streams);
    active_application_inventory_init(&applications);

    add_stream(&streams, 10, NULL, "Torchlight II", "wine64-preloader", NULL);
    add_stream(&streams, 11, NULL, "Torchlight II", "wine64-preloader", NULL);
    assert(active_application_inventory_rebuild(&applications, &streams) == 0);

    assert(applications.count == 1);
    const active_application_t *torchlight = find_application(
        &applications,
        APPLICATION_IDENTITY_PROPERTY_APPLICATION_NAME,
        "Torchlight II");
    assert(strcmp(torchlight->display_name, "Torchlight II") == 0);
    assert(torchlight->stream_count == 2);
    assert(has_stream_index(torchlight, 10));
    assert(has_stream_index(torchlight, 11));

    active_application_inventory_clear(&applications);
    audio_stream_inventory_clear(&streams);
}

static void test_proton_games_with_shared_binary_remain_separate(void) {
    audio_stream_inventory_t streams;
    active_application_inventory_t applications;
    audio_stream_inventory_init(&streams);
    active_application_inventory_init(&applications);

    add_stream(&streams, 20, NULL, "Torchlight II", "wine64-preloader", NULL);
    add_stream(&streams, 21, NULL, "Deadlock", "wine64-preloader", NULL);
    assert(active_application_inventory_rebuild(&applications, &streams) == 0);

    assert(applications.count == 2);
    assert(find_application(
               &applications,
               APPLICATION_IDENTITY_PROPERTY_APPLICATION_NAME,
               "Torchlight II")->stream_count == 1);
    assert(find_application(
               &applications,
               APPLICATION_IDENTITY_PROPERTY_APPLICATION_NAME,
               "Deadlock")->stream_count == 1);

    active_application_inventory_clear(&applications);
    audio_stream_inventory_clear(&streams);
}

static void test_native_application_aggregates_by_application_id(void) {
    audio_stream_inventory_t streams;
    active_application_inventory_t applications;
    audio_stream_inventory_init(&streams);
    active_application_inventory_init(&applications);

    for (uint32_t index = 30; index < 36; index++) {
        add_stream(
            &streams,
            index,
            "com.discordapp.Discord",
            "Discord",
            "Discord",
            "discord-node");
    }
    assert(active_application_inventory_rebuild(&applications, &streams) == 0);

    assert(applications.count == 1);
    const active_application_t *discord = find_application(
        &applications,
        APPLICATION_IDENTITY_PROPERTY_APPLICATION_ID,
        "com.discordapp.Discord");
    assert(discord->identity_property ==
           APPLICATION_IDENTITY_PROPERTY_APPLICATION_ID);
    assert(strcmp(discord->display_name, "Discord") == 0);
    assert(discord->stream_count == 6);
    assert(discord->stream_capacity >= discord->stream_count);
    for (uint32_t index = 30; index < 36; index++) {
        assert(has_stream_index(discord, index));
    }

    active_application_inventory_clear(&applications);
    audio_stream_inventory_clear(&streams);
}

static void test_minecraft_falls_back_to_node_name(void) {
    audio_stream_inventory_t streams;
    active_application_inventory_t applications;
    audio_stream_inventory_init(&streams);
    active_application_inventory_init(&applications);

    add_stream(&streams, 40, NULL, NULL, NULL, "java");
    assert(active_application_inventory_rebuild(&applications, &streams) == 0);

    const active_application_t *minecraft = find_application(
        &applications,
        APPLICATION_IDENTITY_PROPERTY_NODE_NAME,
        "java");
    assert(strcmp(minecraft->display_name, "java") == 0);
    assert(minecraft->stream_count == 1);
    assert(minecraft->stream_indexes[0] == 40);

    active_application_inventory_clear(&applications);
    audio_stream_inventory_clear(&streams);
}

static void test_equal_text_from_different_properties_does_not_merge(void) {
    audio_stream_inventory_t streams;
    active_application_inventory_t applications;
    audio_stream_inventory_init(&streams);
    active_application_inventory_init(&applications);

    add_stream(&streams, 50, "shared", NULL, NULL, NULL);
    add_stream(&streams, 51, NULL, "shared", NULL, NULL);
    assert(active_application_inventory_rebuild(&applications, &streams) == 0);

    assert(applications.count == 2);
    assert(find_application(
               &applications,
               APPLICATION_IDENTITY_PROPERTY_APPLICATION_ID,
               "shared")->stream_indexes[0] == 50);
    assert(find_application(
               &applications,
               APPLICATION_IDENTITY_PROPERTY_APPLICATION_NAME,
               "shared")->stream_indexes[0] == 51);

    active_application_inventory_clear(&applications);
    audio_stream_inventory_clear(&streams);
}

static void test_first_grouped_stream_supplies_display_name(void) {
    audio_stream_inventory_t streams;
    active_application_inventory_t applications;
    audio_stream_inventory_init(&streams);
    active_application_inventory_init(&applications);

    add_stream(
        &streams,
        52,
        "org.example.Shared",
        "First Display Name",
        NULL,
        NULL);
    add_stream(
        &streams,
        53,
        "org.example.Shared",
        "Second Display Name",
        NULL,
        NULL);
    assert(active_application_inventory_rebuild(&applications, &streams) == 0);

    assert(applications.count == 1);
    const active_application_t *application = find_application(
        &applications,
        APPLICATION_IDENTITY_PROPERTY_APPLICATION_ID,
        "org.example.Shared");
    assert(strcmp(application->display_name, "First Display Name") == 0);
    assert(application->stream_count == 2);
    assert(application->stream_indexes[0] == 52);
    assert(application->stream_indexes[1] == 53);

    active_application_inventory_clear(&applications);
    audio_stream_inventory_clear(&streams);
}

static void test_duplicate_stream_index_is_not_added_twice(void) {
    audio_stream_inventory_t streams;
    active_application_inventory_t applications;
    audio_stream_inventory_init(&streams);
    active_application_inventory_init(&applications);

    add_stream(&streams, 60, NULL, "Duplicate Test", NULL, NULL);
    add_stream(&streams, 61, NULL, "Duplicate Test", NULL, NULL);
    streams.streams[1].index = 60;
    assert(active_application_inventory_rebuild(&applications, &streams) == 0);

    const active_application_t *application = find_application(
        &applications,
        APPLICATION_IDENTITY_PROPERTY_APPLICATION_NAME,
        "Duplicate Test");
    assert(application->stream_count == 1);
    assert(application->stream_indexes[0] == 60);

    active_application_inventory_clear(&applications);
    audio_stream_inventory_clear(&streams);
}

static void test_streams_without_identity_are_skipped(void) {
    audio_stream_inventory_t streams;
    active_application_inventory_t applications;
    audio_stream_inventory_init(&streams);
    active_application_inventory_init(&applications);

    add_stream(&streams, 70, NULL, NULL, NULL, NULL);
    add_stream(&streams, 71, "", "", "", "");
    assert(active_application_inventory_rebuild(&applications, &streams) == 0);
    assert(applications.count == 0);

    active_application_inventory_clear(&applications);
    audio_stream_inventory_clear(&streams);
}

static void test_application_array_grows(void) {
    audio_stream_inventory_t streams;
    active_application_inventory_t applications;
    audio_stream_inventory_init(&streams);
    active_application_inventory_init(&applications);

    for (uint32_t index = 0; index < 6; index++) {
        char application_name[32];
        int length = snprintf(
            application_name,
            sizeof(application_name),
            "Application %u",
            index);
        assert(length > 0 && (size_t)length < sizeof(application_name));
        add_stream(&streams, 80 + index, NULL, application_name, NULL, NULL);
    }
    assert(active_application_inventory_rebuild(&applications, &streams) == 0);
    assert(applications.count == 6);
    assert(applications.capacity >= applications.count);
    for (uint32_t index = 0; index < 6; index++) {
        char expected_name[32];
        int length = snprintf(
            expected_name,
            sizeof(expected_name),
            "Application %u",
            index);
        assert(length > 0 && (size_t)length < sizeof(expected_name));

        const active_application_t *application =
            active_application_inventory_get(&applications, index);
        assert(application != NULL);
        assert(application->identity_property ==
               APPLICATION_IDENTITY_PROPERTY_APPLICATION_NAME);
        assert(strcmp(application->identity_value, expected_name) == 0);
        assert(strcmp(application->display_name, expected_name) == 0);
        assert(application->stream_count == 1);
        assert(application->stream_indexes[0] == 80 + index);
    }

    active_application_inventory_clear(&applications);
    audio_stream_inventory_clear(&streams);
}

static void test_rebuild_replaces_old_state_and_handles_invalid_source(void) {
    audio_stream_inventory_t streams;
    active_application_inventory_t applications;
    audio_stream_inventory_init(&streams);
    active_application_inventory_init(&applications);

    add_stream(&streams, 90, NULL, "Old Application", NULL, NULL);
    assert(active_application_inventory_rebuild(&applications, &streams) == 0);
    assert(applications.count == 1);

    assert(active_application_inventory_rebuild(&applications, NULL) == -1);
    assert(find_application(
               &applications,
               APPLICATION_IDENTITY_PROPERTY_APPLICATION_NAME,
               "Old Application")->stream_indexes[0] == 90);

    audio_stream_inventory_clear(&streams);
    audio_stream_inventory_init(&streams);
    add_stream(&streams, 91, NULL, "New Application", NULL, NULL);
    assert(active_application_inventory_rebuild(&applications, &streams) == 0);
    assert(applications.count == 1);
    assert(active_application_inventory_find(
               &applications,
               APPLICATION_IDENTITY_PROPERTY_APPLICATION_NAME,
               "Old Application") == NULL);
    assert(find_application(
               &applications,
               APPLICATION_IDENTITY_PROPERTY_APPLICATION_NAME,
               "New Application")->stream_indexes[0] == 91);

    audio_stream_inventory_clear(&streams);
    audio_stream_inventory_init(&streams);
    assert(active_application_inventory_rebuild(&applications, &streams) == 0);
    assert(applications.count == 0);
    assert(active_application_inventory_rebuild(NULL, &streams) == -1);

    active_application_inventory_clear(&applications);
    audio_stream_inventory_clear(&streams);
}

static void test_cleanup_and_null_contracts(void) {
    audio_stream_inventory_t streams;
    active_application_inventory_t applications;
    audio_stream_inventory_init(&streams);
    active_application_inventory_init(&applications);

    add_stream(&streams, 100, "org.example.Cleanup", "Cleanup", NULL, NULL);
    assert(active_application_inventory_rebuild(&applications, &streams) == 0);

    size_t count_before_find = applications.count;
    assert(active_application_inventory_find(
               &applications,
               (application_identity_property_t)999,
               "org.example.Cleanup") == NULL);
    assert(applications.count == count_before_find);
    const active_application_t *unchanged = find_application(
        &applications,
        APPLICATION_IDENTITY_PROPERTY_APPLICATION_ID,
        "org.example.Cleanup");
    assert(strcmp(unchanged->display_name, "Cleanup") == 0);
    assert(unchanged->stream_count == 1);
    assert(unchanged->stream_indexes[0] == 100);

    active_application_inventory_clear(&applications);
    assert(applications.applications == NULL);
    assert(applications.count == 0);
    assert(applications.capacity == 0);
    active_application_inventory_clear(&applications);
    active_application_inventory_clear(NULL);
    active_application_inventory_init(NULL);
    assert(active_application_inventory_get(NULL, 0) == NULL);
    assert(active_application_inventory_find(
               NULL,
               APPLICATION_IDENTITY_PROPERTY_APPLICATION_ID,
               "org.example.Cleanup") == NULL);
    assert(active_application_inventory_find(
               &applications,
               APPLICATION_IDENTITY_PROPERTY_NONE,
               "org.example.Cleanup") == NULL);
    assert(active_application_inventory_find(
               &applications,
               APPLICATION_IDENTITY_PROPERTY_APPLICATION_ID,
               NULL) == NULL);

    audio_stream_inventory_clear(&streams);
}

int main(void) {
    test_empty_stream_inventory_builds_empty_application_inventory();
    test_one_stream_creates_owned_application();
    test_get_returns_borrowed_application();
    test_torchlight_streams_aggregate();
    test_proton_games_with_shared_binary_remain_separate();
    test_native_application_aggregates_by_application_id();
    test_minecraft_falls_back_to_node_name();
    test_equal_text_from_different_properties_does_not_merge();
    test_first_grouped_stream_supplies_display_name();
    test_duplicate_stream_index_is_not_added_twice();
    test_streams_without_identity_are_skipped();
    test_application_array_grows();
    test_rebuild_replaces_old_state_and_handles_invalid_source();
    test_cleanup_and_null_contracts();

    printf("active_application_inventory tests passed\n");
    return 0;
}
