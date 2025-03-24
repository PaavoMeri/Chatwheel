#include <pulse/pulseaudio.h>
#include <stdio.h>
#include <string.h>
#include "mixer.h"
#include "../config.h"

static pa_context *context = NULL;
static pa_mainloop *mainloop = NULL;

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

static void sink_input_info_cb(pa_context *c, const pa_sink_input_info *i, int eol, void *userdata) {
    if (eol || !i || !userdata) return;
    
    struct volume_control *vc = (struct volume_control*)userdata;
    const char *app_name = pa_proplist_gets(i->proplist, "application.name");
    const char *binary = pa_proplist_gets(i->proplist, "application.process.binary");
    
    // Match either application name or binary name
    if ((app_name && strcasecmp(app_name, vc->app_name) == 0) ||
        (binary && strcasecmp(binary, vc->app_name) == 0)) {
        
        int current_vol = (int)(pa_cvolume_avg(&i->volume) * 100.0f / PA_VOLUME_NORM);
        int target_vol = (int)(vc->target_volume * 100);
        
        printf("\nAdjusting %s volume: %d%% -> %d%%", app_name, current_vol, target_vol);
        
        pa_volume_t vol = (pa_volume_t)(vc->target_volume * PA_VOLUME_NORM);
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

    printf("\nChatmix position: %.0f%%", normalized * 100);
    printf("\nTarget volumes - Game: %.0f%%, Chat: %.0f%%", game_volume * 100, chat_volume * 100);
    
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
        if (strcasecmp(config.apps[i].name, app_name) == 0) {
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