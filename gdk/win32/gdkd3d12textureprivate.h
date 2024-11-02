#pragma once

#include "gdkd3d12texture.h"

#include "gdkd3d12texturebuilder.h"

G_BEGIN_DECLS

GdkTexture *            gdk_d3d12_texture_new_from_builder              (GdkD3D12TextureBuilder *builder,
                                                                         GDestroyNotify          destroy,
                                                                         gpointer                data,
                                                                         GError                **error);

ID3D12Resource *        gdk_d3d12_texture_get_resource                  (GdkD3D12Texture        *self);
HANDLE                  gdk_d3d12_texture_get_resource_handle           (GdkD3D12Texture        *self);

guint                   gdk_d3d12_texture_import_gl                     (GdkD3D12Texture        *self,
                                                                         GdkGLContext           *context,
                                                                         guint                  *out_mem_id);

G_END_DECLS

