#ifndef __GSK_GL_ICON_CACHE_PRIVATE_H__
#define __GSK_GL_ICON_CACHE_PRIVATE_H__

#include "gskgldriverprivate.h"
#include "gskglimageprivate.h"
#include "gskrendererprivate.h"
#include "gskgltextureatlasprivate.h"
#include <pango/pango.h>
#include <gdk/gdk.h>

typedef struct
{
  int ref_count;

  GdkDisplay *display;
  GskGLDriver *gl_driver;

  GskGLTextureAtlases *atlases;
  GHashTable *icons; /* GdkTexture -> IconData */

} GskGLIconCache;

GskGLIconCache * gsk_gl_icon_cache_new            (GdkDisplay *display,
                                                   GskGLTextureAtlases *atlases);
GskGLIconCache * gsk_gl_icon_cache_ref            (GskGLIconCache        *self);
void             gsk_gl_icon_cache_unref          (GskGLIconCache        *self);
void             gsk_gl_icon_cache_begin_frame    (GskGLIconCache        *self);
void             gsk_gl_icon_cache_lookup_or_add  (GskGLIconCache        *self,
                                                   GdkTexture            *texture,
                                                   int                   *out_texture_id,
                                                   graphene_rect_t       *out_texture_rect);

#endif
