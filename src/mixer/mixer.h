#ifndef MIXER_H
#define MIXER_H

#include <pulse/pulseaudio.h> // Include PulseAudio or PipeWire headers as needed

// Initialize and cleanup
int initialize_audio_server(void);
void cleanup_audio_server(void);

// Volume control functions
int set_application_volume(const char* app_name, float volume);
void adjust_volume_based_on_chatmix(float chatmix_value);

// Application listing
void list_applications(void);
void list_unconfigured_applications(void);

#endif // MIXER_H