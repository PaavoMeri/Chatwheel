#include "gui.h"
#include "../mixer/mixer.h"
#include "../headset/headset.h"
#include "../../include/config.h"
#include <gtk/gtk.h>

static GtkWidget *window;
static GtkWidget *app_list;
static GtkListStore *list_store;
static void (*volume_callback)(const char*, float) = NULL;

static void on_app_toggled(GtkCellRendererToggle *cell,
                          gchar *path_str,
                          gpointer data) {
    GtkTreeIter iter;
    GtkTreePath *path = gtk_tree_path_new_from_string(path_str);
    gboolean active;
    gchar *app_name;
    
    gtk_tree_model_get_iter(GTK_TREE_MODEL(list_store), &iter, path);
    gtk_tree_model_get(GTK_TREE_MODEL(list_store), &iter,
                       1, &active,
                       0, &app_name,
                       -1);
    
    active ^= 1;
    gtk_list_store_set(list_store, &iter, 1, active, -1);
    
    if (volume_callback) {
        int chatmix = get_chatmix_value();
        if (chatmix >= 0) {
            volume_callback(app_name, (float)chatmix / 100.0f);
        }
    }
    
    g_free(app_name);
    gtk_tree_path_free(path);
}

void init_gui(int *argc, char ***argv) {
    gtk_init(argc, argv);
    
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), GUI_WINDOW_TITLE);
    gtk_window_set_default_size(GTK_WINDOW(window), 
                               GUI_WINDOW_WIDTH, 
                               GUI_WINDOW_HEIGHT);
    
    list_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_BOOLEAN);
    app_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
    
    GtkCellRenderer *text_renderer = gtk_cell_renderer_text_new();
    GtkCellRenderer *toggle_renderer = gtk_cell_renderer_toggle_new();
    
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(app_list),
                                              0, "Application",
                                              text_renderer,
                                              "text", 0,
                                              NULL);
    
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(app_list),
                                              1, "Control",
                                              toggle_renderer,
                                              "active", 1,
                                              NULL);
    
    g_signal_connect(toggle_renderer, "toggled",
                     G_CALLBACK(on_app_toggled), NULL);
    
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), app_list);
    gtk_container_add(GTK_CONTAINER(window), scroll);
    
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
}

void show_main_window(void) {
    gtk_widget_show_all(window);
    gtk_main();
}

void update_application_list(void) {
    gtk_list_store_clear(list_store);
    char **apps = list_audio_applications();
    
    if (apps) {
        for (int i = 0; apps[i] != NULL; i++) {
            GtkTreeIter iter;
            gtk_list_store_append(list_store, &iter);
            gtk_list_store_set(list_store, &iter,
                             0, apps[i],
                             1, FALSE,
                             -1);
            free(apps[i]);
        }
        free(apps);
    }
}

void set_volume_update_callback(void (*callback)(const char*, float)) {
    volume_callback = callback;
}
