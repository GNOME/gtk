
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

struct _GskGLTextureAtlases
{
  int ref_count;

  GPtrArray *atlases;
};
typedef struct _GskGLTextureAtlases GskGLTextureAtlases;

GskGLTextureAtlases *gsk_gl_texture_atlases_new         (void);
GskGLTextureAtlases *gsk_gl_texture_atlases_ref         (GskGLTextureAtlases *atlases);
void                 gsk_gl_texture_atlases_unref       (GskGLTextureAtlases *atlases);

void                 gsk_gl_texture_atlases_begin_frame (GskGLTextureAtlases *atlases);
gboolean             gsk_gl_texture_atlases_pack        (GskGLTextureAtlases *atlases,
                                                         int                  width,
                                                         int                  height,
                                                         GskGLTextureAtlas  **atlas_out,
                                                         int                 *out_x,
                                                         int                 *out_y);

void        gsk_gl_texture_atlas_init              (GskGLTextureAtlas       *self,
                                                    int                      width,
                                                    int                      height);

void        gsk_gl_texture_atlas_free              (GskGLTextureAtlas       *self);

void        gsk_gl_texture_atlas_realize           (GskGLTextureAtlas       *self);

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
