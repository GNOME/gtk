#pragma once

#include "gdkdmabuftexturebuilder.h"
#include "gdkdmabufprivate.h"

G_BEGIN_DECLS

const GdkDmabuf *       gdk_dmabuf_texture_builder_get_dmabuf   (GdkDmabufTextureBuilder *builder);

void                    gdk_dmabuf_texture_builder_set_dmabuf   (GdkDmabufTextureBuilder *builder,
                                                                 const GdkDmabuf         *dmabuf);

G_END_DECLS

