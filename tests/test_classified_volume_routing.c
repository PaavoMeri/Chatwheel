#include "mixer/classified_volume_routing.h"

#include <assert.h>
#include <pulse/sample.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    audio_stream_inventory_t streams;
    active_application_inventory_t applications;
} routing_fixture_t;

static void fixture_init(routing_fixture_t *fixture) {
    audio_stream_inventory_init(&fixture->streams);
    active_application_inventory_init(&fixture->applications);
}

static void fixture_clear(routing_fixture_t *fixture) {
    active_application_inventory_clear(&fixture->applications);
    audio_stream_inventory_clear(&fixture->streams);
}

static void fixture_add_stream_with_channel_count(
    routing_fixture_t *fixture,
    uint32_t index,
    unsigned int channel_count,
    const char *application_id,
    const char *application_name,
    const char *process_binary,
    const char *node_name) {
    assert(audio_stream_inventory_upsert(
               &fixture->streams,
               index,
               channel_count,
               application_id,
               application_name,
               process_binary,
               node_name) == 0);
}

static void fixture_add_stream(routing_fixture_t *fixture,
                               uint32_t index,
                               const char *application_id,
                               const char *application_name,
                               const char *process_binary,
                               const char *node_name) {
    fixture_add_stream_with_channel_count(
        fixture,
        index,
        2,
        application_id,
        application_name,
        process_binary,
        node_name);
}

static void fixture_rebuild(routing_fixture_t *fixture) {
    assert(active_application_inventory_rebuild(
               &fixture->applications,
               &fixture->streams) == 0);
}

static void config_add(config_t *configuration,
                       const char *pattern,
                       int is_chat) {
    assert(configuration->count >= 0);
    assert(configuration->count < MAX_APPS);
    size_t length = strlen(pattern);
    assert(length < sizeof(configuration->apps[0].name));

    app_config_t *entry = &configuration->apps[configuration->count];
    memcpy(entry->name, pattern, length + 1);
    entry->is_chat = is_chat;
    configuration->count++;
}

static chatmix_volume_targets_t calculate_targets(float raw) {
    chatmix_volume_targets_t targets;
    assert(chatmix_volume_targets_calculate(raw, &targets) == 0);
    return targets;
}

static const classified_volume_assignment_t *find_assignment(
    const classified_volume_plan_t *plan,
    uint32_t stream_index) {
    for (size_t i = 0; i < plan->count; i++) {
        if (plan->assignments[i].stream_index == stream_index) {
            return &plan->assignments[i];
        }
    }
    return NULL;
}

static void expect_assignment(
    const classified_volume_plan_t *plan,
    uint32_t stream_index,
    unsigned int expected_channel_count,
    application_group_t expected_group,
    pa_volume_t expected_volume) {
    const classified_volume_assignment_t *assignment =
        find_assignment(plan, stream_index);
    assert(assignment != NULL);
    assert(assignment->channel_count == expected_channel_count);
    assert(assignment->group == expected_group);
    assert(assignment->pulse_volume == expected_volume);
}

static void expect_assignment_at(
    const classified_volume_plan_t *plan,
    size_t position,
    uint32_t expected_stream_index,
    unsigned int expected_channel_count,
    application_group_t expected_group,
    pa_volume_t expected_volume) {
    assert(position < plan->count);
    const classified_volume_assignment_t *assignment =
        &plan->assignments[position];
    assert(assignment->stream_index == expected_stream_index);
    assert(assignment->channel_count == expected_channel_count);
    assert(assignment->group == expected_group);
    assert(assignment->pulse_volume == expected_volume);
}

static void expect_plan_unchanged(
    const classified_volume_plan_t *plan,
    const classified_volume_assignment_t *expected_assignments,
    size_t expected_count,
    size_t expected_capacity,
    classified_volume_assignment_t expected_first) {
    assert(plan->assignments == expected_assignments);
    assert(plan->count == expected_count);
    assert(plan->capacity == expected_capacity);
    expect_assignment_at(
        plan,
        0,
        expected_first.stream_index,
        expected_first.channel_count,
        expected_first.group,
        expected_first.pulse_volume);
}

