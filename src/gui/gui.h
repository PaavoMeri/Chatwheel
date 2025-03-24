#ifndef GUI_H
#define GUI_H

#include <gtk/gtk.h>

void init_gui(int *argc, char ***argv);
void show_main_window(void);
void update_application_list(void);
void set_volume_update_callback(void (*callback)(const char*, float));

#endif // GUI_H
