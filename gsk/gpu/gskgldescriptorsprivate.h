#pragma once

#include "gskgpudescriptorsprivate.h"

#include "gskgldeviceprivate.h"

G_BEGIN_DECLS

typedef struct _GskGLDescriptors GskGLDescriptors;

#define GSK_GL_DESCRIPTORS(d) ((GskGLDescriptors *) (d))


GskGpuDescriptors *               gsk_gl_descriptors_new                (GskGLDevice            *device);

guint                             gsk_gl_descriptors_get_n_external     (GskGLDescriptors       *self);
void                              gsk_gl_descriptors_use                (GskGLDescriptors       *self);

G_END_DECLS

