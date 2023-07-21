#pragma once

#include <gdk/gdk.h>
#include <gsk/gskrendernode.h>

#include "gskvulkanbufferprivate.h"
#include "gskvulkanprivate.h"
#include "gskvulkanrenderprivate.h"

G_BEGIN_DECLS


GskVulkanRenderPass *   gsk_vulkan_render_pass_new                      (void);

void                    gsk_vulkan_render_pass_free                     (GskVulkanRenderPass    *self);

void                    gsk_vulkan_render_pass_add                      (GskVulkanRenderPass    *self,
                                                                         GskVulkanRender        *render,
                                                                         int                     width,
                                                                         int                     height,
                                                                         cairo_rectangle_int_t  *clip,
                                                                         GskRenderNode          *node,
                                                                         const graphene_rect_t  *viewport);

G_END_DECLS