static void test_all_groups_and_aggregated_streams(void) {
    routing_fixture_t fixture;
    fixture_init(&fixture);
    fixture_add_stream_with_channel_count(
        &fixture, 1, 1, "org.example.Game", "Example Game", "game", NULL);
    fixture_add_stream_with_channel_count(
        &fixture, 2, 2, "org.example.Game", "Example Game", "game", NULL);
    fixture_add_stream_with_channel_count(
        &fixture, 3, 6, "org.example.Chat", "Example Chat", "chat", NULL);
    fixture_add_stream(
        &fixture, 4, "org.example.Other", "Other", "other", NULL);
    fixture_rebuild(&fixture);

    config_t configuration = {0};
    config_add(&configuration, "org.example.Game", 0);
    config_add(&configuration, "org.example.Chat", 1);
    chatmix_volume_targets_t targets = calculate_targets(32.0f);
    classified_volume_plan_t plan;
    classified_volume_plan_init(&plan);

    assert(classified_volume_plan_build_all(
               &plan,
               &fixture.applications,
               &fixture.streams,
               &configuration,
               &targets,
               1) == 0);
    assert(plan.count == 3);
    assert(plan.assignments[0].stream_index == 1);
    assert(plan.assignments[1].stream_index == 2);
    assert(plan.assignments[2].stream_index == 3);
    expect_assignment(
        &plan, 1, 1, APPLICATION_GROUP_GAME, targets.game.pulse);
    expect_assignment(
        &plan, 2, 2, APPLICATION_GROUP_GAME, targets.game.pulse);
    expect_assignment(
        &plan, 3, 6, APPLICATION_GROUP_CHAT, targets.chat.pulse);
    assert(find_assignment(&plan, 4) == NULL);

    classified_volume_plan_clear(&plan);
    fixture_clear(&fixture);
}

static void test_first_config_entry_wins(void) {
    routing_fixture_t fixture;
    fixture_init(&fixture);
    fixture_add_stream(
        &fixture, 10, NULL, "Discord", "discord", NULL);
    fixture_rebuild(&fixture);

    config_t configuration = {0};
    config_add(&configuration, "*", 0);
    config_add(&configuration, "Discord", 1);
    chatmix_volume_targets_t targets = calculate_targets(24.0f);
    classified_volume_plan_t plan;
    classified_volume_plan_init(&plan);

    assert(classified_volume_plan_build_all(
               &plan,
               &fixture.applications,
               &fixture.streams,
               &configuration,
               &targets,
               1) == 0);
    assert(plan.count == 1);
    expect_assignment(
        &plan, 10, 2, APPLICATION_GROUP_GAME, targets.game.pulse);

    classified_volume_plan_clear(&plan);
    fixture_clear(&fixture);
}

static void test_missing_indexes_and_duplicate_suppression(void) {
    routing_fixture_t fixture;
    fixture_init(&fixture);
    fixture_add_stream(
        &fixture, 20, "org.example.Game", "Game", "game", NULL);
    fixture_add_stream(
        &fixture, 21, "org.example.Chat", "Chat", "chat", NULL);
    fixture_rebuild(&fixture);
    assert(fixture.applications.count == 2);

    active_application_t *game = &fixture.applications.applications[0];
    active_application_t *chat = &fixture.applications.applications[1];
    assert(game->stream_count < game->stream_capacity);
    assert(chat->stream_count < chat->stream_capacity);
    game->stream_indexes[game->stream_count++] = 999;
    chat->stream_indexes[chat->stream_count++] = 20;

    config_t configuration = {0};
    config_add(&configuration, "org.example.Chat", 1);
    config_add(&configuration, "org.example.Game", 0);
    chatmix_volume_targets_t targets = calculate_targets(96.0f);
    classified_volume_plan_t plan;
    classified_volume_plan_init(&plan);

    assert(classified_volume_plan_build_all(
               &plan,
               &fixture.applications,
               &fixture.streams,
               &configuration,
               &targets,
               1) == 0);
    assert(plan.count == 2);
    expect_assignment(
        &plan, 20, 2, APPLICATION_GROUP_GAME, targets.game.pulse);
    expect_assignment(
        &plan, 21, 2, APPLICATION_GROUP_CHAT, targets.chat.pulse);
    assert(find_assignment(&plan, 999) == NULL);

    classified_volume_plan_clear(&plan);
    fixture_clear(&fixture);
}

