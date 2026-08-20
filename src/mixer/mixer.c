#include <pulse/pulseaudio.h>
#include <stdio.h>
#include <string.h>
#include <math.h>  // Added for log10 function
#include <ctype.h>  // Added for tolower function
#include "mixer.h"
#include "pulse_stream_lifecycle.h"
#include "../config.h"

static pa_context *context = NULL;
static pa_mainloop *mainloop = NULL;
static float last_chatmix_normalized = 0.5f; // default balanced
static int has_valid_chatmix = 0;
static audio_stream_inventory_t stream_inventory;

struct snapshot_state {
    int failed;
};

// Forward declarations for helpers used before their definitions
static int wait_for_operation(pa_operation *op);
static float linear_to_logarithmic(float linear);
static void apply_current_mix_to_sink_input(pa_context *c, const pa_sink_input_info *i);
static void subscribe_callback(pa_context *c, pa_subscription_event_type_t t, uint32_t idx, void *userdata);
static void sink_input_new_cb(pa_context *ctx, const pa_sink_input_info *info, int eol, void *ud);
static void sink_input_change_cb(pa_context *ctx, const pa_sink_input_info *info, int eol, void *ud);
static void sink_input_snapshot_cb(pa_context *ctx, const pa_sink_input_info *info, int eol, void *ud);

// Add wildcard pattern matching function
static int match_pattern(const char *pattern, const char *text) {
    // If no pattern, do exact match
    if (!pattern || !text) return 0;
    
    // Handle exact match case (no wildcards)
    if (!strchr(pattern, '*') && !strchr(pattern, '?')) {
        return strcasecmp(pattern, text) == 0;
    }
    
    // Simple wildcard matching
    const char *p = pattern;
    const char *t = text;
    
    while (*p && *t) {
        if (*p == '*') {
            // Skip multiple asterisks
            while (*p == '*') p++;
            
            // If asterisk is at the end, match everything
            if (!*p) return 1;
            
            // Find the next non-wildcard character in pattern
            while (*t) {
                if (match_pattern(p, t)) return 1;
                t++;
            }
            return 0;
        }
        else if (*p == '?' || tolower(*p) == tolower(*t)) {
            p++;
            t++;
        }
        else {
            return 0;
        }
    }
    
    // Handle trailing asterisks in pattern
    while (*p == '*') p++;
    
    // Both should be at end for successful match
    return (*p == 0 && *t == 0);
}

static void context_state_callback(pa_context *c, void *userdata) {
    pa_context_state_t state = pa_context_get_state(c);
    int *ready = userdata;
    
    switch (state) {
        case PA_CONTEXT_READY:
            *ready = 1;
            break;
        case PA_CONTEXT_FAILED:
            *ready = 2;
            break;
        default:
            break;
    }
}

static int record_sink_input(const pa_sink_input_info *info) {
    if (!info) return -1;

    return pulse_stream_lifecycle_record(
        &stream_inventory,
        info->index,
        info->proplist);
}

static void subscribe_success_callback(pa_context *c, int success, void *userdata) {
    (void)c;
    int *subscription_succeeded = userdata;
    *subscription_succeeded = success;
}

static void apply_current_mix_to_sink_input(pa_context *c, const pa_sink_input_info *i) {
    if (!i) return;

    const char *app_name = pa_proplist_gets(i->proplist, "application.name");
    const char *binary = pa_proplist_gets(i->proplist, "application.process.binary");

    // Determine if this sink input matches any configured app and whether it's chat or game
    for (int cfgIndex = 0; cfgIndex < config.count; cfgIndex++) {
        const char *pattern = config.apps[cfgIndex].name;
        int is_chat = config.apps[cfgIndex].is_chat;

        int matches = 0;
        if (app_name && match_pattern(pattern, app_name)) matches = 1;
        if (!matches && binary && match_pattern(pattern, binary)) matches = 1;
        if (!matches) continue;

        float game_volume = 1.0f - last_chatmix_normalized;
        float chat_volume = last_chatmix_normalized;
        float linear_volume = is_chat ? chat_volume : game_volume;

        float log_volume = linear_to_logarithmic(linear_volume);
        pa_volume_t vol = (pa_volume_t)(log_volume * PA_VOLUME_NORM);
        pa_cvolume cvolume;
        pa_cvolume_init(&cvolume);
        pa_cvolume_set(&cvolume, i->volume.channels, vol);
        pa_context_set_sink_input_volume(c, i->index, &cvolume, NULL, NULL);
        printf("\nAuto-applied current mix to %s (%s)",
               app_name ? app_name : (binary ? binary : "unknown"),
               is_chat ? "Chat" : "Game");
        break;
    }
}

