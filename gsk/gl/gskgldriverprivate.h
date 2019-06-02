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

typedef struct {
  cairo_rectangle_int_t rect;
  guint texture_id;
} TextureSlice;


GskGLDriver *   gsk_gl_driver_new                       (GdkGLContext    *context);
GdkGLContext   *gsk_gl_driver_get_gl_context            (GskGLDriver     *driver);

int             gsk_gl_driver_get_max_texture_size      (GskGLDriver     *driver);

void            gsk_gl_driver_begin_frame               (GskGLDriver     *driver);
void            gsk_gl_driver_end_frame                 (GskGLDriver     *driver);
gboolean        gsk_gl_driver_in_frame                  (GskGLDriver     *driver);
int             gsk_gl_driver_get_texture_for_texture   (GskGLDriver     *driver,
                                                         GdkTexture      *texture,
                                                         int              min_filter,
                                                         int              mag_filter);
int             gsk_gl_driver_get_texture_for_pointer   (GskGLDriver     *driver,
                                                         gpointer         pointer);
void            gsk_gl_driver_set_texture_for_pointer   (GskGLDriver     *driver,
                                                         gpointer         pointer,
                                                         int              texture_id);
int             gsk_gl_driver_create_permanent_texture  (GskGLDriver     *driver,
                                                         float            width,
                                                         float            height);
int             gsk_gl_driver_create_texture            (GskGLDriver     *driver,
                                                         float            width,
                                                         float            height);
int             gsk_gl_driver_create_render_target      (GskGLDriver     *driver,
                                                         int              texture_id,
                                                         gboolean         add_depth_buffer,
                                                         gboolean         add_stencil_buffer);

void            gsk_gl_driver_bind_source_texture       (GskGLDriver     *driver,
                                                         int              texture_id);
gboolean        gsk_gl_driver_bind_render_target        (GskGLDriver     *driver,
                                                         int              texture_id);

void            gsk_gl_driver_init_texture_empty        (GskGLDriver     *driver,
                                                         int              texture_id,
                                                         int              min_filter,
                                                         int              max_filter);
void            gsk_gl_driver_init_texture_with_surface (GskGLDriver     *driver,
                                                         int              texture_id,
                                                         cairo_surface_t *surface,
                                                         int              min_filter,
                                                         int              mag_filter);

void            gsk_gl_driver_destroy_texture           (GskGLDriver     *driver,
                                                         int              texture_id);

int             gsk_gl_driver_collect_textures          (GskGLDriver     *driver);
void            gsk_gl_driver_slice_texture             (GskGLDriver     *self,
                                                         GdkTexture      *texture,
                                                         TextureSlice   **out_slices,
                                                         guint           *out_n_slices);

G_END_DECLS

#endif /* __GSK_GL_DRIVER_PRIVATE_H__ */
