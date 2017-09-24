#include "config.h"

#include "gskvulkanpipelineprivate.h"

#include "gskvulkanpushconstantsprivate.h"
#include "gskvulkanshaderprivate.h"

#include <graphene.h>

typedef struct _GskVulkanPipelinePrivate GskVulkanPipelinePrivate;

struct _GskVulkanPipelinePrivate
{
  GObject parent_instance;

  GdkVulkanContext *context;

  VkPipeline pipeline;
  VkPipelineLayout layout;

  GskVulkanShader *vertex_shader;
  GskVulkanShader *fragment_shader;
};

G_DEFINE_TYPE_WITH_PRIVATE (GskVulkanPipeline, gsk_vulkan_pipeline, G_TYPE_OBJECT)

static void
gsk_vulkan_pipeline_finalize (GObject *gobject)
{
  GskVulkanPipelinePrivate *priv = gsk_vulkan_pipeline_get_instance_private (GSK_VULKAN_PIPELINE (gobject));
  VkDevice device;

  device = gdk_vulkan_context_get_device (priv->context);

  vkDestroyPipeline (device,
                     priv->pipeline,
                     NULL);

  g_clear_pointer (&priv->fragment_shader, gsk_vulkan_shader_free);
  g_clear_pointer (&priv->vertex_shader, gsk_vulkan_shader_free);

  G_OBJECT_CLASS (gsk_vulkan_pipeline_parent_class)->finalize (gobject);
}

static void
gsk_vulkan_pipeline_class_init (GskVulkanPipelineClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = gsk_vulkan_pipeline_finalize;
}

static void
gsk_vulkan_pipeline_init (GskVulkanPipeline *self)
{
}

