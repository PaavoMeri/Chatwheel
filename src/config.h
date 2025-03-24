#ifndef CONFIG_H
#define CONFIG_H

#define MAX_APPS 32
#define CONFIG_FILE "chatwheel.conf"

typedef struct {
    char name[256];
    int is_chat;  // 0 for game, 1 for chat
} app_config_t;

typedef struct {
    app_config_t apps[MAX_APPS];
    int count;
} config_t;

void load_config(void);
void save_config(void);
int add_application(const char* name, int is_chat);
int remove_application(const char* name);
void list_configured_apps(void);

extern config_t config;

#endif
