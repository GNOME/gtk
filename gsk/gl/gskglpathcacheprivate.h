#ifndef __GSK_GL_PATH_CACHE_H__
#define __GSK_GL_PATH_CACHE_H__

#include <glib.h>
#include "gskgldriverprivate.h"
#include "gskpath.h"

typedef struct
{
  GHashTable *textures;
} GskGLPathCache;


void gsk_gl_path_cache_init           (GskGLPathCache        *self);
void gsk_gl_path_cache_free           (GskGLPathCache        *self,
                                       GskGLDriver           *gl_driver);
void gsk_gl_path_cache_begin_frame    (GskGLPathCache        *self,
                                       GskGLDriver           *gl_driver);
int  gsk_gl_path_cache_get_texture_id (GskGLPathCache        *self,
                                       GskPath               *path,
                                       GskFillRule            fill_rule,
                                       const GskStroke       *stroke,
                                       float                  scale_x,
                                       float                  scale_y,
                                       graphene_rect_t       *out_bounds);
void gsk_gl_path_cache_commit         (GskGLPathCache        *self,
                                       GskPath               *path,
                                       GskFillRule            fill_rule,
                                       const GskStroke       *stroke,
                                       float                  scale_x,
                                       float                  scale_y,
                                       int                    texture_id,
                                       const graphene_rect_t *bounds);

#endif
