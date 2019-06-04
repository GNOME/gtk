
#ifndef __GSK_GL_TEXTURE_ATLAS_H__
#define __GSK_GL_TEXTURE_ATLAS_H__

#include "stb_rect_pack.h"
#include "gskglimageprivate.h"
#include "gskgldriverprivate.h"

struct _GskGLTextureAtlas
{
  struct stbrp_context context;
  struct stbrp_node *nodes;

  int width;
  int height;

  guint texture_id;

  int unused_pixels; /* Pixels of rects that have been used at some point,
                        But are now unused. */

  void *user_data;
};
typedef struct _GskGLTextureAtlas GskGLTextureAtlas;

void        gsk_gl_texture_atlas_init              (GskGLTextureAtlas       *self,
                                                    int                      width,
                                                    int                      height);

void        gsk_gl_texture_atlas_free              (GskGLTextureAtlas       *self);

void        gsk_gl_texture_atlas_mark_unused       (GskGLTextureAtlas       *self,
                                                    int                      width,
                                                    int                      height);

void        gsk_gl_texture_atlas_mark_used         (GskGLTextureAtlas       *self,
                                                    int                      width,
                                                    int                      height);


gboolean    gsk_gl_texture_atlas_pack              (GskGLTextureAtlas       *self,
                                                    int                      width,
                                                    int                      height,
                                                    int                     *out_x,
                                                    int                     *out_y);

double      gsk_gl_texture_atlas_get_unused_ratio  (const GskGLTextureAtlas *self);

#endif
