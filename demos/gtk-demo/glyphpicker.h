#pragma once

#include <gtk/gtk.h>


#define GLYPH_PICKER_TYPE (glyph_picker_get_type ())
#define GLYPH_PICKER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLYPH_PICKER_TYPE, GlyphPicker))


typedef struct _GlyphPicker         GlyphPicker;
typedef struct _GlyphPickerClass    GlyphPickerClass;


GType        glyph_picker_get_type          (void);
