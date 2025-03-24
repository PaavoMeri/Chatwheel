#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "mixer/mixer.h"
#include "headset/headset.h"

int main(void) {
    if (initialize_audio_server() != 0) {
        fprintf(stderr, "Failed to initialize audio server\n");
        return 1;
    }

    printf("Available audio applications:\n");
    printf("----------------------------\n");
    list_applications();
    printf("\n");

    // Let user input the application name
    char app_name[256];
    printf("Enter application name to test (from list above): ");
    if (fgets(app_name, sizeof(app_name), stdin)) {
        // Remove newline
        app_name[strcspn(app_name, "\n")] = 0;
        
        printf("\nTesting volume control for: %s\n", app_name);
        
        printf("Setting volume to 0%%...\n");
        set_application_volume(app_name, 0.0f);
        sleep(1);
        
        printf("Setting volume to 50%%...\n");
        set_application_volume(app_name, 0.5f);
        sleep(1);
        
        printf("Setting volume to 100%%...\n");
        set_application_volume(app_name, 1.0f);
        sleep(1);
        
        // Test volume ramping
        printf("\nRamping volume up and down...\n");
        for (float vol = 0.0f; vol <= 1.0f; vol += 0.1f) {
            set_application_volume(app_name, vol);
            usleep(100000);  // 100ms delay
        }
    }

    cleanup_audio_server();
    return 0;
}
