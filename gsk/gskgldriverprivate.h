#ifndef __GSK_GL_DRIVER_PRIVATE_H__
#define __GSK_GL_DRIVER_PRIVATE_H__

#include <cairo.h>
#include <gdk/gdk.h>
#include <graphene.h>

#include <gsk/gsktexture.h>

G_BEGIN_DECLS

#define GSK_TYPE_GL_DRIVER (gsk_gl_driver_get_type ())

G_DECLARE_FINAL_TYPE (GskGLDriver, gsk_gl_driver, GSK, GL_DRIVER, GObject)

typedef struct {
  float position[2];
  float uv[2];
} GskQuadVertex;

GskGLDriver *   gsk_gl_driver_new                       (GdkGLContext    *context);

int             gsk_gl_driver_get_max_texture_size      (GskGLDriver     *driver);

void            gsk_gl_driver_begin_frame               (GskGLDriver     *driver);
void            gsk_gl_driver_end_frame                 (GskGLDriver     *driver);

int             gsk_gl_driver_get_texture_for_texture   (GskGLDriver     *driver,
                                                         GskTexture      *texture,
                                                         int              min_filter,
                                                         int              mag_filter);
int             gsk_gl_driver_create_texture            (GskGLDriver     *driver,
                                                         int              width,
                                                         int              height);
int             gsk_gl_driver_create_vao_for_quad       (GskGLDriver     *driver,
                                                         int              position_id,
                                                         int              uv_id,
                                                         int              n_vertices,
                                                         GskQuadVertex   *vertices);
int             gsk_gl_driver_create_render_target      (GskGLDriver     *driver,
                                                         int              texture_id,
                                                         gboolean         add_depth_buffer,
                                                         gboolean         add_stencil_buffer);

void            gsk_gl_driver_bind_source_texture       (GskGLDriver     *driver,
                                                         int              texture_id);
void            gsk_gl_driver_bind_mask_texture         (GskGLDriver     *driver,
                                                         int              texture_id);
void            gsk_gl_driver_bind_vao                  (GskGLDriver     *driver,
                                                         int              vao_id);
gboolean        gsk_gl_driver_bind_render_target        (GskGLDriver     *driver,
                                                         int              texture_id);

void            gsk_gl_driver_init_texture_empty        (GskGLDriver     *driver,
                                                         int              texture_id);
void            gsk_gl_driver_init_texture_with_surface (GskGLDriver     *driver,
                                                         int              texture_id,
                                                         cairo_surface_t *surface,
                                                         int              min_filter,
                                                         int              mag_filter);

void            gsk_gl_driver_destroy_texture           (GskGLDriver     *driver,
                                                         int              texture_id);
void            gsk_gl_driver_destroy_vao               (GskGLDriver     *driver,
                                                         int              vao_id);

int             gsk_gl_driver_collect_textures          (GskGLDriver     *driver);
int             gsk_gl_driver_collect_vaos              (GskGLDriver     *driver);

G_END_DECLS

#endif /* __GSK_GL_DRIVER_PRIVATE_H__ */
