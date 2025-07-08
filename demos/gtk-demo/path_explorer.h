#pragma once

#include <gtk/gtk.h>

G_DECLARE_FINAL_TYPE (PathExplorer, path_explorer, PATH, EXPLORER, GtkWidget)

GtkWidget * path_explorer_new (void);
