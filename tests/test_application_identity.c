#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "application_identity.h"

static void assert_resolution(
    application_identity_resolution_t resolution,
    application_identity_property_t expected_property,
    const char *expected_value) {
    assert(resolution.property == expected_property);
    assert(resolution.value == expected_value);
}

static void test_identity_uses_each_fallback_level(void) {
    audio_stream_t stream = {
        .application_id = "org.example.Player",
        .application_name = "Example Player",
        .process_binary = "example-player",
        .node_name = "example-node",
    };

    assert_resolution(
        application_identity_resolve(&stream),
        APPLICATION_IDENTITY_PROPERTY_APPLICATION_ID,
        stream.application_id);

    stream.application_id = NULL;
    assert_resolution(
        application_identity_resolve(&stream),
        APPLICATION_IDENTITY_PROPERTY_APPLICATION_NAME,
        stream.application_name);

    stream.application_name = NULL;
    assert_resolution(
        application_identity_resolve(&stream),
        APPLICATION_IDENTITY_PROPERTY_PROCESS_BINARY,
        stream.process_binary);

    stream.process_binary = NULL;
    assert_resolution(
        application_identity_resolve(&stream),
        APPLICATION_IDENTITY_PROPERTY_NODE_NAME,
        stream.node_name);
}

static void test_identity_ignores_empty_strings(void) {
    audio_stream_t stream = {
        .application_id = "",
        .application_name = "",
        .process_binary = "",
        .node_name = "fallback-node",
    };

    assert_resolution(
        application_identity_resolve(&stream),
        APPLICATION_IDENTITY_PROPERTY_NODE_NAME,
        stream.node_name);

    stream.node_name = "";
    assert_resolution(
        application_identity_resolve(&stream),
        APPLICATION_IDENTITY_PROPERTY_NONE,
        NULL);
}

static void test_identity_handles_all_missing_input(void) {
    audio_stream_t stream = {0};

    assert_resolution(
        application_identity_resolve(&stream),
        APPLICATION_IDENTITY_PROPERTY_NONE,
        NULL);
    assert_resolution(
        application_identity_resolve(NULL),
        APPLICATION_IDENTITY_PROPERTY_NONE,
        NULL);
}

static void test_proton_streams_use_distinct_application_names(void) {
    audio_stream_t first_game = {
        .application_name = "Game One",
        .process_binary = "wine64-preloader",
    };
    audio_stream_t second_game = {
        .application_name = "Game Two",
        .process_binary = "wine64-preloader",
    };

    application_identity_resolution_t first =
        application_identity_resolve(&first_game);
    application_identity_resolution_t second =
        application_identity_resolve(&second_game);

    assert_resolution(
        first,
        APPLICATION_IDENTITY_PROPERTY_APPLICATION_NAME,
        first_game.application_name);
    assert_resolution(
        second,
        APPLICATION_IDENTITY_PROPERTY_APPLICATION_NAME,
        second_game.application_name);
    assert(strcmp(first.value, second.value) != 0);
}

static void test_minecraft_stream_uses_node_name(void) {
    audio_stream_t stream = {
        .node_name = "java",
    };

    assert_resolution(
        application_identity_resolve(&stream),
        APPLICATION_IDENTITY_PROPERTY_NODE_NAME,
        stream.node_name);
}

static void test_display_name_uses_its_own_fallback_order(void) {
    audio_stream_t stream = {
        .application_id = "org.example.Player",
        .application_name = "Example Player",
        .process_binary = "example-player",
        .node_name = "example-node",
    };

    assert_resolution(
        application_display_name_resolve(&stream),
        APPLICATION_IDENTITY_PROPERTY_APPLICATION_NAME,
        stream.application_name);

    stream.application_name = "";
    assert_resolution(
        application_display_name_resolve(&stream),
        APPLICATION_IDENTITY_PROPERTY_NODE_NAME,
        stream.node_name);

    stream.node_name = NULL;
    assert_resolution(
        application_display_name_resolve(&stream),
        APPLICATION_IDENTITY_PROPERTY_PROCESS_BINARY,
        stream.process_binary);

    stream.process_binary = "";
    assert_resolution(
        application_display_name_resolve(&stream),
        APPLICATION_IDENTITY_PROPERTY_APPLICATION_ID,
        stream.application_id);

    stream.application_id = "";
    assert_resolution(
        application_display_name_resolve(&stream),
        APPLICATION_IDENTITY_PROPERTY_NONE,
        NULL);
    assert_resolution(
        application_display_name_resolve(NULL),
        APPLICATION_IDENTITY_PROPERTY_NONE,
        NULL);
}

int main(void) {
    test_identity_uses_each_fallback_level();
    test_identity_ignores_empty_strings();
    test_identity_handles_all_missing_input();
    test_proton_streams_use_distinct_application_names();
    test_minecraft_stream_uses_node_name();
    test_display_name_uses_its_own_fallback_order();

    printf("application_identity tests passed\n");
    return 0;
}
