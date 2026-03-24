#include "config.h"

#include "gskvulkanpipelineprivate.h"

#include "gskgpucacheprivate.h"
#include "gskgpucachedprivate.h"
#include "gskgpushaderflagsprivate.h"
#include "gskgpushaderopprivate.h"

#include "gdkprofilerprivate.h"

typedef struct _GskVulkanShaderSpecialization GskVulkanShaderSpecialization;

struct _GskVulkanPipeline
{
  GskGpuCached parent;

  const GskGpuShaderOpClass *op_class;
  GskGpuShaderFlags flags;
  GskGpuColorStates color_states;
  guint32 variation;
  GskGpuBlend blend;
  VkFormat vk_format;
  VkPipelineLayout vk_layout;
  VkPipeline vk_pipeline;
};

struct _GskVulkanShaderSpecialization
{
  guint32 flags;
  guint32 color_states;
  guint32 variation;
};

static VkPipelineColorBlendAttachmentState blend_attachment_states[] = {
  [GSK_GPU_BLEND_NONE] = {
    .blendEnable = VK_FALSE,
    .colorWriteMask = VK_COLOR_COMPONENT_A_BIT
                    | VK_COLOR_COMPONENT_R_BIT
                    | VK_COLOR_COMPONENT_G_BIT
                    | VK_COLOR_COMPONENT_B_BIT
  },
  [GSK_GPU_BLEND_OVER] = {
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
  [GSK_GPU_BLEND_ADD] = {
    .blendEnable = VK_TRUE,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .colorWriteMask = VK_COLOR_COMPONENT_A_BIT
                    | VK_COLOR_COMPONENT_R_BIT
                    | VK_COLOR_COMPONENT_G_BIT
                    | VK_COLOR_COMPONENT_B_BIT
  },
  [GSK_GPU_BLEND_CLEAR] = {
    .blendEnable = VK_TRUE,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcColorBlendFactor = VK_BLEND_FACTOR_ZERO,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .colorWriteMask = VK_COLOR_COMPONENT_A_BIT
                    | VK_COLOR_COMPONENT_R_BIT
                    | VK_COLOR_COMPONENT_G_BIT
                    | VK_COLOR_COMPONENT_B_BIT
  },
  [GSK_GPU_BLEND_MASK] = {
    .blendEnable = VK_TRUE,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcColorBlendFactor = VK_BLEND_FACTOR_ZERO,
    .dstColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
    .colorWriteMask = VK_COLOR_COMPONENT_A_BIT
                    | VK_COLOR_COMPONENT_R_BIT
                    | VK_COLOR_COMPONENT_G_BIT
                    | VK_COLOR_COMPONENT_B_BIT
  },
  [GSK_GPU_BLEND_MASK_ONE] = {
    .blendEnable = VK_TRUE,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstColorBlendFactor = VK_BLEND_FACTOR_SRC1_ALPHA,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_SRC1_ALPHA,
    .colorWriteMask = VK_COLOR_COMPONENT_A_BIT
                    | VK_COLOR_COMPONENT_R_BIT
                    | VK_COLOR_COMPONENT_G_BIT
                    | VK_COLOR_COMPONENT_B_BIT
  },
  [GSK_GPU_BLEND_MASK_ALPHA] = {
    .blendEnable = VK_TRUE,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA,
    .dstColorBlendFactor = VK_BLEND_FACTOR_SRC1_ALPHA,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_SRC1_ALPHA,
    .colorWriteMask = VK_COLOR_COMPONENT_A_BIT
                    | VK_COLOR_COMPONENT_R_BIT
                    | VK_COLOR_COMPONENT_G_BIT
                    | VK_COLOR_COMPONENT_B_BIT
  },
  [GSK_GPU_BLEND_MASK_INV_ALPHA] = {
    .blendEnable = VK_TRUE,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
    .dstColorBlendFactor = VK_BLEND_FACTOR_SRC1_ALPHA,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_SRC1_ALPHA,
    .colorWriteMask = VK_COLOR_COMPONENT_A_BIT
                    | VK_COLOR_COMPONENT_R_BIT
                    | VK_COLOR_COMPONENT_G_BIT
                    | VK_COLOR_COMPONENT_B_BIT
  },
};

static void
gsk_vulkan_pipeline_finalize (GskGpuCached *cached)
{
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cached->cache);
  GskVulkanPipeline *self = (GskVulkanPipeline *) cached;
  GskVulkanDevice *device;
  VkDevice vk_device;

  device = GSK_VULKAN_DEVICE (gsk_gpu_cache_get_device (cached->cache));
  vk_device = gsk_vulkan_device_get_vk_device (device);

  g_hash_table_remove (priv->pipeline_cache, self);

  vkDestroyPipeline (vk_device,
                     self->vk_pipeline,
                     NULL);
}

static gboolean
gsk_vulkan_pipeline_should_collect (GskGpuCached *cached,
                                    gint64        cache_timeout,
                                    gint64        timestamp)
{
  return gsk_gpu_cached_is_old (cached, cache_timeout, timestamp);
}

