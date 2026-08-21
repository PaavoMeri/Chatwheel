#include "application_classifier.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    audio_stream_inventory_t streams;
    active_application_inventory_t applications;
} classifier_fixture_t;

static void fixture_init(classifier_fixture_t *fixture) {
    audio_stream_inventory_init(&fixture->streams);
    active_application_inventory_init(&fixture->applications);
}

static void fixture_clear(classifier_fixture_t *fixture) {
    active_application_inventory_clear(&fixture->applications);
    audio_stream_inventory_clear(&fixture->streams);
}

static void fixture_add_stream(classifier_fixture_t *fixture,
                               uint32_t index,
                               const char *application_id,
                               const char *application_name,
                               const char *process_binary,
                               const char *node_name) {
    assert(audio_stream_inventory_upsert(
        &fixture->streams,
        index,
        application_id,
        application_name,
        process_binary,
        node_name) == 0);
}

static void fixture_rebuild_applications(classifier_fixture_t *fixture) {
    assert(active_application_inventory_rebuild(
        &fixture->applications,
        &fixture->streams) == 0);
}

static void config_add(config_t *configuration,
                       const char *pattern,
                       int is_chat) {
    assert(configuration->count >= 0);
    assert(configuration->count < MAX_APPS);
    size_t pattern_length = strlen(pattern);
    assert(pattern_length < sizeof(configuration->apps[0].name));

    app_config_t *entry = &configuration->apps[configuration->count];
    memcpy(entry->name, pattern, pattern_length + 1);
    entry->is_chat = is_chat;
    configuration->count++;
}

static const active_application_t *find_application(
    const classifier_fixture_t *fixture,
    application_identity_property_t property,
    const char *identity_value) {
    const active_application_t *application =
        active_application_inventory_find(
            &fixture->applications,
            property,
            identity_value);
    assert(application != NULL);
    return application;
}

static void expect_classification(
    const active_application_t *application,
    const audio_stream_inventory_t *streams,
    const config_t *configuration,
    application_group_t expected_group,
    int expected_config_index) {
    application_classification_t result = application_classifier_classify(
        application,
        streams,
        configuration);
    assert(result.group == expected_group);
    assert(result.matched_config_index == expected_config_index);
}

static void test_invalid_inputs_and_empty_config(void) {
    classifier_fixture_t fixture;
    fixture_init(&fixture);
    fixture_add_stream(&fixture, 1, NULL, "Discord", "discord", NULL);
    fixture_rebuild_applications(&fixture);
    const active_application_t *application = find_application(
        &fixture,
        APPLICATION_IDENTITY_PROPERTY_APPLICATION_NAME,
        "Discord");
    config_t configuration = {0};

    expect_classification(NULL,
                          &fixture.streams,
                          &configuration,
                          APPLICATION_GROUP_UNASSIGNED,
                          -1);
    expect_classification(application,
                          NULL,
                          &configuration,
                          APPLICATION_GROUP_UNASSIGNED,
                          -1);
    expect_classification(application,
                          &fixture.streams,
                          NULL,
                          APPLICATION_GROUP_UNASSIGNED,
                          -1);
    expect_classification(application,
                          &fixture.streams,
                          &configuration,
                          APPLICATION_GROUP_UNASSIGNED,
                          -1);

    configuration.count = -1;
    expect_classification(application,
                          &fixture.streams,
                          &configuration,
                          APPLICATION_GROUP_UNASSIGNED,
                          -1);
    configuration.count = MAX_APPS + 1;
    expect_classification(application,
                          &fixture.streams,
                          &configuration,
                          APPLICATION_GROUP_UNASSIGNED,
                          -1);

    fixture_clear(&fixture);
}