static void test_new_stream_routes_complete_containing_application(void) {
    routing_fixture_t fixture;
    fixture_init(&fixture);
    fixture_add_stream(
        &fixture, 30, "org.example.Voice", "Voice", "voice", NULL);
    fixture_add_stream(
        &fixture, 31, "org.example.Voice", "Voice", "voice", NULL);
    fixture_add_stream(
        &fixture, 32, "org.example.Game", "Game", "game", NULL);
    fixture_add_stream(
        &fixture, 33, "org.example.Other", "Other", "other", NULL);
    fixture_rebuild(&fixture);

    config_t configuration = {0};
    config_add(&configuration, "org.example.Voice", 1);
    config_add(&configuration, "org.example.Game", 0);
    chatmix_volume_targets_t targets = calculate_targets(80.0f);
    classified_volume_plan_t plan;
    classified_volume_plan_init(&plan);

    assert(classified_volume_plan_build_for_stream(
               &plan,
               &fixture.applications,
               &fixture.streams,
               &configuration,
               &targets,
               1,
               31) == 0);
    assert(plan.count == 2);
    expect_assignment(
        &plan, 30, 2, APPLICATION_GROUP_CHAT, targets.chat.pulse);
    expect_assignment(
        &plan, 31, 2, APPLICATION_GROUP_CHAT, targets.chat.pulse);
    assert(find_assignment(&plan, 32) == NULL);

    assert(classified_volume_plan_build_for_stream(
               &plan,
               &fixture.applications,
               &fixture.streams,
               &configuration,
               &targets,
               1,
               33) == 0);
    assert(plan.count == 0);

    classified_volume_plan_clear(&plan);
    fixture_clear(&fixture);
}

static void test_unavailable_inventory_produces_no_assignments(void) {
    routing_fixture_t fixture;
    fixture_init(&fixture);
    fixture_add_stream(
        &fixture, 40, "org.example.Game", "Game", "game", NULL);
    fixture_rebuild(&fixture);

    config_t configuration = {0};
    config_add(&configuration, "org.example.Game", 0);
    chatmix_volume_targets_t targets = calculate_targets(64.0f);
    classified_volume_plan_t plan;
    classified_volume_plan_init(&plan);

    assert(classified_volume_plan_build_all(
               &plan,
               &fixture.applications,
               &fixture.streams,
               &configuration,
               &targets,
               1) == 0);
    assert(plan.count == 1);

    assert(classified_volume_plan_build_all(
               &plan,
               &fixture.applications,
               &fixture.streams,
               &configuration,
               &targets,
               0) == 0);
    assert(plan.count == 0);

    assert(classified_volume_plan_build_for_stream(
               &plan,
               &fixture.applications,
               &fixture.streams,
               &configuration,
               &targets,
               0,
               40) == 0);
    assert(plan.count == 0);

    classified_volume_plan_clear(&plan);
    fixture_clear(&fixture);
}

static void test_empty_inventories_and_configuration(void) {
    routing_fixture_t fixture;
    fixture_init(&fixture);

    config_t configuration = {0};
    chatmix_volume_targets_t targets = calculate_targets(64.0f);
    classified_volume_plan_t plan;
    classified_volume_plan_init(&plan);

    assert(classified_volume_plan_build_all(
               &plan,
               &fixture.applications,
               &fixture.streams,
               &configuration,
               &targets,
               1) == 0);
    assert(plan.assignments == NULL);
    assert(plan.count == 0);
    assert(plan.capacity == 0);

    assert(classified_volume_plan_build_for_stream(
               &plan,
               &fixture.applications,
               &fixture.streams,
               &configuration,
               &targets,
               1,
               123) == 0);
    assert(plan.assignments == NULL);
    assert(plan.count == 0);
    assert(plan.capacity == 0);

    fixture_add_stream(
        &fixture, 123, "org.example.Game", "Game", "game", NULL);
    fixture_rebuild(&fixture);
    assert(classified_volume_plan_build_all(
               &plan,
               &fixture.applications,
               &fixture.streams,
               &configuration,
               &targets,
               1) == 0);
    assert(plan.assignments == NULL);
    assert(plan.count == 0);
    assert(plan.capacity == 0);

    classified_volume_plan_clear(&plan);
    fixture_clear(&fixture);
}

