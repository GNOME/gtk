#pragma once

#include <gtk/gtk.h>


#define FONT_EXPLORER_APP_TYPE (font_explorer_app_get_type ())
#define FONT_EXPLORER_APP(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), FONT_EXPLORER_APP_TYPE, FontExplorerApp))


typedef struct _FontExplorerApp       FontExplorerApp;
typedef struct _FontExplorerAppClass  FontExplorerAppClass;


GType                   font_explorer_app_get_type    (void);
FontExplorerApp *       font_explorer_app_new         (void);