static void sink_input_new_cb(pa_context *ctx, const pa_sink_input_info *info, int eol, void *ud) {
    (void)ud;
    if (eol < 0) {
        fprintf(stderr, "Failed to read new PulseAudio stream information\n");
        return;
    }
    if (eol > 0 || !info) return;

    if (record_sink_input(info) != 0) {
        fprintf(stderr, "Failed to store PulseAudio stream %u\n", info->index);
    }
    if (has_valid_chatmix) {
        apply_current_mix_to_sink_input(ctx, info);
    }
}

static void sink_input_change_cb(pa_context *ctx, const pa_sink_input_info *info, int eol, void *ud) {
    (void)ctx;
    (void)ud;
    if (eol < 0) {
        fprintf(stderr, "Failed to read changed PulseAudio stream information\n");
        return;
    }
    if (eol > 0 || !info) return;

    if (record_sink_input(info) != 0) {
        fprintf(stderr, "Failed to update PulseAudio stream %u\n", info->index);
    }
}

static void sink_input_snapshot_cb(pa_context *ctx, const pa_sink_input_info *info, int eol, void *ud) {
    (void)ctx;
    struct snapshot_state *state = ud;

    if (eol < 0) {
        state->failed = 1;
        return;
    }
    if (eol > 0 || !info) return;

    if (record_sink_input(info) != 0) {
        state->failed = 1;
    }
}

static void subscribe_callback(pa_context *c, pa_subscription_event_type_t t, uint32_t idx, void *userdata) {
    (void)userdata;
    // Only react to sink input events
    if ((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) != PA_SUBSCRIPTION_EVENT_SINK_INPUT) return;

    pa_subscription_event_type_t type = t & PA_SUBSCRIPTION_EVENT_TYPE_MASK;
    if (type == PA_SUBSCRIPTION_EVENT_REMOVE) {
        audio_stream_inventory_remove(&stream_inventory, idx);
        return;
    }

    pa_sink_input_info_cb_t callback = NULL;
    if (type == PA_SUBSCRIPTION_EVENT_NEW) {
        callback = sink_input_new_cb;
    } else if (type == PA_SUBSCRIPTION_EVENT_CHANGE) {
        callback = sink_input_change_cb;
    }

    if (callback) {
        pa_operation *op = pa_context_get_sink_input_info(c, idx, callback, NULL);
        if (!op) {
            fprintf(stderr, "Failed to request PulseAudio stream %u\n", idx);
            return;
        }
        pa_operation_unref(op);
    }
}

int initialize_audio_server(void) {
    int ready = 0;
    has_valid_chatmix = 0;
    audio_stream_inventory_init(&stream_inventory);
    mainloop = pa_mainloop_new();
    if (!mainloop) goto fail;

    pa_mainloop_api *mainloop_api = pa_mainloop_get_api(mainloop);
    context = pa_context_new(mainloop_api, "chatwheel");
    if (!context) goto fail;

    pa_context_set_state_callback(context, context_state_callback, &ready);
    if (pa_context_connect(context, NULL, 0, NULL) < 0) goto fail;

    while (ready == 0) {
        if (pa_mainloop_iterate(mainloop, 1, NULL) < 0) goto fail;
    }

    pa_context_set_state_callback(context, NULL, NULL);
    if (ready != 1) goto fail;

    // Subscribe before taking the snapshot so changes during it are not missed.
    pa_context_set_subscribe_callback(context, subscribe_callback, NULL);
    int subscription_succeeded = 0;
    pa_operation *sub = pa_context_subscribe(context,
        (pa_subscription_mask_t)(PA_SUBSCRIPTION_MASK_SINK_INPUT),
        subscribe_success_callback,
        &subscription_succeeded);
    if (!sub) goto fail;

    int subscription_wait_result = wait_for_operation(sub);
    pa_operation_state_t subscription_state = pa_operation_get_state(sub);
    if (subscription_state == PA_OPERATION_RUNNING) {
        pa_operation_cancel(sub);
    }
    pa_operation_unref(sub);
    if (subscription_wait_result != 0 ||
        subscription_state != PA_OPERATION_DONE ||
        !subscription_succeeded) {
        goto fail;
    }

    struct snapshot_state snapshot = {0};
    pa_operation *snapshot_op = pa_context_get_sink_input_info_list(
        context,
        sink_input_snapshot_cb,
        &snapshot);
    if (!snapshot_op) goto fail;

    int snapshot_wait_result = wait_for_operation(snapshot_op);
    pa_operation_state_t snapshot_operation_state =
        pa_operation_get_state(snapshot_op);
    if (snapshot_operation_state == PA_OPERATION_RUNNING) {
        pa_operation_cancel(snapshot_op);
    }
    pa_operation_unref(snapshot_op);
    if (snapshot_wait_result != 0 ||
        snapshot_operation_state != PA_OPERATION_DONE ||
        snapshot.failed) {
        goto fail;
    }

    return 0;

fail:
    cleanup_audio_server();
    return -1;
}

