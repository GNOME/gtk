#pragma once

#include "gskgpuimageprivate.h"

#include "gskgldeviceprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_GL_IMAGE (gsk_gl_image_get_type ())

G_DECLARE_FINAL_TYPE (GskGLImage, gsk_gl_image, GSK, GL_IMAGE, GskGpuImage)

GskGpuImage *           gsk_gl_image_new_backbuffer                     (GskGLDevice            *device,
                                                                         GdkGLContext           *context,
                                                                         GdkMemoryFormat         format,
                                                                         gboolean                is_srgb,
                                                                         gsize                   width,
                                                                         gsize                   height);
GskGpuImage *           gsk_gl_image_new                                (GskGLDevice            *device,
                                                                         GdkMemoryFormat         format,
                                                                         gboolean                try_srgb,
                                                                         GskGpuImageFlags        required_flags,
                                                                         gsize                   width,
                                                                         gsize                   height);
GskGpuImage *           gsk_gl_image_new_for_texture                    (GskGLDevice            *device,
                                                                         GdkTexture             *owner,
                                                                         gsize                   n_textures,
                                                                         GLuint                 *tex_id,
                                                                         GLuint                 *mem_id,
                                                                         GLuint                  semaphore_id,
                                                                         gboolean                take_ownership,
                                                                         GskGpuImageFlags        extra_flags,
                                                                         GskGpuConversion        conv);
                                                                         

void                    gsk_gl_image_bind_textures                      (GskGLImage             *self,
                                                                         GLenum                  target);
void                    gsk_gl_image_bind_framebuffer                   (GskGLImage             *self);
void                    gsk_gl_image_bind_framebuffer_target            (GskGLImage             *self,
                                                                         GLenum                  target);

gboolean                gsk_gl_image_is_flipped                         (GskGLImage             *self);
GLint                   gsk_gl_image_get_gl_internal_format             (GskGLImage             *self,
                                                                         gsize                   nth);
GLenum                  gsk_gl_image_get_gl_format                      (GskGLImage             *self,
                                                                         gsize                   nth);
GLenum                  gsk_gl_image_get_gl_type                        (GskGLImage             *self,
                                                                         gsize                   nth);

GLuint                  gsk_gl_image_get_texture_id                     (GskGLImage             *self,
                                                                         gsize                   nth);
void                    gsk_gl_image_steal_texture_ownership            (GskGLImage             *self);

G_END_DECLS