static const GskGpuCachedClass GSK_VULKAN_PIPELINE_CLASS =
{
  sizeof (GskVulkanPipeline),
  "VkPipeline",
  FALSE,
  gsk_gpu_cached_print_no_stats,
  gsk_vulkan_pipeline_finalize,
  gsk_vulkan_pipeline_should_collect
};

static guint
gsk_vulkan_pipeline_hash (gconstpointer data)
{
  const GskVulkanPipeline *self = data;

  return GPOINTER_TO_UINT (self->op_class) ^
         self->flags ^
         (self->color_states << 8) ^
         (self->variation << 16) ^
         (self->blend << 24) ^
         GPOINTER_TO_UINT (self->vk_layout) ^
         (self->vk_format << 21) ^ (self->vk_format >> 11);
}

static gboolean
gsk_vulkan_pipeline_equal (gconstpointer a,
                           gconstpointer b)
{
  const GskVulkanPipeline *selfa = a;
  const GskVulkanPipeline *selfb = b;

  return selfa->op_class == selfb->op_class &&
         selfa->flags == selfb->flags &&
         selfa->color_states == selfb->color_states &&
         selfa->variation == selfb->variation &&
         selfa->blend == selfb->blend &&
         selfa->vk_layout == selfb->vk_layout &&
         selfa->vk_format == selfb->vk_format;
}

void
gsk_vulkan_pipeline_init_cache (GskGpuCache *cache)
{
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cache);

  priv->pipeline_cache = g_hash_table_new (gsk_vulkan_pipeline_hash, gsk_vulkan_pipeline_equal);
}

void
gsk_vulkan_pipeline_finish_cache (GskGpuCache *cache)
{
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cache);

  g_assert (g_hash_table_size (priv->pipeline_cache) == 0);
  g_hash_table_unref (priv->pipeline_cache);
}

static VkShaderModule
gsk_vulkan_pipeline_get_vk_shader_module (GdkDisplay *display,
                                          const char *resource_name)
{
  VkShaderModule *shader;
  GError *error = NULL;
  GBytes *bytes;

  shader = g_hash_table_lookup (display->vk_shader_modules, resource_name);
  if (shader)
    return *shader;

  bytes = g_resources_lookup_data (resource_name, 0, &error);
  if (bytes == NULL)
    {
      GDK_DEBUG (VULKAN, "Error loading shader data: %s", error->message);
      g_clear_error (&error);
      return VK_NULL_HANDLE;
    }

  shader = g_new0 (VkShaderModule, 1);
  if (GDK_VK_CHECK (vkCreateShaderModule, display->vk_device,
                                          &(VkShaderModuleCreateInfo) {
                                              .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                              .codeSize = g_bytes_get_size (bytes),
                                              .pCode = (uint32_t *) g_bytes_get_data (bytes, NULL),
                                          },
                                          NULL,
                                          shader) == VK_SUCCESS)
    {
      g_hash_table_insert (display->vk_shader_modules, g_strdup (resource_name), shader);
    }
  else
    {
      g_free (shader);

      return VK_NULL_HANDLE;
    }

  g_bytes_unref (bytes);

  return *shader;
}

