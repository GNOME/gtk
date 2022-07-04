#pragma once

#include <gtk/gtk.h>
#include "fontexplorerapp.h"


#define FONT_EXPLORER_WINDOW_TYPE (font_explorer_window_get_type ())
#define FONT_EXPLORER_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), FONT_EXPLORER_WINDOW_TYPE, FontExplorerWindow))


typedef struct _FontExplorerWindow         FontExplorerWindow;
typedef struct _FontExplorerWindowClass    FontExplorerWindowClass;


GType                   font_explorer_window_get_type     (void);
FontExplorerWindow *    font_explorer_window_new          (FontExplorerApp    *app);
