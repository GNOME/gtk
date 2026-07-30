#pragma once

#include <gtk/gtk.h>

typedef struct _OverlayConfig OverlayConfig;

struct _OverlayConfig
{
  gboolean include_original;
  GdkRGBA color;
};

void            overlay_config_init                     (OverlayConfig          *self);

GOptionGroup *  overlay_config_create_option_group      (OverlayConfig          *self);

GskRenderNode * overlay_config_render_rect              (const OverlayConfig    *self,
                                                         GskRenderNode          *node,
                                                         const graphene_rect_t  *rect);
GskRenderNode * overlay_config_render_region            (const OverlayConfig    *self,
                                                         GskRenderNode          *node,
                                                         const cairo_region_t   *region);
GskRenderNode * overlay_config_render_path              (const OverlayConfig    *self,
                                                         GskRenderNode          *node,
                                                         GskPath                *path);