static void test_malformed_inventory_structures(void) {
    classifier_fixture_t fixture;
    fixture_init(&fixture);
    fixture_add_stream(&fixture, 2, NULL, "Discord", "discord", NULL);
    fixture_rebuild_applications(&fixture);
    const active_application_t *application = find_application(
        &fixture,
        APPLICATION_IDENTITY_PROPERTY_APPLICATION_NAME,
        "Discord");
    config_t configuration = {0};
    config_add(&configuration, "Discord", 0);

    audio_stream_inventory_t malformed_streams = fixture.streams;
    malformed_streams.count = malformed_streams.capacity + 1;
    expect_classification(application,
                          &malformed_streams,
                          &configuration,
                          APPLICATION_GROUP_UNASSIGNED,
                          -1);

    malformed_streams = (audio_stream_inventory_t){
        .streams = NULL,
        .count = 1,
        .capacity = 1,
    };
    expect_classification(application,
                          &malformed_streams,
                          &configuration,
                          APPLICATION_GROUP_UNASSIGNED,
                          -1);

    active_application_t malformed_application = *application;
    malformed_application.stream_count =
        malformed_application.stream_capacity + 1;
    expect_classification(&malformed_application,
                          &fixture.streams,
                          &configuration,
                          APPLICATION_GROUP_UNASSIGNED,
                          -1);

    malformed_application = *application;
    malformed_application.stream_indexes = NULL;
    malformed_application.stream_count = 1;
    expect_classification(&malformed_application,
                          &fixture.streams,
                          &configuration,
                          APPLICATION_GROUP_UNASSIGNED,
                          -1);

    fixture_clear(&fixture);
}

static void test_zero_stream_application(void) {
    audio_stream_inventory_t streams;
    audio_stream_inventory_init(&streams);
    active_application_t application = {0};
    config_t configuration = {0};
    config_add(&configuration, "*", 1);

    expect_classification(&application,
                          &streams,
                          &configuration,
                          APPLICATION_GROUP_UNASSIGNED,
                          -1);

    audio_stream_inventory_clear(&streams);
}

static void test_empty_property_matching(void) {
    classifier_fixture_t fixture;
    fixture_init(&fixture);
    fixture_add_stream(
        &fixture, 3, NULL, "Temporary Identity", NULL, NULL);
    fixture_rebuild_applications(&fixture);
    const active_application_t *application = find_application(
        &fixture,
        APPLICATION_IDENTITY_PROPERTY_APPLICATION_NAME,
        "Temporary Identity");
    config_t configuration = {0};

    config_add(&configuration, "", 0);
    expect_classification(application,
                          &fixture.streams,
                          &configuration,
                          APPLICATION_GROUP_UNASSIGNED,
                          -1);

    assert(audio_stream_inventory_upsert(
        &fixture.streams,
        3,
        NULL,
        "",
        NULL,
        NULL) == 0);
    expect_classification(application,
                          &fixture.streams,
                          &configuration,
                          APPLICATION_GROUP_GAME,
                          0);

    configuration = (config_t){0};
    config_add(&configuration, "*", 1);
    expect_classification(application,
                          &fixture.streams,
                          &configuration,
                          APPLICATION_GROUP_CHAT,
                          0);

    assert(audio_stream_inventory_upsert(
        &fixture.streams,
        3,
        NULL,
        NULL,
        NULL,
        NULL) == 0);

    configuration = (config_t){0};
    config_add(&configuration, "", 0);
    expect_classification(application,
                          &fixture.streams,
                          &configuration,
                          APPLICATION_GROUP_UNASSIGNED,
                          -1);

    configuration = (config_t){0};
    config_add(&configuration, "*", 1);
    expect_classification(application,
                          &fixture.streams,
                          &configuration,
                          APPLICATION_GROUP_UNASSIGNED,
                          -1);

    fixture_clear(&fixture);
}

