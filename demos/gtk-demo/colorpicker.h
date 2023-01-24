#pragma once

#include <gtk/gtk.h>


#define COLOR_PICKER_TYPE (color_picker_get_type ())
#define COLOR_PICKER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), COLOR_PICKER_TYPE, ColorPicker))


typedef struct _ColorPicker         ColorPicker;
typedef struct _ColorPickerClass    ColorPickerClass;


GType        color_picker_get_type          (void);
GAction *    color_picker_get_reset_action  (ColorPicker *self);
