#ifndef MIXER_H
#define MIXER_H

#include <stddef.h>
#include <stdint.h>
#include <pulse/pulseaudio.h> // Include PulseAudio or PipeWire headers as needed
#include "../application_classifier.h"
#include "../application_identity.h"

typedef struct {
    uint32_t index;
    const char *application_id;
    const char *application_name;
    const char *process_binary;
    const char *node_name;
} audio_stream_view_t;

typedef struct {
    application_identity_property_t identity_property;
    const char *identity_value;
    const char *display_name;
    const uint32_t *stream_indexes;
    size_t stream_count;
    application_group_t group;
    int matched_config_index;
} active_application_view_t;

// Initialize and cleanup
int initialize_audio_server(void);
void cleanup_audio_server(void);
void process_audio_events(void);

size_t get_active_audio_stream_count(void);

/*
 * Copies one stream's read-only view to stream. The view struct is owned by
 * the caller, but its string pointers are borrowed from the audio server's
 * inventory. They must not be modified or freed and may be invalidated by any
 * stream lifecycle change or cleanup_audio_server(). Returns 0 on success and
 * -1 when stream is NULL or position is out of bounds.
 */
int get_active_audio_stream(size_t position, audio_stream_view_t *stream);

/*
 * Returns the number of active applications while the derived inventory is
 * synchronized with the raw stream inventory. Returns 0 before initial
 * synchronization and after a rebuild failure, until a later rebuild succeeds.
 */
size_t get_active_application_count(void);

/*
 * Copies one application's read-only view to view. The view struct is owned by
 * the caller, but all pointer fields are borrowed from the private application
 * inventory. The caller must not modify or free borrowed data. Any successful
 * application-inventory rebuild or cleanup_audio_server() invalidates it, so
 * the view must not be retained while audio events are processed. Classification
 * fields are copied by value and describe the configuration loaded when this
 * function is called. Returns 0 on success and -1 when view is NULL, position
 * is out of bounds, or the derived inventory is not currently synchronized.
 */
int get_active_application(size_t position, active_application_view_t *view);

// Volume control functions
int set_application_volume(const char* app_name, float volume);
void adjust_volume_based_on_chatmix(float chatmix_value);

// Application listing
void list_applications(void);
void list_unconfigured_applications(void);

#endif // MIXER_H