static void test_group_mapping_and_match_styles(void) {
    classifier_fixture_t fixture;
    fixture_init(&fixture);
    fixture_add_stream(&fixture, 10, NULL, "Discord", "discord", NULL);
    fixture_rebuild_applications(&fixture);
    const active_application_t *application = find_application(
        &fixture,
        APPLICATION_IDENTITY_PROPERTY_APPLICATION_NAME,
        "Discord");
    config_t configuration = {0};

    config_add(&configuration, "Discord", 0);
    expect_classification(application,
                          &fixture.streams,
                          &configuration,
                          APPLICATION_GROUP_GAME,
                          0);

    configuration = (config_t){0};
    config_add(&configuration, "Discord", 1);
    expect_classification(application,
                          &fixture.streams,
                          &configuration,
                          APPLICATION_GROUP_CHAT,
                          0);

    configuration = (config_t){0};
    config_add(&configuration, "Discord", -7);
    expect_classification(application,
                          &fixture.streams,
                          &configuration,
                          APPLICATION_GROUP_CHAT,
                          0);

    configuration = (config_t){0};
    config_add(&configuration, "dIsCoRd", 0);
    expect_classification(application,
                          &fixture.streams,
                          &configuration,
                          APPLICATION_GROUP_GAME,
                          0);

    configuration = (config_t){0};
    config_add(&configuration, "Dis*", 1);
    expect_classification(application,
                          &fixture.streams,
                          &configuration,
                          APPLICATION_GROUP_CHAT,
                          0);

    configuration = (config_t){0};
    config_add(&configuration, "D?scord", 0);
    expect_classification(application,
                          &fixture.streams,
                          &configuration,
                          APPLICATION_GROUP_GAME,
                          0);

    fixture_clear(&fixture);
}

static void test_property_matching_and_unmatched(void) {
    classifier_fixture_t fixture;
    fixture_init(&fixture);
    fixture_add_stream(&fixture,
                       20,
                       "org.example.Torchlight",
                       "Torchlight: Infinite",
                       "wine64-preloader",
                       "torchlight-node");
    fixture_rebuild_applications(&fixture);
    const active_application_t *application = find_application(
        &fixture,
        APPLICATION_IDENTITY_PROPERTY_APPLICATION_ID,
        "org.example.Torchlight");
    config_t configuration = {0};

    config_add(&configuration, "org.example.Torchlight", 0);
    expect_classification(application,
                          &fixture.streams,
                          &configuration,
                          APPLICATION_GROUP_GAME,
                          0);

    configuration = (config_t){0};
    config_add(&configuration, "Torchlight: Infinite", 1);
    expect_classification(application,
                          &fixture.streams,
                          &configuration,
                          APPLICATION_GROUP_CHAT,
                          0);

    configuration = (config_t){0};
    config_add(&configuration, "wine64-preloader", 0);
    expect_classification(application,
                          &fixture.streams,
                          &configuration,
                          APPLICATION_GROUP_GAME,
                          0);

    configuration = (config_t){0};
    config_add(&configuration, "torchlight-node", 1);
    expect_classification(application,
                          &fixture.streams,
                          &configuration,
                          APPLICATION_GROUP_CHAT,
                          0);

    configuration = (config_t){0};
    config_add(&configuration, "NoSuchApplication", 1);
    expect_classification(application,
                          &fixture.streams,
                          &configuration,
                          APPLICATION_GROUP_UNASSIGNED,
                          -1);

    fixture_clear(&fixture);

    fixture_init(&fixture);
    fixture_add_stream(&fixture, 21, NULL, NULL, NULL, "java");
    fixture_rebuild_applications(&fixture);
    application = find_application(
        &fixture,
        APPLICATION_IDENTITY_PROPERTY_NODE_NAME,
        "java");
    configuration = (config_t){0};
    config_add(&configuration, "java", 0);
    expect_classification(application,
                          &fixture.streams,
                          &configuration,
                          APPLICATION_GROUP_GAME,
                          0);
    fixture_clear(&fixture);
}

static void test_missing_and_later_stream_indexes(void) {
    classifier_fixture_t fixture;
    fixture_init(&fixture);
    fixture_add_stream(&fixture,
                       100,
                       "org.example.Multi",
                       "First Stream",
                       "shared-binary",
                       "first-node");
    fixture_add_stream(&fixture,
                       101,
                       "org.example.Multi",
                       "Second Stream",
                       "shared-binary",
                       "second-node");
    fixture_rebuild_applications(&fixture);
    const active_application_t *application = find_application(
        &fixture,
        APPLICATION_IDENTITY_PROPERTY_APPLICATION_ID,
        "org.example.Multi");
    assert(application->stream_count == 2);
    assert(audio_stream_inventory_remove(&fixture.streams, 100) == 1);

    config_t configuration = {0};
    config_add(&configuration, "First Stream", 0);
    expect_classification(application,
                          &fixture.streams,
                          &configuration,
                          APPLICATION_GROUP_UNASSIGNED,
                          -1);

    configuration = (config_t){0};
    config_add(&configuration, "Second Stream", 1);
    expect_classification(application,
                          &fixture.streams,
                          &configuration,
                          APPLICATION_GROUP_CHAT,
                          0);

    fixture_clear(&fixture);
}

