#ifndef __GSK_GL_DRIVER_PRIVATE_H__
#define __GSK_GL_DRIVER_PRIVATE_H__

#include <cairo.h>
#include <gdk/gdk.h>
#include <graphene.h>

G_BEGIN_DECLS

#define GSK_TYPE_GL_DRIVER (gsk_gl_driver_get_type ())

G_DECLARE_FINAL_TYPE (GskGLDriver, gsk_gl_driver, GSK, GL_DRIVER, GObject)

typedef struct {
  float position[2];
  float uv[2];
} GskQuadVertex;

GskGLDriver *   gsk_gl_driver_new                       (GdkGLContext    *context);

void            gsk_gl_driver_begin_frame               (GskGLDriver     *driver);
void            gsk_gl_driver_end_frame                 (GskGLDriver     *driver);

int             gsk_gl_driver_create_texture            (GskGLDriver     *driver,
                                                         int              width,
                                                         int              height,
                                                         int              min_filter,
                                                         int              mag_filter);
int             gsk_gl_driver_create_vao_for_quad       (GskGLDriver     *driver,
                                                         int              position_id,
                                                         int              uv_id,
                                                         int              n_vertices,
                                                         GskQuadVertex   *vertices);

void            gsk_gl_driver_bind_source_texture       (GskGLDriver     *driver,
                                                         int              texture_id);
void            gsk_gl_driver_bind_mask_texture         (GskGLDriver     *driver,
                                                         int              texture_id);
void            gsk_gl_driver_bind_vao                  (GskGLDriver     *driver,
                                                         int              vao_id);

void            gsk_gl_driver_render_surface_to_texture (GskGLDriver     *driver,
                                                         cairo_surface_t *surface,
                                                         int              texture_id);

G_END_DECLS

#endif /* __GSK_GL_DRIVER_PRIVATE_H__ */
