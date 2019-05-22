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
  GskGLDriver *gl_driver;
  GskRenderer *renderer;

  GPtrArray *atlases;
  GHashTable *icons; /* GdkTexture -> IconData */

} GskGLIconCache;

void             gsk_gl_icon_cache_init           (GskGLIconCache        *self,
                                                   GskRenderer            *renderer,
                                                   GskGLDriver            *gl_driver);
void             gsk_gl_icon_cache_free           (GskGLIconCache        *self);
void             gsk_gl_icon_cache_begin_frame    (GskGLIconCache        *self);
void             gsk_gl_icon_cache_lookup_or_add  (GskGLIconCache        *self,
                                                   GdkTexture            *texture,
                                                   int                   *out_texture_id,
                                                   graphene_rect_t       *out_texture_rect);

#endif
