#pragma once

#include "gskgpubufferprivate.h"

#include "gskgldeviceprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_GL_MAP_BUFFER (gsk_gl_map_buffer_get_type ())

G_DECLARE_FINAL_TYPE (GskGLMapBuffer, gsk_gl_map_buffer, GSK, GL_MAP_BUFFER, GskGpuBuffer)

GskGpuBuffer *          gsk_gl_map_buffer_new                           (GLenum                  target,
                                                                         gsize                   size,
                                                                         GLenum                  access);

void                    gsk_gl_map_buffer_bind                          (GskGLMapBuffer         *self);
void                    gsk_gl_map_buffer_bind_base                     (GskGLMapBuffer         *self,
                                                                         GLuint                  index);

G_END_DECLS

