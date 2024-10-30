#pragma once

#include "gdkd3d12texture.h"

#include "gdkd3d12texturebuilder.h"

G_BEGIN_DECLS

GdkTexture *            gdk_d3d12_texture_new_from_builder  (GdkD3D12TextureBuilder *builder,
                                                             GDestroyNotify           destroy,
                                                             gpointer                 data,
                                                             GError                 **error);

G_END_DECLS

