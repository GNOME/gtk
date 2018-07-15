

#ifndef __GSK_GL_SHADOW_CACHE_H__
#define __GSK_GL_SHADOW_CACHE_H__

#include <glib.h>
#include "gskgldriverprivate.h"
#include "gskroundedrect.h"

typedef struct
{
  GArray *textures;
} GskGLShadowCache;


void gsk_gl_shadow_cache_init           (GskGLShadowCache     *self);
void gsk_gl_shadow_cache_free           (GskGLShadowCache     *self,
                                         GskGLDriver          *gl_driver);
void gsk_gl_shadow_cache_begin_frame    (GskGLShadowCache     *self,
                                         GskGLDriver          *gl_driver);
int  gsk_gl_shadow_cache_get_texture_id (GskGLShadowCache     *self,
                                         GskGLDriver          *gl_driver,
                                         const GskRoundedRect *shadow_rect,
                                         float                 blur_radius);
void gsk_gl_shadow_cache_commit         (GskGLShadowCache     *self,
                                         const GskRoundedRect *shadow_rect,
                                         float                 blur_radius,
                                         int                   texture_id);


#endif
