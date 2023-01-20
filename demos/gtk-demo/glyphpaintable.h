#pragma once

#include <gtk/gtk.h>
#include <hb.h>

G_BEGIN_DECLS

#define GLYPH_TYPE_PAINTABLE (glyph_paintable_get_type ())

G_DECLARE_FINAL_TYPE (GlyphPaintable, glyph_paintable, GLYPH, PAINTABLE, GObject)

GdkPaintable *  glyph_paintable_new               (hb_face_t           *face,
                                                   hb_codepoint_t       glyph);

void            glyph_paintable_set_face          (GlyphPaintable      *self,
                                                   hb_face_t           *face);

hb_face_t *     glyph_paintable_get_face          (GlyphPaintable      *self);

void            glyph_paintable_set_glyph         (GlyphPaintable      *self,
                                                   hb_codepoint_t       glyph);

hb_codepoint_t  glyph_paintable_get_glyph         (GlyphPaintable      *self);

void            glyph_paintable_set_variations    (GlyphPaintable      *self,
                                                   const char          *variations);

const char *    glyph_paintable_get_variations    (GlyphPaintable      *self);

void            glyph_paintable_set_color         (GlyphPaintable      *self,
                                                   const GdkRGBA       *color);

const GdkRGBA * glyph_paintable_get_color         (GlyphPaintable      *self);

void            glyph_paintable_set_palette_index (GlyphPaintable      *self,
                                                   unsigned int         palette_index);

unsigned int    glyph_paintable_get_palette_index (GlyphPaintable      *self);

void            glyph_paintable_set_custom_colors (GlyphPaintable      *self,
                                                   const char          *palette);

const char *    glyph_paintable_get_custom_colors (GlyphPaintable      *self);

G_END_DECLS
