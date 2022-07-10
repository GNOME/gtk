#pragma once

#include <gtk/gtk.h>


#define FONT_COLORS_TYPE (font_colors_get_type ())
#define FONT_COLORS(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), FONT_COLORS_TYPE, FontColors))


typedef struct _FontColors         FontColors;
typedef struct _FontColorsClass    FontColorsClass;


GType        font_colors_get_type          (void);
GAction *    font_colors_get_reset_action  (FontColors    *self);