static void test_invalid_inputs_preserve_populated_plan(void) {
    routing_fixture_t fixture;
    fixture_init(&fixture);
    fixture_add_stream(
        &fixture, 60, "org.example.Game", "Game", "game", NULL);
    fixture_rebuild(&fixture);

    config_t configuration = {0};
    config_add(&configuration, "org.example.Game", 0);
    chatmix_volume_targets_t targets = calculate_targets(48.0f);
    classified_volume_plan_t plan;
    classified_volume_plan_init(&plan);
    assert(classified_volume_plan_build_all(
               &plan,
               &fixture.applications,
               &fixture.streams,
               &configuration,
               &targets,
               1) == 0);
    assert(plan.count == 1);

    classified_volume_assignment_t *expected_assignments = plan.assignments;
    size_t expected_count = plan.count;
    size_t expected_capacity = plan.capacity;
    classified_volume_assignment_t expected_first = plan.assignments[0];

#define EXPECT_PLAN_PRESERVED_AFTER_FAILURE(call) do { \
    assert((call) == -1); \
    expect_plan_unchanged( \
        &plan, \
        expected_assignments, \
        expected_count, \
        expected_capacity, \
        expected_first); \
} while (0)

    assert(classified_volume_plan_build_all(
               NULL,
               &fixture.applications,
               &fixture.streams,
               &configuration,
               &targets,
               1) == -1);
    EXPECT_PLAN_PRESERVED_AFTER_FAILURE(
        classified_volume_plan_build_all(
            &plan, NULL, &fixture.streams, &configuration, &targets, 1));
    EXPECT_PLAN_PRESERVED_AFTER_FAILURE(
        classified_volume_plan_build_all(
            &plan, &fixture.applications, NULL, &configuration, &targets, 1));
    EXPECT_PLAN_PRESERVED_AFTER_FAILURE(
        classified_volume_plan_build_all(
            &plan, &fixture.applications, &fixture.streams, NULL, &targets, 1));
    EXPECT_PLAN_PRESERVED_AFTER_FAILURE(
        classified_volume_plan_build_all(
            &plan,
            &fixture.applications,
            &fixture.streams,
            &configuration,
            NULL,
            1));

    assert(classified_volume_plan_build_for_stream(
               NULL,
               &fixture.applications,
               &fixture.streams,
               &configuration,
               &targets,
               1,
               60) == -1);
    EXPECT_PLAN_PRESERVED_AFTER_FAILURE(
        classified_volume_plan_build_for_stream(
            &plan, NULL, &fixture.streams, &configuration, &targets, 1, 60));
    EXPECT_PLAN_PRESERVED_AFTER_FAILURE(
        classified_volume_plan_build_for_stream(
            &plan, &fixture.applications, NULL, &configuration, &targets, 1, 60));
    EXPECT_PLAN_PRESERVED_AFTER_FAILURE(
        classified_volume_plan_build_for_stream(
            &plan,
            &fixture.applications,
            &fixture.streams,
            NULL,
            &targets,
            1,
            60));
    EXPECT_PLAN_PRESERVED_AFTER_FAILURE(
        classified_volume_plan_build_for_stream(
            &plan,
            &fixture.applications,
            &fixture.streams,
            &configuration,
            NULL,
            1,
            60));

    audio_stream_inventory_t malformed_streams = fixture.streams;
    malformed_streams.count = malformed_streams.capacity + 1;
    EXPECT_PLAN_PRESERVED_AFTER_FAILURE(
        classified_volume_plan_build_all(
            &plan,
            &fixture.applications,
            &malformed_streams,
            &configuration,
            &targets,
            1));

    malformed_streams = (audio_stream_inventory_t){
        .streams = NULL,
        .count = 1,
        .capacity = 1,
    };
    EXPECT_PLAN_PRESERVED_AFTER_FAILURE(
        classified_volume_plan_build_all(
            &plan,
            &fixture.applications,
            &malformed_streams,
            &configuration,
            &targets,
            1));

    active_application_inventory_t malformed_applications =
        fixture.applications;
    malformed_applications.count = malformed_applications.capacity + 1;
    EXPECT_PLAN_PRESERVED_AFTER_FAILURE(
        classified_volume_plan_build_all(
            &plan,
            &malformed_applications,
            &fixture.streams,
            &configuration,
            &targets,
            1));

    malformed_applications = (active_application_inventory_t){
        .applications = NULL,
        .count = 1,
        .capacity = 1,
    };
    EXPECT_PLAN_PRESERVED_AFTER_FAILURE(
        classified_volume_plan_build_all(
            &plan,
            &malformed_applications,
            &fixture.streams,
            &configuration,
            &targets,
            1));

    active_application_t malformed_application =
        fixture.applications.applications[0];
    malformed_application.stream_count =
        malformed_application.stream_capacity + 1;
    malformed_applications = (active_application_inventory_t){
        .applications = &malformed_application,
        .count = 1,
        .capacity = 1,
    };
    EXPECT_PLAN_PRESERVED_AFTER_FAILURE(
        classified_volume_plan_build_all(
            &plan,
            &malformed_applications,
            &fixture.streams,
            &configuration,
            &targets,
            1));

    malformed_application = fixture.applications.applications[0];
    malformed_application.stream_indexes = NULL;
    malformed_application.stream_count = 1;
    malformed_application.stream_capacity = 1;
    malformed_applications.applications = &malformed_application;
    EXPECT_PLAN_PRESERVED_AFTER_FAILURE(
        classified_volume_plan_build_for_stream(
            &plan,
            &malformed_applications,
            &fixture.streams,
            &configuration,
            &targets,
            1,
            60));

    fixture.streams.streams[0].channel_count = 0;
    EXPECT_PLAN_PRESERVED_AFTER_FAILURE(
        classified_volume_plan_build_all(
            &plan,
            &fixture.applications,
            &fixture.streams,
            &configuration,
            &targets,
            1));
    fixture.streams.streams[0].channel_count = PA_CHANNELS_MAX + 1U;
    EXPECT_PLAN_PRESERVED_AFTER_FAILURE(
        classified_volume_plan_build_for_stream(
            &plan,
            &fixture.applications,
            &fixture.streams,
            &configuration,
            &targets,
            1,
            60));
    fixture.streams.streams[0].channel_count = 2;

