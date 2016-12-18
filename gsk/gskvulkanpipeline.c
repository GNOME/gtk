#include "config.h"

#include "gskvulkanpipelineprivate.h"

#include "gskvulkanpushconstantsprivate.h"
#include "gskvulkanshaderprivate.h"

#include <graphene.h>

typedef struct _GskVulkanPipelinePrivate GskVulkanPipelinePrivate;

struct _GskVulkanPipelineLayout
{
  volatile gint ref_count;
  GdkVulkanContext *vulkan;

  VkPipelineLayout pipeline_layout;
  VkDescriptorSetLayout descriptor_set_layout;
};

struct _GskVulkanPipelinePrivate
{
  GObject parent_instance;

  GskVulkanPipelineLayout *layout;

  VkPipeline pipeline;

  GskVulkanShader *vertex_shader;
  GskVulkanShader *fragment_shader;
};

G_DEFINE_TYPE_WITH_PRIVATE (GskVulkanPipeline, gsk_vulkan_pipeline, G_TYPE_OBJECT)

static void
gsk_vulkan_pipeline_finalize (GObject *gobject)
{
  GskVulkanPipelinePrivate *priv = gsk_vulkan_pipeline_get_instance_private (GSK_VULKAN_PIPELINE (gobject));

  VkDevice device = gdk_vulkan_context_get_device (priv->layout->vulkan);

  vkDestroyPipeline (device,
                     priv->pipeline,
                     NULL);
  
  g_clear_pointer (&priv->fragment_shader, gsk_vulkan_shader_free);
  g_clear_pointer (&priv->vertex_shader, gsk_vulkan_shader_free);

  g_clear_pointer (&priv->layout, gsk_vulkan_pipeline_layout_unref);

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
                         GskVulkanPipelineLayout *layout,
                         const char              *shader_name,
                         VkRenderPass             render_pass)
{
  GskVulkanPipelinePrivate *priv;
  GskVulkanPipeline *self;
  
  VkDevice device;

  g_return_val_if_fail (g_type_is_a (pipeline_type, GSK_TYPE_VULKAN_PIPELINE), NULL);
  g_return_val_if_fail (layout != NULL, NULL);
  g_return_val_if_fail (shader_name != NULL, NULL);
  g_return_val_if_fail (render_pass != VK_NULL_HANDLE, NULL);

  self = g_object_new (pipeline_type, NULL);

  priv = gsk_vulkan_pipeline_get_instance_private (self);

  priv->layout = gsk_vulkan_pipeline_layout_ref (layout);

  device = gdk_vulkan_context_get_device (layout->vulkan);

  priv->vertex_shader = gsk_vulkan_shader_new_from_resource (layout->vulkan, GSK_VULKAN_SHADER_VERTEX, shader_name, NULL);
  priv->fragment_shader = gsk_vulkan_shader_new_from_resource (layout->vulkan, GSK_VULKAN_SHADER_FRAGMENT, shader_name, NULL);

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
                                                           .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                                                           .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                                                           .alphaBlendOp = VK_BLEND_OP_ADD,
                                                           .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                                           .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
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
                                               .layout = gsk_vulkan_pipeline_layout_get_pipeline_layout (priv->layout),
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

/*** GskVulkanPipelineLayout ***/

GskVulkanPipelineLayout *
gsk_vulkan_pipeline_layout_new (GdkVulkanContext *context)
{
  GskVulkanPipelineLayout *self;
  VkDevice device;

  self = g_slice_new0 (GskVulkanPipelineLayout);
  self->ref_count = 1;
  self->vulkan = g_object_ref (context);

  device = gdk_vulkan_context_get_device (context);

  GSK_VK_CHECK (vkCreateDescriptorSetLayout, device,
                                             &(VkDescriptorSetLayoutCreateInfo) {
                                                 .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                 .bindingCount = 1,
                                                 .pBindings = (VkDescriptorSetLayoutBinding[1]) {
                                                     {
                                                         .binding = 0,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                         .descriptorCount = 1,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
                                                     }
                                                 }
                                             },
                                             NULL,
                                             &self->descriptor_set_layout);

  GSK_VK_CHECK (vkCreatePipelineLayout, device,
                                        &(VkPipelineLayoutCreateInfo) {
                                            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                            .setLayoutCount = 1,
                                            .pSetLayouts = &self->descriptor_set_layout,
                                            .pushConstantRangeCount = gst_vulkan_push_constants_get_range_count (),
                                            .pPushConstantRanges = gst_vulkan_push_constants_get_ranges ()
                                        },
                                        NULL,
                                        &self->pipeline_layout);

  return self;
}

GskVulkanPipelineLayout *
gsk_vulkan_pipeline_layout_ref (GskVulkanPipelineLayout *self)
{
  self->ref_count++;

  return self;
}

void
gsk_vulkan_pipeline_layout_unref (GskVulkanPipelineLayout *self)
{
  VkDevice device;

  self->ref_count--;

  if (self->ref_count > 0)
    return;

  device = gdk_vulkan_context_get_device (self->vulkan);

  vkDestroyPipelineLayout (device,
                           self->pipeline_layout,
                           NULL);

  vkDestroyDescriptorSetLayout (device,
                                self->descriptor_set_layout,
                                NULL);

  g_slice_free (GskVulkanPipelineLayout, self);
}


VkPipelineLayout
gsk_vulkan_pipeline_layout_get_pipeline_layout (GskVulkanPipelineLayout *self)
{
  return self->pipeline_layout;
}

VkDescriptorSetLayout
gsk_vulkan_pipeline_layout_get_descriptor_set_layout (GskVulkanPipelineLayout *self)
{
  return self->descriptor_set_layout;
}

