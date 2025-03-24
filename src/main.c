#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include "headset/headset.h"
#include "mixer/mixer.h"
#include "config.h"

#define POLL_INTERVAL_MS 100
volatile sig_atomic_t running = 1;

static void print_usage(void) {
    printf("Usage: chatwheel [OPTIONS]\n");
    printf("Options:\n");
    printf("  --daemon           Run as background service\n");
    printf("  --add NAME,TYPE    Add application (TYPE: game|chat)\n");
    printf("  --remove NAME      Remove application from control\n");
    printf("  --list            List all configured applications\n");
    printf("  --list-new        List unconfigured applications\n");
    printf("  --status          Show current chatmix and volume status\n");
    printf("  --restart         Restart the service to apply changes\n");
    printf("  --help            Show this help message\n");
}

static void print_restart_notice(void) {
    printf("\nNOTE: Changes require service restart to take effect.\n");
    printf("Run: systemctl --user restart chatwheel\n");
}

static void handle_signal(int signum) {
    (void)signum;  // Suppress unused parameter warning
    running = 0;
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        if (strcmp(argv[1], "--help") == 0) {
            print_usage();
            return 0;
        }
        else if (strcmp(argv[1], "--list") == 0) {
            load_config();
            list_configured_apps();
            return 0;
        }
        else if (strcmp(argv[1], "--add") == 0 && argc > 2) {
            load_config();
            char name[256] = {0};
            char type[32] = {0};
            
            if (sscanf(argv[2], "%255[^,],%31s", name, type) != 2) {
                fprintf(stderr, "Invalid format. Use: NAME,game or NAME,chat\n");
                return 1;
            }
            
            int is_chat;
            if (strcasecmp(type, "chat") == 0) {
                is_chat = 1;
            }
            else if (strcasecmp(type, "game") == 0) {
                is_chat = 0;
            }
            else {
                fprintf(stderr, "Invalid type '%s'. Use: game or chat\n", type);
                return 1;
            }
            
            if (add_application(name, is_chat) == 0) {
                save_config();
                printf("Added %s as %s application\n", name, is_chat ? "chat" : "game");
                print_restart_notice();
            }
            return 0;
        }
        else if (strcmp(argv[1], "--remove") == 0 && argc > 2) {
            load_config();
            if (remove_application(argv[2]) == 0) {
                save_config();
                printf("Removed %s from configuration\n", argv[2]);
                print_restart_notice();
            } else {
                fprintf(stderr, "Application '%s' not found in configuration\n", argv[2]);
            }
            return 0;
        }
        else if (strcmp(argv[1], "--list-new") == 0) {
            if (initialize_audio_server() != 0) {
                fprintf(stderr, "Failed to initialize audio server\n");
                return 1;
            }
            list_unconfigured_applications();
            cleanup_audio_server();
            return 0;
        }
        else if (strcmp(argv[1], "--reload") == 0) {
            load_config();
            printf("Configuration reloaded\n");
            return 0;
        }
        else if (strcmp(argv[1], "--restart") == 0) {
            system("systemctl --user restart chatwheel");
            printf("Service restarted\n");
            return 0;
        }
        else if (strcmp(argv[1], "--daemon") == 0) {
            // Continue with daemon mode
        }
        else {
            print_usage();
            return 1;
        }
    }

    load_config();
    if (initialize_audio_server() != 0) {
        fprintf(stderr, "Failed to initialize audio server\n");
        return 1;
    }

    int prev_chatmix = -1;
    
    // Set up signal handling
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    printf("Monitoring chatmix value. Press Ctrl+C to exit.\n\n");
    
    while (running) {
        int chatmix = get_chatmix_value();
        
        if (chatmix != prev_chatmix) {
            printf("\033[2K\r"); // Clear line
            if (chatmix == -1) {
                printf("Failed to get chatmix value");
            } else {
                // Pass raw chatmix value (0-128) directly
                adjust_volume_based_on_chatmix(chatmix);
                printf("Chatmix: %d (%s)", chatmix, get_chatmix_mode(chatmix));
            }
            fflush(stdout);
            prev_chatmix = chatmix;
        }
        
        usleep(POLL_INTERVAL_MS * 1000);
    }
    
    printf("\nExiting...\n");
    cleanup_audio_server();
    return 0;
}