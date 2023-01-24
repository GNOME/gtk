#pragma once

#include <gtk/gtk.h>


#define FONT_PICKER_TYPE (font_picker_get_type ())
#define FONT_PICKER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), FONT_PICKER_TYPE, FontPicker))


typedef struct _FontPicker         FontPicker;
typedef struct _FontPickerClass    FontPickerClass;


GType        font_picker_get_type          (void);
void         font_picker_set_face          (FontPicker *self,
                                            hb_face_t   *face);
void         font_picker_set_from_file     (FontPicker *self,
                                            const char *path);
