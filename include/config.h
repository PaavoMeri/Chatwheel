#ifndef CONFIG_H
#define CONFIG_H

// Configuration settings for DynamicChatmixMixer

#define MAX_AUDIO_APPS 100          // Maximum number of audio applications
#define DEFAULT_CHATMIX_VALUE 50    // Default chatmix value (0-100)
#define CONFIG_FILE_PATH "/etc/dynamic_chatmix_mixer.conf" // Path to configuration file

#define GUI_WINDOW_TITLE "DynamicChatmixMixer"
#define GUI_WINDOW_WIDTH 400
#define GUI_WINDOW_HEIGHT 300
#define REFRESH_INTERVAL_MS 100    // How often to update volumes

#endif // CONFIG_H