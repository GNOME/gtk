#pragma once

#include "gdkdmabuftexture.h"

#include "gdkdmabuftexturebuilder.h"
#include "gdkdmabufformatsprivate.h"
#include "gdktextureprivate.h"
#include "gdkdisplay.h"

G_BEGIN_DECLS

#define MAX_DMABUF_PLANES 4

int *                   gdk_dmabuf_texture_builder_get_fds      (GdkDmabufTextureBuilder *builder);
unsigned int *          gdk_dmabuf_texture_builder_get_offsets  (GdkDmabufTextureBuilder *builder);
unsigned int *          gdk_dmabuf_texture_builder_get_strides  (GdkDmabufTextureBuilder *builder);

GdkTexture *            gdk_dmabuf_texture_new_from_builder (GdkDmabufTextureBuilder *builder,
                                                             GDestroyNotify           destroy,
                                                             gpointer                 data);

guint32                 gdk_dmabuf_texture_get_fourcc   (GdkDmabufTexture *texture);
guint64                 gdk_dmabuf_texture_get_modifier (GdkDmabufTexture *texture);
unsigned int            gdk_dmabuf_texture_get_n_planes (GdkDmabufTexture *texture);
int *                   gdk_dmabuf_texture_get_fds      (GdkDmabufTexture *texture);
unsigned int *          gdk_dmabuf_texture_get_offsets  (GdkDmabufTexture *texture);
unsigned int *          gdk_dmabuf_texture_get_strides  (GdkDmabufTexture *texture);

GdkDmabufFormat *       gdk_dmabuf_texture_get_supported_formats (gsize *n_formats);


G_END_DECLS

