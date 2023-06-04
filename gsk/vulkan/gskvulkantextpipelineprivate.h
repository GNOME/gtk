#pragma once

#include <graphene.h>

#include "gskvulkanpipelineprivate.h"
#include "gskvulkanrendererprivate.h"

G_BEGIN_DECLS

typedef struct _GskVulkanTextPipelineLayout GskVulkanTextPipelineLayout;

#define GSK_TYPE_VULKAN_TEXT_PIPELINE (gsk_vulkan_text_pipeline_get_type ())

G_DECLARE_FINAL_TYPE (GskVulkanTextPipeline, gsk_vulkan_text_pipeline, GSK, VULKAN_TEXT_PIPELINE, GskVulkanPipeline)

GskVulkanPipeline *     gsk_vulkan_text_pipeline_new                   (GdkVulkanContext              *context,
                                                                        VkPipelineLayout               layout,
                                                                        const char                    *shader_name,
                                                                        VkRenderPass                   render_pass);

void                    gsk_vulkan_text_pipeline_collect_vertex_data   (GskVulkanTextPipeline         *pipeline,
                                                                        guchar                        *data,
                                                                        GskVulkanRenderer             *renderer,
                                                                        const graphene_rect_t         *rect,
                                                                        guint32                        tex_id[2],
                                                                        PangoFont                     *font,
                                                                        guint                          total_glyphs,
                                                                        const PangoGlyphInfo          *glyphs,
                                                                        const GdkRGBA                 *color,
                                                                        const graphene_point_t        *offset,
                                                                        guint                          start_glyph,
                                                                        guint                          num_glyphs,
                                                                        float                          scale);
gsize                   gsk_vulkan_text_pipeline_draw                  (GskVulkanTextPipeline         *pipeline,
                                                                        VkCommandBuffer                command_buffer,
                                                                        gsize                          offset,
                                                                        gsize                          n_commands);

G_END_DECLS