static void test_configuration_precedence(void) {
    classifier_fixture_t fixture;
    fixture_init(&fixture);
    fixture_add_stream(&fixture,
                       200,
                       "org.example.Ordered",
                       "First Stream",
                       "ordered-binary",
                       NULL);
    fixture_add_stream(&fixture,
                       201,
                       "org.example.Ordered",
                       "Second Stream",
                       "ordered-binary",
                       NULL);
    fixture_rebuild_applications(&fixture);
    const active_application_t *application = find_application(
        &fixture,
        APPLICATION_IDENTITY_PROPERTY_APPLICATION_ID,
        "org.example.Ordered");
    config_t configuration = {0};

    config_add(&configuration, "Second Stream", 1);
    config_add(&configuration, "First Stream", 0);
    expect_classification(application,
                          &fixture.streams,
                          &configuration,
                          APPLICATION_GROUP_CHAT,
                          0);

    configuration = (config_t){0};
    config_add(&configuration, "*Stream", 0);
    config_add(&configuration, "First*", 1);
    expect_classification(application,
                          &fixture.streams,
                          &configuration,
                          APPLICATION_GROUP_GAME,
                          0);

    fixture_clear(&fixture);
}

static void test_proton_application_classification(void) {
    classifier_fixture_t fixture;
    fixture_init(&fixture);
    fixture_add_stream(&fixture,
                       300,
                       NULL,
                       "Torchlight: Infinite",
                       "wine64-preloader",
                       NULL);
    fixture_add_stream(&fixture,
                       301,
                       NULL,
                       "Deadlock",
                       "wine64-preloader",
                       NULL);
    fixture_rebuild_applications(&fixture);
    const active_application_t *torchlight = find_application(
        &fixture,
        APPLICATION_IDENTITY_PROPERTY_APPLICATION_NAME,
        "Torchlight: Infinite");
    const active_application_t *deadlock = find_application(
        &fixture,
        APPLICATION_IDENTITY_PROPERTY_APPLICATION_NAME,
        "Deadlock");
    config_t configuration = {0};

    config_add(&configuration, "Torchlight*", 0);
    config_add(&configuration, "Deadlock", 1);
    config_add(&configuration, "wine64-preloader", 1);
    expect_classification(torchlight,
                          &fixture.streams,
                          &configuration,
                          APPLICATION_GROUP_GAME,
                          0);
    expect_classification(deadlock,
                          &fixture.streams,
                          &configuration,
                          APPLICATION_GROUP_CHAT,
                          1);

    configuration = (config_t){0};
    config_add(&configuration, "wine64-preloader", 1);
    config_add(&configuration, "Torchlight*", 0);
    expect_classification(torchlight,
                          &fixture.streams,
                          &configuration,
                          APPLICATION_GROUP_CHAT,
                          0);

    configuration = (config_t){0};
    config_add(&configuration, "Torchlight*", 0);
    config_add(&configuration, "wine64-preloader", 1);
    expect_classification(torchlight,
                          &fixture.streams,
                          &configuration,
                          APPLICATION_GROUP_GAME,
                          0);

    fixture_clear(&fixture);
}

int main(void) {
    test_invalid_inputs_and_empty_config();
    test_malformed_inventory_structures();
    test_zero_stream_application();
    test_empty_property_matching();
    test_group_mapping_and_match_styles();
    test_property_matching_and_unmatched();
    test_missing_and_later_stream_indexes();
    test_configuration_precedence();
    test_proton_application_classification();
    printf("application_classifier tests passed\n");
    return 0;
}
