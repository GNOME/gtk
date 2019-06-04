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
  GdkDisplay *display;
  GskGLDriver *gl_driver;

  GPtrArray *atlases;
  GHashTable *icons; /* GdkTexture -> IconData */

} GskGLIconCache;

GskGLIconCache * gsk_gl_icon_cache_new            (GdkDisplay *display);
void             gsk_gl_icon_cache_free           (GskGLIconCache        *self);
void             gsk_gl_icon_cache_begin_frame    (GskGLIconCache        *self);
void             gsk_gl_icon_cache_lookup_or_add  (GskGLIconCache        *self,
                                                   GdkTexture            *texture,
                                                   int                   *out_texture_id,
                                                   graphene_rect_t       *out_texture_rect);

#endif