#undef EXPECT_PLAN_PRESERVED_AFTER_FAILURE

    classified_volume_plan_init(NULL);
    classified_volume_plan_clear(NULL);
    classified_volume_plan_clear(&plan);
    fixture_clear(&fixture);
}

static void test_assignment_growth_clear_reuse_and_replacement(void) {
    routing_fixture_t fixture;
    fixture_init(&fixture);
    for (uint32_t index = 100; index < 110; index++) {
        fixture_add_stream_with_channel_count(
            &fixture,
            index,
            (unsigned int)(index - 99),
            "org.example.ManyStreams",
            "Many Streams",
            "many-streams",
            NULL);
    }
    fixture_rebuild(&fixture);
    assert(fixture.applications.count == 1);

    config_t configuration = {0};
    config_add(&configuration, "org.example.ManyStreams", 0);
    chatmix_volume_targets_t targets = calculate_targets(16.0f);
    classified_volume_plan_t plan;
    classified_volume_plan_init(&plan);

    assert(classified_volume_plan_build_all(
               &plan,
               &fixture.applications,
               &fixture.streams,
               &configuration,
               &targets,
               1) == 0);
    assert(plan.count == 10);
    assert(plan.capacity >= 10);
    for (size_t i = 0; i < 10; i++) {
        expect_assignment_at(
            &plan,
            i,
            (uint32_t)(100 + i),
            (unsigned int)(i + 1),
            APPLICATION_GROUP_GAME,
            targets.game.pulse);
    }

    classified_volume_plan_clear(&plan);
    assert(plan.assignments == NULL);
    assert(plan.count == 0);
    assert(plan.capacity == 0);
    classified_volume_plan_clear(&plan);
    assert(plan.assignments == NULL);
    assert(plan.count == 0);
    assert(plan.capacity == 0);

    assert(classified_volume_plan_build_all(
               &plan,
               &fixture.applications,
               &fixture.streams,
               &configuration,
               &targets,
               1) == 0);
    assert(plan.count == 10);
    for (size_t i = 0; i < 10; i++) {
        expect_assignment_at(
            &plan,
            i,
            (uint32_t)(100 + i),
            (unsigned int)(i + 1),
            APPLICATION_GROUP_GAME,
            targets.game.pulse);
    }

    config_t empty_configuration = {0};
    assert(classified_volume_plan_build_all(
               &plan,
               &fixture.applications,
               &fixture.streams,
               &empty_configuration,
               &targets,
               1) == 0);
    assert(plan.assignments == NULL);
    assert(plan.count == 0);
    assert(plan.capacity == 0);

    assert(classified_volume_plan_build_all(
               &plan,
               &fixture.applications,
               &fixture.streams,
               &configuration,
               &targets,
               1) == 0);
    assert(plan.count == 10);
    for (size_t i = 0; i < 10; i++) {
        expect_assignment_at(
            &plan,
            i,
            (uint32_t)(100 + i),
            (unsigned int)(i + 1),
            APPLICATION_GROUP_GAME,
            targets.game.pulse);
    }

    classified_volume_plan_clear(&plan);
    fixture_clear(&fixture);
}