void cleanup_audio_server(void) {
    if (context) {
        pa_context_set_state_callback(context, NULL, NULL);
        pa_context_set_subscribe_callback(context, NULL, NULL);
        pa_context_disconnect(context);
        pa_context_unref(context);
        context = NULL;
    }
    if (mainloop) {
        pa_mainloop_free(mainloop);
        mainloop = NULL;
    }
    audio_stream_inventory_clear(&stream_inventory);
    has_valid_chatmix = 0;
}

void process_audio_events(void) {
    if (!mainloop) return;
    // Non-blocking iterate to process pending context callbacks
    int retval = 0;
    pa_mainloop_iterate(mainloop, 0, &retval);
}

size_t get_active_audio_stream_count(void) {
    return stream_inventory.count;
}

int get_active_audio_stream(size_t position, audio_stream_view_t *stream) {
    if (!stream || position >= stream_inventory.count) return -1;

    const audio_stream_t *inventory_stream =
        &stream_inventory.streams[position];
    stream->index = inventory_stream->index;
    stream->application_name = inventory_stream->application_name;
    stream->process_binary = inventory_stream->process_binary;
    return 0;
}

// Structure to store app info
typedef struct {
    uint32_t index;
    char *name;
    pa_cvolume volume;
} app_info_t;

struct volume_control {
    const char* app_name;
    float target_volume;
    int found;  // Flag to track if we found the app
};

// Convert linear volume (0.0-1.0) to logarithmic scale for better perception
static float linear_to_logarithmic(float linear) {
    // Avoid log(0) which is -infinity
    if (linear < 0.01f) return 0.0f;
    
    // Calculate logarithmic volume using the formula:
    // volume_log = (10^(volume_linear) - 1) / 9
    // This creates a logarithmic curve from 0.0 to 1.0
    return (powf(10.0f, linear) - 1.0f) / 9.0f;
}

static void sink_input_info_cb(pa_context *c, const pa_sink_input_info *i, int eol, void *userdata) {
    if (eol || !i || !userdata) return;
    
    struct volume_control *vc = (struct volume_control*)userdata;
    const char *app_name = pa_proplist_gets(i->proplist, "application.name");
    const char *binary = pa_proplist_gets(i->proplist, "application.process.binary");
    
    // Match either application name or binary name using pattern matching
    if ((app_name && match_pattern(vc->app_name, app_name)) ||
        (binary && match_pattern(vc->app_name, binary))) {
        
        int current_vol = (int)(pa_cvolume_avg(&i->volume) * 100.0f / PA_VOLUME_NORM);
        int target_vol = (int)(vc->target_volume * 100);
        
        printf("\nAdjusting %s volume: %d%% -> %d%%", app_name, current_vol, target_vol);
        
        // Apply logarithmic scaling to the volume
        float log_volume = linear_to_logarithmic(vc->target_volume);
        pa_volume_t vol = (pa_volume_t)(log_volume * PA_VOLUME_NORM);
        pa_cvolume cvolume;
        pa_cvolume_init(&cvolume);
        pa_cvolume_set(&cvolume, i->volume.channels, vol);
        pa_context_set_sink_input_volume(c, i->index, &cvolume, NULL, NULL);
        vc->found = 1;
    }
}

