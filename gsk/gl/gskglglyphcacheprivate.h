#ifndef __GSK_GL_GLYPH_CACHE_PRIVATE_H__
#define __GSK_GL_GLYPH_CACHE_PRIVATE_H__

#include "gskgldriverprivate.h"
#include "gskglimageprivate.h"
#include "gskrendererprivate.h"
#include "gskgltextureatlasprivate.h"
#include <pango/pango.h>
#include <gdk/gdk.h>

typedef struct
{
  GskGLDriver *gl_driver;
  GskRenderer *renderer;

  GHashTable *hash_table;
  GPtrArray *atlases;

  guint64 timestamp;
} GskGLGlyphCache;

typedef struct
{
  PangoFont *font;
  PangoGlyph glyph;
  guint scale; /* times 1024 */
} GlyphCacheKey;

typedef struct _DirtyGlyph DirtyGlyph;
typedef struct _GskGLCachedGlyph GskGLCachedGlyph;

struct _DirtyGlyph
{
  GlyphCacheKey *key;
  GskGLCachedGlyph *value;
};

struct _GskGLCachedGlyph
{
  GskGLTextureAtlas *atlas;

  float tx;
  float ty;
  float tw;
  float th;

  int draw_x;
  int draw_y;
  int draw_width;
  int draw_height;

  float scale;

  guint64 timestamp;
  guint used: 1;
};


void                     gsk_gl_glyph_cache_init            (GskGLGlyphCache        *self,
                                                             GskRenderer            *renderer,
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
