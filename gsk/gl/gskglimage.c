
#include "gskglimageprivate.h"
#include <epoxy/gl.h>

void
gsk_gl_image_create (GskGLImage  *self,
                     GskGLDriver *gl_driver,
                     int          width,
                     int          height)
{
  self->texture_id = gsk_gl_driver_create_texture (gl_driver, width, height);
  self->width = width;
  self->height = height;

  gsk_gl_driver_bind_source_texture (gl_driver, self->texture_id);
  gsk_gl_driver_init_texture_empty (gl_driver, self->texture_id);
  gsk_gl_driver_mark_texture_permanent (gl_driver, self->texture_id);
}

void
gsk_gl_image_destroy (GskGLImage  *self,
                      GskGLDriver *gl_driver)
{
  gsk_gl_driver_destroy_texture (gl_driver, self->texture_id);
  self->texture_id = 0;
}

void
gsk_gl_image_write_to_png (const GskGLImage *self,
                           GskGLDriver      *gl_driver,
                           const char       *filename)
{
  int stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, self->width);
  guchar *data = g_malloc (self->height * stride);
  cairo_surface_t *s;

  gsk_gl_driver_bind_source_texture (gl_driver, self->texture_id);
  glGetTexImage (GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, data);
  s = cairo_image_surface_create_for_data (data, CAIRO_FORMAT_ARGB32, self->width, self->height, stride);
  cairo_surface_write_to_png (s, filename);

  cairo_surface_destroy (s);
  g_free (data);
}

void
gsk_gl_image_upload_region (GskGLImage           *self,
                            GskGLDriver          *gl_driver,
                            const GskImageRegion *region)
{
  gsk_gl_driver_bind_source_texture (gl_driver, self->texture_id);
  glBindTexture (GL_TEXTURE_2D, self->texture_id);

  glTexSubImage2D (GL_TEXTURE_2D, 0, region->x, region->y, region->width, region->height,
                   GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, region->data);

#ifdef G_ENABLE_DEBUG
  /*gsk_gl_driver_bind_source_texture (gl_driver, self->texture_id);*/
  /*gsk_gl_image_dump (self, gl_driver, "/home/baedert/atlases/test_dump.png");*/
#endif
}

