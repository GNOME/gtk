#pragma once

#include "gdkdmabuftexture.h"

#include "gdkdmabuftexturebuilder.h"
#include "gdkdmabufprivate.h"

G_BEGIN_DECLS

GdkTexture *            gdk_dmabuf_texture_new_from_builder (GdkDmabufTextureBuilder *builder,
                                                             GDestroyNotify           destroy,
                                                             gpointer                 data,
                                                             GError                 **error);

GdkDisplay *            gdk_dmabuf_texture_get_display      (GdkDmabufTexture        *self);
const GdkDmabuf *       gdk_dmabuf_texture_get_dmabuf       (GdkDmabufTexture        *self);

G_END_DECLS

