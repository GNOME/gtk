#pragma once

#include "gdkdmabuftexture.h"

#include "gdkdmabuftexturebuilder.h"
#include "gdktextureprivate.h"

G_BEGIN_DECLS

GdkTexture *            gdk_dmabuf_texture_new_from_builder (GdkDmabufTextureBuilder *builder,
                                                             GDestroyNotify           destroy,
                                                             gpointer                 data);

unsigned int            gdk_dmabuf_texture_get_fourcc   (GdkDmabufTexture *texture);
guint64                 gdk_dmabuf_texture_get_modifier (GdkDmabufTexture *texture);
int                     gdk_dmabuf_texture_get_fd       (GdkDmabufTexture *texture,
                                                         int               plane);
unsigned int            gdk_dmabuf_texture_get_offset   (GdkDmabufTexture *texture,
                                                         int               plane);
unsigned int            gdk_dmabuf_texture_get_stride   (GdkDmabufTexture *texture,
                                                         int               plane);

gboolean                gdk_dmabuf_texture_may_support  (unsigned int      fourcc);


G_END_DECLS

