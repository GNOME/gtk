#pragma once

#include <gtk/gtk.h>
#include <hb.h>

G_BEGIN_DECLS

#define GTK_TYPE_GLYPH_PAINTABLE (gtk_glyph_paintable_get_type ())

GDK_AVAILABLE_IN_4_10
G_DECLARE_FINAL_TYPE (GtkGlyphPaintable, gtk_glyph_paintable, GTK, GLYPH_PAINTABLE, GObject)

GDK_AVAILABLE_IN_4_10
GdkPaintable *  gtk_glyph_paintable_new               (hb_face_t              *face);

GDK_AVAILABLE_IN_4_10
void            gtk_glyph_paintable_set_face          (GtkGlyphPaintable      *self,
                                                       hb_face_t              *face);

GDK_AVAILABLE_IN_4_10
hb_face_t *     gtk_glyph_paintable_get_face          (GtkGlyphPaintable      *self);

GDK_AVAILABLE_IN_4_10
void            gtk_glyph_paintable_set_glyph         (GtkGlyphPaintable      *self,
                                                       hb_codepoint_t          glyph);

GDK_AVAILABLE_IN_4_10
hb_codepoint_t  gtk_glyph_paintable_get_glyph         (GtkGlyphPaintable      *self);

GDK_AVAILABLE_IN_4_10
void            gtk_glyph_paintable_set_variations    (GtkGlyphPaintable      *self,
                                                       const char             *variations);

GDK_AVAILABLE_IN_4_10
const char *    gtk_glyph_paintable_get_variations    (GtkGlyphPaintable      *self);

GDK_AVAILABLE_IN_4_10
void            gtk_glyph_paintable_set_color         (GtkGlyphPaintable      *self,
                                                       const GdkRGBA          *color);

GDK_AVAILABLE_IN_4_10
const GdkRGBA * gtk_glyph_paintable_get_color         (GtkGlyphPaintable      *self);

GDK_AVAILABLE_IN_4_10
void            gtk_glyph_paintable_set_palette_index (GtkGlyphPaintable      *self,
                                                       unsigned int            palette_index);

GDK_AVAILABLE_IN_4_10
unsigned int    gtk_glyph_paintable_get_palette_index (GtkGlyphPaintable      *self);

GDK_AVAILABLE_IN_4_10
void            gtk_glyph_paintable_set_custom_colors (GtkGlyphPaintable      *self,
                                                       const char             *palette);

GDK_AVAILABLE_IN_4_10
const char *    gtk_glyph_paintable_get_custom_colors (GtkGlyphPaintable      *self);

G_END_DECLS