// Add this helper function
static int wait_for_operation(pa_operation *op) {
    if (!op) return -1;

    while (op && pa_operation_get_state(op) == PA_OPERATION_RUNNING) {
        if (pa_mainloop_iterate(mainloop, 1, NULL) < 0) return -1;
    }

    return 0;
}

static void list_apps_callback(pa_context *c, const pa_sink_input_info *i, int eol, void *userdata) {
    (void)c;      // Suppress warning
    (void)userdata; // Suppress warning
    
    if (eol > 0 || !i) return;

    const char *app_name = pa_proplist_gets(i->proplist, "application.name");
    const char *binary = pa_proplist_gets(i->proplist, "application.process.binary");
    float volume = pa_cvolume_avg(&i->volume) * 100.0f / PA_VOLUME_NORM;
    
    if (app_name) {
        printf("Application: %-20s Volume: %.0f%% [Index: %u]", app_name, volume, i->index);
        if (binary) {
            printf(" (Binary: %s)", binary);
        }
        printf("\n");
    }
}

int set_application_volume(const char* app_name, float volume) {
    if (!context || volume < 0.0f || volume > 1.0f) return -1;

    struct volume_control vc = {
        .app_name = app_name,
        .target_volume = volume,
        .found = 0
    };

    pa_operation *op = pa_context_get_sink_input_info_list(context, 
                                                          sink_input_info_cb, 
                                                          &vc);
    if (op) {
        wait_for_operation(op);
        pa_operation_unref(op);
        return vc.found ? 0 : -1;
    }
    return -1;
}

void adjust_volume_based_on_chatmix(float chatmix_value) {
    float normalized = chatmix_value / 128.0f;
    float game_volume = 1.0f - normalized;
    float chat_volume = normalized;

    last_chatmix_normalized = normalized;
    has_valid_chatmix = 1;

    // Calculate logarithmic equivalents for display purposes
    float log_game_volume = linear_to_logarithmic(game_volume);
    float log_chat_volume = linear_to_logarithmic(chat_volume);

    printf("\nChatmix position: %.0f%%", normalized * 100);
    printf("\nTarget volumes - Game: %.0f%% (%.0f%% logarithmic), Chat: %.0f%% (%.0f%% logarithmic)", 
           game_volume * 100, log_game_volume * 100, 
           chat_volume * 100, log_chat_volume * 100);
    
    // Update all configured applications
    for (int i = 0; i < config.count; i++) {
        float volume = config.apps[i].is_chat ? chat_volume : game_volume;
        if (set_application_volume(config.apps[i].name, volume) == 0) {
            printf("\nUpdated %s (%s)", config.apps[i].name, 
                   config.apps[i].is_chat ? "Chat" : "Game");
        }
    }
    printf("\n");
}

void list_applications(void) {
    if (!context) {
        printf("No PulseAudio context available\n");
        return;
    }

    pa_operation *op = pa_context_get_sink_input_info_list(context, list_apps_callback, NULL);
    if (op) {
        wait_for_operation(op);
        pa_operation_unref(op);
    }
}

static int is_app_configured(const char* app_name) {
    for (int i = 0; i < config.count; i++) {
        if (match_pattern(config.apps[i].name, app_name)) {
            return 1;
        }
    }
    return 0;
}

static void list_unconfigured_callback(pa_context *c, const pa_sink_input_info *i, int eol, void *userdata) {
    (void)c;
    (void)userdata;
    
    if (eol > 0 || !i) return;
    const char *app_name = pa_proplist_gets(i->proplist, "application.name");
    if (app_name && !is_app_configured(app_name)) {
        printf("%s\n", app_name);
    }
}

void list_unconfigured_applications(void) {
    if (!context) {
        printf("No PulseAudio context available\n");
        return;
    }

    printf("Unconfigured applications:\n");
    pa_operation *op = pa_context_get_sink_input_info_list(context, 
                                                          list_unconfigured_callback, 
                                                          NULL);
    if (op) {
        wait_for_operation(op);
        pa_operation_unref(op);
    }
}
