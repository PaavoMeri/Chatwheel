#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config.h"

// Add wildcard pattern matching function (duplicate from mixer.c for now)
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

config_t config = {0};

static const char* get_config_path(void) {
    static char path[512];
    const char* xdg_config = getenv("XDG_CONFIG_HOME");
    
    if (xdg_config) {
        snprintf(path, sizeof(path), "%s/chatwheel/chatwheel.conf", xdg_config);
    } else {
        snprintf(path, sizeof(path), "%s/.config/chatwheel/chatwheel.conf", getenv("HOME"));
    }
    
    return path;
}

void load_config(void) {
    config.count = 0;  // Reset config before loading
    const char* config_path = get_config_path();
    FILE *f = fopen(config_path, "r");
    
    // Try system config if user config doesn't exist
    if (!f) {
        // No default config is created
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char name[256];
        int is_chat;
        if (sscanf(line, "%255[^,],%d", name, &is_chat) == 2) {
            add_application(name, is_chat);
        }
    }
    fclose(f);
}

void save_config(void) {
    const char* config_path = get_config_path();
    FILE *f = fopen(config_path, "w");
    if (!f) return;

    for (int i = 0; i < config.count; i++) {
        fprintf(f, "%s,%d\n", config.apps[i].name, config.apps[i].is_chat);
    }
    fclose(f);
}

int add_application(const char* name, int is_chat) {
    // Check for duplicates first
    for (int i = 0; i < config.count; i++) {
        if (strcasecmp(config.apps[i].name, name) == 0) {
            // Update type if different
            if (config.apps[i].is_chat != is_chat) {
                config.apps[i].is_chat = is_chat;
                printf("Updated %s to %s\n", name, is_chat ? "chat" : "game");
                return 0;
            }
            printf("Application '%s' already configured as %s\n", 
                   name, is_chat ? "chat" : "game");
            return -1;
        }
    }
    
    // Add new if not found and space available
    if (config.count >= MAX_APPS) return -1;
    
    strncpy(config.apps[config.count].name, name, 255);
    config.apps[config.count].name[255] = '\0';  // Ensure null termination
    config.apps[config.count].is_chat = is_chat;
    config.count++;
    return 0;
}

int remove_application(const char* name) {
    for (int i = 0; i < config.count; i++) {
        // For removal, we support both exact match and pattern match
        if (strcasecmp(config.apps[i].name, name) == 0 || 
            match_pattern(config.apps[i].name, name)) {
            memmove(&config.apps[i], &config.apps[i+1], 
                    (config.count - i - 1) * sizeof(app_config_t));
            config.count--;
            return 0;
        }
    }
    return -1;
}

void list_configured_apps(void) {
    printf("Configured applications:\n");
    for (int i = 0; i < config.count; i++) {
        printf("%s (%s)\n", config.apps[i].name, 
               config.apps[i].is_chat ? "Chat" : "Game");
    }
}
