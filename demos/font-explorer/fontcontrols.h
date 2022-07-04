#pragma once

#include <gtk/gtk.h>


#define FONT_CONTROLS_TYPE (font_controls_get_type ())
#define FONT_CONTROLS(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), FONT_CONTROLS_TYPE, FontControls))


typedef struct _FontControls         FontControls;
typedef struct _FontControlsClass    FontControlsClass;


GType           font_controls_get_type          (void);
FontControls *  font_controls_new               (void);
GAction *       font_controls_get_reset_action  (FontControls *self);
