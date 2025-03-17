#pragma once

#include <gtk/gtk.h>

#define UDMABUF_STRIDE_ALIGN 256U

gboolean        udmabuf_initialize              (GError        **error);

GdkTexture *    udmabuf_texture_new             (gsize           width,
                                                 gsize           height,
                                                 guint           fourcc,
                                                 GdkColorState  *color_state,
                                                 gboolean        premultiplied,
                                                 GBytes         *bytes,
                                                 gsize           stride,
                                                 GError        **error);

GdkTexture *    udmabuf_texture_from_texture    (GdkTexture     *texture,
                                                 GError        **error);