GskVulkanPipeline *
gsk_vulkan_pipeline_new (GType                    pipeline_type,
                         GdkVulkanContext        *context,
                         VkPipelineLayout         layout,
                         const char              *shader_name,
                         VkRenderPass             render_pass)
{
  return gsk_vulkan_pipeline_new_full (pipeline_type, context, layout,
                                       shader_name,
                                       render_pass,
                                       VK_BLEND_FACTOR_ONE,
                                       VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
}

GskVulkanPipeline *
gsk_vulkan_pipeline_new_full (GType                    pipeline_type,
                              GdkVulkanContext        *context,
                              VkPipelineLayout         layout,
                              const char              *shader_name,
                              VkRenderPass             render_pass,
                              VkBlendFactor            srcBlendFactor,
                              VkBlendFactor            dstBlendFactor)
{
  GskVulkanShader *vertex_shader;
  GskVulkanShader *fragment_shader;

  vertex_shader = gsk_vulkan_shader_new_from_resource (context,
                                                       GSK_VULKAN_SHADER_VERTEX,
                                                       shader_name,
                                                       NULL);
  fragment_shader = gsk_vulkan_shader_new_from_resource (context,
                                                         GSK_VULKAN_SHADER_FRAGMENT,
                                                         shader_name,
                                                         NULL);

  return gsk_vulkan_pipeline_new_with_shaders (pipeline_type, context, layout,
                                               vertex_shader, fragment_shader,
                                               render_pass,
                                               srcBlendFactor, dstBlendFactor);
}

GskVulkanPipeline *
gsk_vulkan_pipeline_new_with_shaders (GType                    pipeline_type,
                                      GdkVulkanContext        *context,
                                      VkPipelineLayout         layout,
                                      GskVulkanShader         *vertex_shader,
                                      GskVulkanShader         *fragment_shader,
                                      VkRenderPass             render_pass,
                                      VkBlendFactor            srcBlendFactor,
                                      VkBlendFactor            dstBlendFactor)
{
  GskVulkanPipelinePrivate *priv;
  GskVulkanPipeline *self;
  VkDevice device;

  g_return_val_if_fail (g_type_is_a (pipeline_type, GSK_TYPE_VULKAN_PIPELINE), NULL);
  g_return_val_if_fail (layout != VK_NULL_HANDLE, NULL);
  g_return_val_if_fail (vertex_shader != NULL, NULL);
  g_return_val_if_fail (fragment_shader != NULL, NULL);
  g_return_val_if_fail (render_pass != VK_NULL_HANDLE, NULL);

  self = g_object_new (pipeline_type, NULL);

  priv = gsk_vulkan_pipeline_get_instance_private (self);

  device = gdk_vulkan_context_get_device (context);

  priv->context = context;
  priv->layout = layout;

  priv->vertex_shader = vertex_shader;
  priv->fragment_shader = fragment_shader;

  GSK_VK_CHECK (vkCreateGraphicsPipelines, device,
                                           VK_NULL_HANDLE,
                                           1,
                                           &(VkGraphicsPipelineCreateInfo) {
                                               .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                               .stageCount = 2,
                                               .pStages = (VkPipelineShaderStageCreateInfo[2]) {
                                                   GST_VULKAN_SHADER_STAGE_CREATE_INFO (priv->vertex_shader),
                                                   GST_VULKAN_SHADER_STAGE_CREATE_INFO (priv->fragment_shader)
                                               },
                                               .pVertexInputState = GSK_VULKAN_PIPELINE_GET_CLASS (self)->get_input_state_create_info (self),
                                               .pInputAssemblyState = &(VkPipelineInputAssemblyStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                                                   .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                                   .primitiveRestartEnable = VK_FALSE,
                                               },
                                               .pTessellationState = NULL,
                                               .pViewportState = &(VkPipelineViewportStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                                                   .viewportCount = 1,
                                                   .scissorCount = 1
                                               },
                                               .pRasterizationState = &(VkPipelineRasterizationStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                                   .depthClampEnable = VK_FALSE,
                                                   .rasterizerDiscardEnable = VK_FALSE,
                                                   .polygonMode = VK_POLYGON_MODE_FILL,
                                                   .cullMode = VK_CULL_MODE_BACK_BIT,
                                                   .frontFace = VK_FRONT_FACE_CLOCKWISE,
                                                   .lineWidth = 1.0f,
                                               },
                                               .pMultisampleState = &(VkPipelineMultisampleStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                                   .rasterizationSamples = 1,
                                               },
                                               .pDepthStencilState = &(VkPipelineDepthStencilStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO
                                               },
                                               .pColorBlendState = &(VkPipelineColorBlendStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                                   .attachmentCount = 1,
                                                   .pAttachments = (VkPipelineColorBlendAttachmentState []) {
                                                       {
                                                           .blendEnable = VK_TRUE,
                                                           .colorBlendOp = VK_BLEND_OP_ADD,
                                                           .srcColorBlendFactor = srcBlendFactor,
                                                           .dstColorBlendFactor = dstBlendFactor,
                                                           .alphaBlendOp = VK_BLEND_OP_ADD,
                                                           .srcAlphaBlendFactor = srcBlendFactor,
                                                           .dstAlphaBlendFactor = dstBlendFactor,
                                                           .colorWriteMask = VK_COLOR_COMPONENT_A_BIT
                                                                           | VK_COLOR_COMPONENT_R_BIT
                                                                           | VK_COLOR_COMPONENT_G_BIT
                                                                           | VK_COLOR_COMPONENT_B_BIT
                                                       },
                                                   }
                                               },
                                               .pDynamicState = &(VkPipelineDynamicStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                   .dynamicStateCount = 2,
                                                   .pDynamicStates = (VkDynamicState[2]) {
                                                       VK_DYNAMIC_STATE_VIEWPORT,
                                                       VK_DYNAMIC_STATE_SCISSOR
                                                   },
                                               },
                                               .layout = priv->layout,
                                               .renderPass = render_pass,
                                               .subpass = 0,
                                               .basePipelineHandle = VK_NULL_HANDLE,
                                               .basePipelineIndex = -1,
                                           },
                                           NULL,
                                           &priv->pipeline);

  return self;
}

VkPipeline
gsk_vulkan_pipeline_get_pipeline (GskVulkanPipeline *self)
{
  GskVulkanPipelinePrivate *priv = gsk_vulkan_pipeline_get_instance_private (self);

  return priv->pipeline;
}

VkPipelineLayout
gsk_vulkan_pipeline_get_pipeline_layout (GskVulkanPipeline *self)
{
  GskVulkanPipelinePrivate *priv = gsk_vulkan_pipeline_get_instance_private (self);

  return priv->layout;
}
