#ifndef __GSK_GL_GLYPH_CACHE_PRIVATE_H__
#define __GSK_GL_GLYPH_CACHE_PRIVATE_H__

#include "gskgldriverprivate.h"
#include "gskglimageprivate.h"
#include "gskgltextureatlasprivate.h"
#include <pango/pango.h>
#include <gdk/gdk.h>

typedef struct
{
  int ref_count;

  GdkDisplay *display;
  GHashTable *hash_table;
  GskGLTextureAtlases *atlases;

  guint64 timestamp;
} GskGLGlyphCache;

typedef struct
{
  PangoFont *font;
  PangoGlyph glyph;
  guint scale; /* times 1024 */
} GlyphCacheKey;

typedef struct _GskGLCachedGlyph GskGLCachedGlyph;

struct _GskGLCachedGlyph
{
  GskGLTextureAtlas *atlas;
  guint texture_id;

  float tx;
  float ty;
  float tw;
  float th;

  int draw_x;
  int draw_y;
  int draw_width;
  int draw_height;

  guint64 timestamp;
  guint used: 1;
};


GskGLGlyphCache *        gsk_gl_glyph_cache_new             (GdkDisplay *display,
                                                             GskGLTextureAtlases *atlases);
GskGLGlyphCache *        gsk_gl_glyph_cache_ref             (GskGLGlyphCache *self);
void                     gsk_gl_glyph_cache_unref           (GskGLGlyphCache        *self);
void                     gsk_gl_glyph_cache_begin_frame     (GskGLGlyphCache        *self);
gboolean                 gsk_gl_glyph_cache_lookup          (GskGLGlyphCache        *self,
                                                             PangoFont              *font,
                                                             PangoGlyph              glyph,
                                                             float                   scale,
                                                             GskGLDriver            *driver,
                                                             GskGLCachedGlyph       *cached_glyph_out);

#endif
