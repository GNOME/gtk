#pragma once

#include <gdk/gdk.h>
#include <gsk/gskrendernode.h>

#include "gskvulkanbufferprivate.h"
#include "gskvulkanprivate.h"
#include "gskvulkanrenderprivate.h"

G_BEGIN_DECLS


GskVulkanRenderPass *   gsk_vulkan_render_pass_new                      (GdkVulkanContext       *context,
                                                                         GskVulkanRender        *render,
                                                                         GskVulkanImage         *target,
                                                                         const graphene_vec2_t  *scale,
                                                                         const graphene_rect_t  *viewport,
                                                                         cairo_region_t         *clip,
                                                                         GskRenderNode          *node);

void                    gsk_vulkan_render_pass_free                     (GskVulkanRenderPass    *self);

void                    gsk_vulkan_render_pass_add                      (GskVulkanRenderPass    *self,
                                                                         GskVulkanRender        *render,
                                                                         GskRenderNode          *node);

G_END_DECLS

