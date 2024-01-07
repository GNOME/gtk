#pragma once

#include "gskgpubufferprivate.h"

#include "gskgldeviceprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_GL_BUFFER (gsk_gl_buffer_get_type ())

G_DECLARE_FINAL_TYPE (GskGLBuffer, gsk_gl_buffer, GSK, GL_BUFFER, GskGpuBuffer)

GskGpuBuffer *          gsk_gl_buffer_new                               (GLenum                  target,
                                                                         gsize                   size,
                                                                         GLenum                  access);

void                    gsk_gl_buffer_bind                              (GskGLBuffer            *self);
void                    gsk_gl_buffer_bind_base                         (GskGLBuffer            *self,
                                                                         GLuint                  index);

G_END_DECLS

