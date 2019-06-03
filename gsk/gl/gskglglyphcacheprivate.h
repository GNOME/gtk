#ifndef __GSK_GL_GLYPH_CACHE_PRIVATE_H__
#define __GSK_GL_GLYPH_CACHE_PRIVATE_H__

#include "gskgldriverprivate.h"
#include "gskglimageprivate.h"
#include "gskgltextureatlasprivate.h"
#include <pango/pango.h>
#include <gdk/gdk.h>

typedef struct
{
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


void                     gsk_gl_glyph_cache_init            (GskGLGlyphCache        *self);
void                     gsk_gl_glyph_cache_free            (GskGLGlyphCache        *self,
                                                             GskGLDriver            *driver);
void                     gsk_gl_glyph_cache_begin_frame     (GskGLGlyphCache        *self,
                                                             GskGLDriver            *driver);
guint                    gsk_gl_glyph_cache_get_glyph_texture_id (GskGLGlyphCache        *self,
                                                             GskGLDriver            *driver,
                                                             const GskGLCachedGlyph *glyph);
const GskGLCachedGlyph * gsk_gl_glyph_cache_lookup          (GskGLGlyphCache        *self,
                                                             gboolean                create,
                                                             PangoFont              *font,
                                                             PangoGlyph              glyph,
                                                             float                   scale);

#endif
