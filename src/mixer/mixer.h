#ifndef MIXER_H
#define MIXER_H

#include <stddef.h>
#include <stdint.h>
#include <pulse/pulseaudio.h> // Include PulseAudio or PipeWire headers as needed

typedef struct {
    uint32_t index;
    const char *application_id;
    const char *application_name;
    const char *process_binary;
    const char *node_name;
} audio_stream_view_t;

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

// Volume control functions
int set_application_volume(const char* app_name, float volume);
void adjust_volume_based_on_chatmix(float chatmix_value);

// Application listing
void list_applications(void);
void list_unconfigured_applications(void);

#endif // MIXER_H
