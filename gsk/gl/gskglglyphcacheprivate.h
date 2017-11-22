#ifndef __GSK_GL_GLYPH_CACHE_PRIVATE_H__
#define __GSK_GL_GLYPH_CACHE_PRIVATE_H__

#include "gskgldriverprivate.h"
#include "gskglimageprivate.h"
#include <pango/pango.h>
#include <gdk/gdk.h>

typedef struct
{
  GskGLDriver *gl_driver;

  GHashTable *hash_table;
  GPtrArray *atlases;

  guint64 timestamp;
} GskGLGlyphCache;


typedef struct
{
  GskGLImage *image;
  int width, height;
  int x, y, y0;
  int num_glyphs;
  GList *dirty_glyphs;
  guint old_pixels;
} GskGLGlyphAtlas;

typedef struct
{
  GskGLGlyphAtlas *atlas;

  float tx;
  float ty;
  float tw;
  float th;

  int draw_x;
  int draw_y;
  int draw_width;
  int draw_height;

  guint64 timestamp;
} GskGLCachedGlyph;

void                     gsk_gl_glyph_cache_init            (GskGLGlyphCache        *self,
                                                             GskGLDriver            *gl_driver);
void                     gsk_gl_glyph_cache_free            (GskGLGlyphCache        *self);
void                     gsk_gl_glyph_cache_begin_frame     (GskGLGlyphCache        *self);
GskGLImage *             gsk_gl_glyph_cache_get_glyph_image (GskGLGlyphCache        *self,
                                                             const GskGLCachedGlyph *glyph);
const GskGLCachedGlyph * gsk_gl_glyph_cache_lookup          (GskGLGlyphCache        *self,
                                                             gboolean                create,
                                                             PangoFont              *font,
                                                             PangoGlyph              glyph,
                                                             float                   scale);

#endif