static void test_proton_identity_and_java_fallback(void) {
    routing_fixture_t fixture;
    fixture_init(&fixture);
    fixture_add_stream(
        &fixture, 50, NULL, "Torchlight II", "wine64-preloader", NULL);
    fixture_add_stream(
        &fixture, 51, NULL, "Deadlock", "wine64-preloader", NULL);
    fixture_add_stream(
        &fixture, 52, NULL, NULL, NULL, "java");
    fixture_rebuild(&fixture);
    assert(fixture.applications.count == 3);

    config_t configuration = {0};
    config_add(&configuration, "Torchlight II", 0);
    config_add(&configuration, "Deadlock", 1);
    config_add(&configuration, "java", 1);
    chatmix_volume_targets_t targets = calculate_targets(40.0f);
    classified_volume_plan_t plan;
    classified_volume_plan_init(&plan);

    assert(classified_volume_plan_build_all(
               &plan,
               &fixture.applications,
               &fixture.streams,
               &configuration,
               &targets,
               1) == 0);
    assert(plan.count == 3);
    expect_assignment(
        &plan, 50, 2, APPLICATION_GROUP_GAME, targets.game.pulse);
    expect_assignment(
        &plan, 51, 2, APPLICATION_GROUP_CHAT, targets.chat.pulse);
    expect_assignment(
        &plan, 52, 2, APPLICATION_GROUP_CHAT, targets.chat.pulse);

    classified_volume_plan_clear(&plan);
    fixture_clear(&fixture);
}

int main(void) {
    test_all_groups_and_aggregated_streams();
    test_first_config_entry_wins();
    test_missing_indexes_and_duplicate_suppression();
    test_new_stream_routes_complete_containing_application();
    test_unavailable_inventory_produces_no_assignments();
    test_empty_inventories_and_configuration();
    test_invalid_inputs_preserve_populated_plan();
    test_assignment_growth_clear_reuse_and_replacement();
    test_proton_identity_and_java_fallback();

    printf("classified_volume_routing tests passed\n");
    return 0;
}
