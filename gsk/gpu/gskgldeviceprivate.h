#pragma once

#include "gskgpudeviceprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_GL_DEVICE (gsk_gl_device_get_type ())

G_DECLARE_FINAL_TYPE (GskGLDevice, gsk_gl_device, GSK, GL_DEVICE, GskGpuDevice)

GskGpuDevice *          gsk_gl_device_get_for_display                   (GdkDisplay             *display,
                                                                         GError                **error);

void                    gsk_gl_device_use_program                       (GskGLDevice            *self,
                                                                         const GskGpuShaderOpClass *op_class,
                                                                         GskGpuShaderFlags       flags,
                                                                         GskGpuColorStates       color_states,
                                                                         guint32                 variation);

GLuint                  gsk_gl_device_get_sampler_id                    (GskGLDevice            *self,
                                                                         GskGpuSampler           sampler);

void                    gsk_gl_device_find_gl_format                    (GskGLDevice            *self,
                                                                         GdkMemoryFormat         format,
                                                                         GskGpuImageFlags        required_flags,
                                                                         GdkMemoryFormat        *out_format,
                                                                         GskGpuImageFlags       *out_flags,
                                                                         GLint                  *out_gl_internal_format,
                                                                         GLint                  *out_gl_internal_srgb_format,
                                                                         GLenum                 *out_gl_format,
                                                                         GLenum                 *out_gl_type,
                                                                         GdkSwizzle             *out_swizzle);

G_END_DECLS
