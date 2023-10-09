#pragma once

#include "gdkdmabuftexturebuilder.h"
#include "gdktextureprivate.h"

G_BEGIN_DECLS

#define MAX_DMABUF_PLANES 4

int *                   gdk_dmabuf_texture_builder_get_fds      (GdkDmabufTextureBuilder *builder);
unsigned int *          gdk_dmabuf_texture_builder_get_offsets  (GdkDmabufTextureBuilder *builder);
unsigned int *          gdk_dmabuf_texture_builder_get_strides  (GdkDmabufTextureBuilder *builder);

G_END_DECLS

