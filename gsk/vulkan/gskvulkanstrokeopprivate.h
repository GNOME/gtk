#pragma once

#include "gskvulkanopprivate.h"

#include "gskenums.h"

G_BEGIN_DECLS

void                    gsk_vulkan_stroke_op                            (GskVulkanRender                *render,
                                                                         GskVulkanShaderClip             clip,
                                                                         const graphene_point_t         *offset,
                                                                         const graphene_rect_t          *rect,
                                                                         GskPath                        *path,
                                                                         float                           line_width,
                                                                         GskLineCap                      line_cap,
                                                                         GskLineJoin                     line_join,
                                                                         float                           miter_limit,
                                                                         const GdkRGBA                  *color);


G_END_DECLS

