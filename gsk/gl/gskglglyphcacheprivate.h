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

  int timestamp;
} GskGLGlyphCache;

struct _CacheKeyData
{
  PangoFont *font;
  PangoGlyph glyph;
  guint xshift : 3;
  guint yshift : 3;
  guint scale  : 26; /* times 1024 */
};

typedef struct _CacheKeyData CacheKeyData;

struct _GlyphCacheKey
{
  CacheKeyData data;
  guint hash;
};

typedef struct _GlyphCacheKey GlyphCacheKey;

#define PHASE(x) ((int)(floor (4 * (x + 0.125)) - 4 * floor (x + 0.125)))

static inline void
glyph_cache_key_set_glyph_and_shift (GlyphCacheKey *key,
                                     PangoGlyph glyph,
                                     float x,
                                     float y)
{
  key->data.glyph = glyph;
  key->data.xshift = PHASE (x);
  key->data.yshift = PHASE (y);
  key->hash = GPOINTER_TO_UINT (key->data.font) ^
              key->data.glyph ^
              (key->data.xshift << 24) ^
              (key->data.yshift << 26) ^
              key->data.scale;
}

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

  guint accessed : 1; /* accessed since last check */
  guint used     : 1; /* accounted as used in the atlas */
};


GskGLGlyphCache *        gsk_gl_glyph_cache_new             (GdkDisplay *display,
                                                             GskGLTextureAtlases *atlases);
GskGLGlyphCache *        gsk_gl_glyph_cache_ref             (GskGLGlyphCache *self);
void                     gsk_gl_glyph_cache_unref           (GskGLGlyphCache        *self);
void                     gsk_gl_glyph_cache_begin_frame     (GskGLGlyphCache        *self,
                                                             GskGLDriver            *driver,
                                                             GPtrArray              *removed_atlases);
void                     gsk_gl_glyph_cache_lookup_or_add   (GskGLGlyphCache        *self,
                                                             GlyphCacheKey          *lookup,
                                                             GskGLDriver            *driver,
                                                             const GskGLCachedGlyph **cached_glyph_out);

#endif
