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
    printf("  --list-streams    List active audio stream properties\n");
    printf("  --list-active     List active logical applications\n");
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

static int print_active_audio_streams(void) {
    size_t stream_count = get_active_audio_stream_count();
    printf("Active audio streams (%zu):\n", stream_count);

    for (size_t position = 0; position < stream_count; position++) {
        audio_stream_view_t stream;
        if (get_active_audio_stream(position, &stream) != 0) {
            fprintf(stderr, "Failed to read audio stream at position %zu\n", position);
            return -1;
        }

        printf("Sink input index: %u\n", stream.index);
        printf("  application.id: %s\n",
               stream.application_id ? stream.application_id : "(missing)");
        printf("  application.name: %s\n",
               stream.application_name ? stream.application_name : "(missing)");
        printf("  application.process.binary: %s\n",
               stream.process_binary ? stream.process_binary : "(missing)");
        printf("  node.name: %s\n",
               stream.node_name ? stream.node_name : "(missing)");
    }

    return 0;
}

static const char *identity_property_label(
    application_identity_property_t property) {
    switch (property) {
        case APPLICATION_IDENTITY_PROPERTY_NONE:
            return "(none)";
        case APPLICATION_IDENTITY_PROPERTY_APPLICATION_ID:
            return "application.id";
        case APPLICATION_IDENTITY_PROPERTY_APPLICATION_NAME:
            return "application.name";
        case APPLICATION_IDENTITY_PROPERTY_PROCESS_BINARY:
            return "application.process.binary";
        case APPLICATION_IDENTITY_PROPERTY_NODE_NAME:
            return "node.name";
        default:
            return "(unknown)";
    }
}

static const char *application_group_label(application_group_t group) {
    switch (group) {
        case APPLICATION_GROUP_UNASSIGNED:
            return "Unassigned";
        case APPLICATION_GROUP_GAME:
            return "Game";
        case APPLICATION_GROUP_CHAT:
            return "Chat";
        default:
            return "Unknown";
    }
}

static int print_active_applications(void) {
    size_t application_count = get_active_application_count();
    printf("Active applications (%zu):\n", application_count);

    for (size_t position = 0; position < application_count; position++) {
        active_application_view_t application;
        if (get_active_application(position, &application) != 0) {
            fprintf(stderr,
                    "Failed to read active application at position %zu\n",
                    position);
            return -1;
        }

        printf("Application %zu:\n", position + 1);
        printf("  display name: %s\n",
               application.display_name ? application.display_name : "(missing)");
        printf("  identity property: %s\n",
               identity_property_label(application.identity_property));
        printf("  identity value: %s\n",
               application.identity_value ? application.identity_value : "(missing)");
        printf("  classification: %s\n",
               application_group_label(application.group));
        if (application.matched_config_index < 0) {
            printf("  matched config index (zero-based): (none)\n");
            printf("  matched config pattern: (none)\n");
            printf("  matched config group: (none)\n");
        } else {
            if (config.count < 0 ||
                config.count > MAX_APPS ||
                application.matched_config_index >= config.count) {
                fprintf(stderr,
                        "Invalid matched config index %d for active application %zu\n",
                        application.matched_config_index,
                        position);
                return -1;
            }

            const app_config_t *matched_config =
                &config.apps[application.matched_config_index];
            printf("  matched config index (zero-based): %d\n",
                   application.matched_config_index);
            printf("  matched config pattern: %s\n",
                   matched_config->name[0] != '\0'
                       ? matched_config->name
                       : "(empty)");
            printf("  matched config group: %s\n",
                   matched_config->is_chat != 0 ? "Chat" : "Game");
        }
        printf("  stream count: %zu\n", application.stream_count);
        printf("  stream indexes:");
        if (!application.stream_indexes) {
            printf(" (missing)\n");
            continue;
        }
        if (application.stream_count == 0) {
            printf(" (none)\n");
            continue;
        }
        for (size_t i = 0; i < application.stream_count; i++) {
            printf("%s%u", i == 0 ? " " : ", ", application.stream_indexes[i]);
        }
        printf("\n");
    }

    return 0;
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
        else if (strcmp(argv[1], "--list-streams") == 0) {
            if (initialize_audio_server() != 0) {
                fprintf(stderr, "Failed to initialize audio server\n");
                return 1;
            }
            int result = print_active_audio_streams();
            cleanup_audio_server();
            return result == 0 ? 0 : 1;
        }
        else if (strcmp(argv[1], "--list-active") == 0) {
            if (load_config() != 0 ||
                config.count < 0 ||
                config.count > MAX_APPS) {
                fprintf(stderr, "Failed to load a valid configuration\n");
                return 1;
            }
            if (initialize_audio_server() != 0) {
                fprintf(stderr, "Failed to initialize audio server\n");
                return 1;
            }
            int result = print_active_applications();
            cleanup_audio_server();
            return result == 0 ? 0 : 1;
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
        
        // Process any pending audio server events (e.g., new app streams)
        process_audio_events();

        usleep(POLL_INTERVAL_MS * 1000);
    }
    
    printf("\nExiting...\n");
    cleanup_audio_server();
    return 0;
}
