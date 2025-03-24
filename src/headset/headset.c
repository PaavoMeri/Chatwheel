#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "headset.h"

#define BUFFER_SIZE 4096
#define NO_CHATMIX -1

static char* find_json_value(const char* json, const char* key) {
    char search_key[256];
    static char value[256];
    snprintf(search_key, sizeof(search_key), "\"%s\":", key);
    
    char* pos = strstr(json, search_key);
    if (!pos) return NULL;
    
    pos += strlen(search_key);
    // Skip whitespace and any non-digit characters
    while (*pos && (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '"')) pos++;
    
    if ((*pos >= '0' && *pos <= '9') || *pos == '-') {
        int i = 0;
        while ((pos[i] >= '0' && pos[i] <= '9') || pos[i] == '-') {
            value[i] = pos[i];
            i++;
        }
        value[i] = '\0';
        return value;
    }
    return NULL;
}

const char* get_chatmix_mode(int value) {
    if (value < 0) return "Unknown";
    if (value < 32) return "100% Game";     // Top quarter
    if (value < 64) return "Game Focus";    // Upper middle
    if (value == 64) return "Balanced";     // Middle
    if (value < 96) return "Chat Focus";    // Lower middle
    return "100% Chat";                     // Bottom quarter
}

int get_chatmix_value() {
    FILE *fp;
    char buffer[BUFFER_SIZE] = {0};
    char *ptr = buffer;
    int chatmix_value = NO_CHATMIX;

    fp = popen("headsetcontrol --output json", "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed to run HeadsetControl\n");
        return NO_CHATMIX;
    }

    while (fgets(ptr, BUFFER_SIZE - (ptr - buffer), fp) != NULL) {
        ptr += strlen(ptr);
    }
    pclose(fp);

    char* device_count = find_json_value(buffer, "device_count");
    if (!device_count || strcmp(device_count, "0") == 0) {
        fprintf(stderr, "No devices found\n");
        return NO_CHATMIX;
    }

    if (strstr(buffer, "\"errors\"")) {
        if (strstr(buffer, "\"chatmix\":")) {
            fprintf(stderr, "Error retrieving chatmix status\n");
            return NO_CHATMIX;
        }
    }

    char* chatmix = find_json_value(buffer, "chatmix");
    if (chatmix) {
        chatmix_value = atoi(chatmix);
    }

    return chatmix_value;
}