GskVulkanPipeline *
gsk_vulkan_pipeline_get (GskVulkanDevice           *device,
                         VkPipelineLayout           vk_layout,
                         const GskGpuShaderOpClass *op_class,
                         GskGpuShaderFlags          flags,
                         GskGpuColorStates          color_states,
                         guint32                    variation,
                         GskGpuBlend                blend,
                         VkFormat                   vk_format,
                         VkRenderPass               render_pass)
{
  GskGpuCache *cache = gsk_gpu_device_get_cache (GSK_GPU_DEVICE (device));
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cache);
  GskVulkanPipeline *result;
  GdkDisplay *display;
  char *vertex_shader_name, *fragment_shader_name;
  G_GNUC_UNUSED gint64 begin_time = GDK_PROFILER_CURRENT_TIME;
  const char *blend_name[] = { "NONE", "OVER", "ADD", "CLEAR", "MASK", "MASK-ONE", "MASK-ALPHA", "MASK-INV-ALPHA" };

  result = g_hash_table_lookup (priv->pipeline_cache,
                                &(GskVulkanPipeline) {
                                    .vk_layout = vk_layout,
                                    .op_class = op_class,
                                    .color_states = color_states,
                                    .variation = variation,
                                    .flags = flags,
                                    .blend = blend,
                                    .vk_format = vk_format,
                                });
  if (result)
    {
      gsk_gpu_cached_use ((GskGpuCached *) result);
      return result;
    }

  g_assert (blend <= G_N_ELEMENTS (blend_attachment_states));

  display = gsk_gpu_device_get_display (GSK_GPU_DEVICE (device));

  vertex_shader_name = g_strconcat ("/org/gtk/libgsk/shaders/vulkan/",
                                    op_class->shader_name,
                                    ".vert.spv",
                                    NULL);
  fragment_shader_name = g_strconcat ("/org/gtk/libgsk/shaders/vulkan/",
                                      op_class->shader_name,
                                      gsk_gpu_shader_flags_has_clip_mask (flags) ? "-clipmask" : "",
                                      ".frag.spv",
                                      NULL);

  result = gsk_gpu_cached_new (cache, &GSK_VULKAN_PIPELINE_CLASS);
  result->vk_layout = vk_layout;
  result->op_class = op_class,
  result->color_states = color_states,
  result->variation = variation,
  result->flags = flags,
  result->blend = blend,
  result->vk_format = vk_format,

  GSK_VK_CHECK (vkCreateGraphicsPipelines, display->vk_device,
                                           display->vk_pipeline_cache,
                                           1,
                                           &(VkGraphicsPipelineCreateInfo) {
                                               .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                               .stageCount = 2,
                                               .pStages = (VkPipelineShaderStageCreateInfo[2]) {
                                                   {
                                                       .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                       .stage = VK_SHADER_STAGE_VERTEX_BIT,
                                                       .module = gsk_vulkan_pipeline_get_vk_shader_module (display, vertex_shader_name),
                                                       .pName = "main",
                                                       .pSpecializationInfo = &(VkSpecializationInfo) {
                                                           .mapEntryCount = 3,
                                                           .pMapEntries = (VkSpecializationMapEntry[6]) {
                                                               {
                                                                   .constantID = 0,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, flags),
                                                                   .size = sizeof (guint32),
                                                               },
                                                               {
                                                                   .constantID = 1,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, color_states),
                                                                   .size = sizeof (guint32),
                                                               },
                                                               {
                                                                   .constantID = 2,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, variation),
                                                                   .size = sizeof (guint32),
                                                               },
                                                           },
                                                           .dataSize = sizeof (GskVulkanShaderSpecialization),
                                                           .pData = &(GskVulkanShaderSpecialization) {
                                                               .flags = flags,
                                                               .color_states = color_states,
                                                               .variation = variation,
                                                           },
                                                       },
                                                   },
                                                   {
                                                       .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                       .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                       .module = gsk_vulkan_pipeline_get_vk_shader_module (display, fragment_shader_name),
                                                       .pName = "main",
                                                       .pSpecializationInfo = &(VkSpecializationInfo) {
                                                           .mapEntryCount = 3,
                                                           .pMapEntries = (VkSpecializationMapEntry[6]) {
                                                               {
                                                                   .constantID = 0,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, flags),
                                                                   .size = sizeof (guint32),
                                                               },
                                                               {
                                                                   .constantID = 1,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, color_states),
                                                                   .size = sizeof (guint32),
                                                               },
                                                               {
                                                                   .constantID = 2,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, variation),
                                                                   .size = sizeof (guint32),
                                                               },
                                                           },
                                                           .dataSize = sizeof (GskVulkanShaderSpecialization),
                                                           .pData = &(GskVulkanShaderSpecialization) {
                                                               .flags = flags,
                                                               .color_states = color_states,
                                                               .variation = variation,
                                                           },
                                                       },
                                                   },
                                               },

                                               .pVertexInputState = op_class->vertex_input_state,
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
                                                   .cullMode = VK_CULL_MODE_NONE,
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
                                                   .pAttachments = &blend_attachment_states[blend],
                                               },
                                               .pDynamicState = &(VkPipelineDynamicStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                   .dynamicStateCount = 2,
                                                   .pDynamicStates = (VkDynamicState[2]) {
                                                       VK_DYNAMIC_STATE_VIEWPORT,
                                                       VK_DYNAMIC_STATE_SCISSOR
                                                   },
                                               },
                                               .layout = vk_layout,
                                               .renderPass = render_pass,
                                               .subpass = 0,
                                               .basePipelineHandle = VK_NULL_HANDLE,
                                               .basePipelineIndex = -1,
                                           },
                                           NULL,
                                           &result->vk_pipeline);

  gdk_profiler_end_markf (begin_time,
                          "Create Vulkan pipeline", "%s color states=%u variation=%u clip=%u blend=%s format=%u",
                          op_class->shader_name,
                          flags,
                          color_states, 
                          variation,
                          blend >= G_N_ELEMENTS (blend_name) ? "FIX THE BLEND NAMES ARRAY!" : blend_name[blend],
                          vk_format);

  GSK_DEBUG (SHADERS,
             "Create Vulkan pipeline (%s, %u/%u/%u/%s/%u)",
             op_class->shader_name,
             flags,
             color_states, 
             variation,
             blend >= G_N_ELEMENTS (blend_name) ? "FIX THE BLEND NAMES ARRAY!" : blend_name[blend],
             vk_format);

  g_free (fragment_shader_name);
  g_free (vertex_shader_name);

  g_hash_table_add (priv->pipeline_cache, result);
  gdk_display_vulkan_pipeline_cache_updated (display);
  gsk_gpu_cached_use ((GskGpuCached *) result);

  return result;
}

VkPipeline
gsk_vulkan_pipeline_get_vk_pipeline (GskVulkanPipeline *self)
{
  return self->vk_pipeline;
}

