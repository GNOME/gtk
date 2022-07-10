#pragma once

#include <gtk/gtk.h>


#define GLYPH_VIEW_TYPE (glyph_view_get_type ())
#define GLYPH_VIEW(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLYPH_VIEW_TYPE, GlyphView))


typedef struct _GlyphView         GlyphView;
typedef struct _GlyphViewClass    GlyphViewClass;


GType       glyph_view_get_type (void);
GlyphView * glyph_view_new      (void);
void        glyph_view_set_font (GlyphView *view,
                                 Pango2Font *font);
void        glyph_view_set_palette (GlyphView *view,
                                    GQuark     palette);
void        glyph_view_set_glyph (GlyphView *view,
                                  hb_codepoint_t  glyph);
