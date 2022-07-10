#pragma once

#include <gtk/gtk.h>


#define GLYPHS_VIEW_TYPE (glyphs_view_get_type ())
#define GLYPHS_VIEW(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLYPHS_VIEW_TYPE, GlyphsView))


typedef struct _GlyphsView         GlyphsView;
typedef struct _GlyphsViewClass    GlyphsViewClass;


GType           glyphs_view_get_type     (void);
