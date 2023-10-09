#pragma once

#include "gdkdmabuftexture.h"

#include "gdkdmabuftexturebuilder.h"
#include "gdktextureprivate.h"

G_BEGIN_DECLS

int *                   gdk_dmabuf_texture_builder_get_fds      (GdkDmabufTextureBuilder *builder);
unsigned int *          gdk_dmabuf_texture_builder_get_offsets  (GdkDmabufTextureBuilder *builder);
unsigned int *          gdk_dmabuf_texture_builder_get_strides  (GdkDmabufTextureBuilder *builder);

GdkTexture *            gdk_dmabuf_texture_new_from_builder (GdkDmabufTextureBuilder *builder,
                                                             GDestroyNotify           destroy,
                                                             gpointer                 data);

unsigned int            gdk_dmabuf_texture_get_fourcc   (GdkDmabufTexture *texture);
guint64                 gdk_dmabuf_texture_get_modifier (GdkDmabufTexture *texture);
unsigned int            gdk_dmabuf_texture_get_n_planes (GdkDmabufTexture *texture);
int *                   gdk_dmabuf_texture_get_fds      (GdkDmabufTexture *texture);
unsigned int *          gdk_dmabuf_texture_get_offsets  (GdkDmabufTexture *texture);
unsigned int *          gdk_dmabuf_texture_get_strides  (GdkDmabufTexture *texture);

gboolean                gdk_dmabuf_texture_may_support  (unsigned int      fourcc);


G_END_DECLS

