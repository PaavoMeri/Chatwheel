#include <pulse/pulseaudio.h>
#include <stdio.h>
#include <string.h>
#include <math.h>  // Added for log10 function
#include <ctype.h>  // Added for tolower function
#include "mixer.h"
#include "../config.h"

static pa_context *context = NULL;
static pa_mainloop *mainloop = NULL;

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

int initialize_audio_server(void) {
    int ready = 0;
    mainloop = pa_mainloop_new();
    pa_mainloop_api *mainloop_api = pa_mainloop_get_api(mainloop);
    
    context = pa_context_new(mainloop_api, "chatwheel");
    pa_context_connect(context, NULL, 0, NULL);
    
    pa_context_set_state_callback(context, context_state_callback, &ready);
    
    while (ready == 0) {
        pa_mainloop_iterate(mainloop, 1, NULL);
    }
    
    return (ready == 1) ? 0 : -1;
}

void cleanup_audio_server(void) {
    if (context) {
        pa_context_disconnect(context);
        pa_context_unref(context);
    }
    if (mainloop) {
        pa_mainloop_free(mainloop);
    }
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
static void wait_for_operation(pa_operation *op) {
    while (op && pa_operation_get_state(op) == PA_OPERATION_RUNNING) {
        pa_mainloop_iterate(mainloop, 1, NULL);
    }
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