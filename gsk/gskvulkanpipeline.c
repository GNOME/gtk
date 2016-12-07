#include "config.h"

#include "gskvulkanpipelineprivate.h"

#include "gskvulkanshaderprivate.h"

#include <graphene.h>

struct _GskVulkanPipeline
{
  GObject parent_instance;

  GdkVulkanContext *vulkan;

  VkPipeline pipeline;
  VkPipelineLayout pipeline_layout;
  VkDescriptorSetLayout descriptor_set_layout;

  GskVulkanShader *vertex_shader;
  GskVulkanShader *fragment_shader;
};

G_DEFINE_TYPE (GskVulkanPipeline, gsk_vulkan_pipeline, G_TYPE_OBJECT)

static void
gsk_vulkan_pipeline_finalize (GObject *gobject)
{
  GskVulkanPipeline *self = GSK_VULKAN_PIPELINE (gobject);
  VkDevice device = gdk_vulkan_context_get_device (self->vulkan);

  vkDestroyPipeline (device,
                     self->pipeline,
                     NULL);
  
  g_clear_pointer (&self->fragment_shader, gsk_vulkan_shader_free);
  g_clear_pointer (&self->vertex_shader, gsk_vulkan_shader_free);

  vkDestroyPipelineLayout (device,
                           self->pipeline_layout,
                           NULL);

  vkDestroyDescriptorSetLayout (device,
                                self->descriptor_set_layout,
                                NULL);

  g_clear_object (&self->vulkan);

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
gsk_vulkan_pipeline_new (GdkVulkanContext *context,
                         VkRenderPass      render_pass)
{
  GskVulkanPipeline *self;
  VkDevice device;

  g_return_val_if_fail (GDK_IS_VULKAN_CONTEXT (context), NULL);
  g_return_val_if_fail (render_pass != VK_NULL_HANDLE, NULL);

  device = gdk_vulkan_context_get_device (context);

  self = g_object_new (GSK_TYPE_VULKAN_PIPELINE, NULL);

  self->vulkan = g_object_ref (context);

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
                                            .pushConstantRangeCount = 1,
                                            .pPushConstantRanges = (VkPushConstantRange[1]) {
                                                {
                                                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                                                    .offset = 0,
                                                    .size = sizeof (graphene_matrix_t)
                                                }
                                            }
                                        },
                                        NULL,
                                        &self->pipeline_layout);

  self->vertex_shader = gsk_vulkan_shader_new_from_resource (context, GSK_VULKAN_SHADER_VERTEX, "blit", NULL);
  self->fragment_shader = gsk_vulkan_shader_new_from_resource (context, GSK_VULKAN_SHADER_FRAGMENT, "blit", NULL);

  GSK_VK_CHECK (vkCreateGraphicsPipelines, device,
                                           VK_NULL_HANDLE,
                                           1,
                                           &(VkGraphicsPipelineCreateInfo) {
                                               .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                               .stageCount = 2,
                                               .pStages = (VkPipelineShaderStageCreateInfo[2]) {
                                                   GST_VULKAN_SHADER_STAGE_CREATE_INFO (self->vertex_shader),
                                                   GST_VULKAN_SHADER_STAGE_CREATE_INFO (self->fragment_shader)
                                               },
                                               .pVertexInputState = &(VkPipelineVertexInputStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                                                   .vertexBindingDescriptionCount = 1,
                                                   .pVertexBindingDescriptions = (VkVertexInputBindingDescription[]) {
                                                       {
                                                           .binding = 0,
                                                           .stride = 4 * sizeof (float),
                                                           .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
                                                       }
                                                   },
                                                   .vertexAttributeDescriptionCount = 2,
                                                   .pVertexAttributeDescriptions = (VkVertexInputAttributeDescription[]) {
                                                       {
                                                           .location = 0,
                                                           .binding = 0,
                                                           .format = VK_FORMAT_R32G32_SFLOAT,
                                                           .offset = 0,
                                                       },
                                                       {
                                                           .location = 1,
                                                           .binding = 0,
                                                           .format = VK_FORMAT_R32G32_SFLOAT,
                                                           .offset = 2 * sizeof (float),
                                                       }
                                                   }
                                               },
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
                                                       { .colorWriteMask = VK_COLOR_COMPONENT_A_BIT |
                                                                           VK_COLOR_COMPONENT_R_BIT |
                                                                           VK_COLOR_COMPONENT_G_BIT |
                                                                           VK_COLOR_COMPONENT_B_BIT },
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
                                               .layout = self->pipeline_layout,
                                               .renderPass = render_pass,
                                               .subpass = 0,
                                               .basePipelineHandle = VK_NULL_HANDLE,
                                               .basePipelineIndex = -1,
                                           },
                                           NULL,
                                           &self->pipeline);


  return self;
}

VkPipeline
gsk_vulkan_pipeline_get_pipeline (GskVulkanPipeline *self)
{
  return self->pipeline;
}

VkPipelineLayout
gsk_vulkan_pipeline_get_pipeline_layout (GskVulkanPipeline *self)
{
  return self->pipeline_layout;
}

VkDescriptorSetLayout
gsk_vulkan_pipeline_get_descriptor_set_layout (GskVulkanPipeline *self)
{
  return self->descriptor_set_layout;
}

