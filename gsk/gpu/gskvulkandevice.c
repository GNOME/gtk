#include "config.h"

#include "gskvulkandeviceprivate.h"

#include "gskgpuglobalsopprivate.h"
#include "gskgpushaderopprivate.h"
#include "gskvulkanbufferprivate.h"
#include "gskvulkanimageprivate.h"

#include "gdk/gdkdisplayprivate.h"
#include "gdk/gdkvulkancontextprivate.h"

#define DESCRIPTOR_POOL_MAXITEMS 50000

struct _GskVulkanDevice
{
  GskGpuDevice parent_instance;

  GskVulkanAllocator *allocators[VK_MAX_MEMORY_TYPES];

  GHashTable *pipeline_cache;
  GHashTable *render_pass_cache;

  VkDescriptorSetLayout vk_descriptor_set_layouts[GSK_VULKAN_N_DESCRIPTOR_SETS];
  VkPipelineLayout vk_pipeline_layout;
  VkCommandPool vk_command_pool;
  VkSampler vk_samplers[GSK_GPU_SAMPLER_N_SAMPLERS];
};

struct _GskVulkanDeviceClass
{
  GskGpuDeviceClass parent_class;
};

G_DEFINE_TYPE (GskVulkanDevice, gsk_vulkan_device, GSK_TYPE_GPU_DEVICE)

typedef struct _PipelineCacheKey PipelineCacheKey;
typedef struct _RenderPassCacheKey RenderPassCacheKey;

struct _PipelineCacheKey
{
  const GskGpuShaderOpClass *op_class;
  GskGpuShaderClip clip;
  VkFormat format;
};

struct _RenderPassCacheKey
{
  VkFormat format;
  VkImageLayout from_layout;
  VkImageLayout to_layout;
};

static guint
pipeline_cache_key_hash (gconstpointer data)
{
  const PipelineCacheKey *key = data;

  return GPOINTER_TO_UINT (key->op_class) ^
         key->clip ^
         (key->format << 2);
}

static gboolean
pipeline_cache_key_equal (gconstpointer a,
                          gconstpointer b)
{
  const PipelineCacheKey *keya = a;
  const PipelineCacheKey *keyb = b;

  return keya->op_class == keyb->op_class &&
         keya->clip == keyb->clip &&
         keya->format == keyb->format;
}

static guint
render_pass_cache_key_hash (gconstpointer data)
{
  const RenderPassCacheKey *key = data;

  return (key->from_layout << 20) ^
         (key->to_layout << 16) ^
         (key->format);
}

static gboolean
render_pass_cache_key_equal (gconstpointer a,
                             gconstpointer b)
{
  const RenderPassCacheKey *keya = a;
  const RenderPassCacheKey *keyb = b;

  return keya->from_layout == keyb->from_layout &&
         keya->to_layout == keyb->to_layout &&
         keya->format == keyb->format;
}

static GskGpuImage *
gsk_vulkan_device_create_offscreen_image (GskGpuDevice   *device,
                                          GdkMemoryDepth  depth,
                                          gsize           width,
                                          gsize           height)
{
  GskVulkanDevice *self = GSK_VULKAN_DEVICE (device);

  return gsk_vulkan_image_new_for_offscreen (self,
                                             GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
                                             width,
                                             height);
}

static GskGpuImage *
gsk_vulkan_device_create_atlas_image (GskGpuDevice *device,
                                      gsize         width,
                                      gsize         height)
{
  GskVulkanDevice *self = GSK_VULKAN_DEVICE (device);

  return gsk_vulkan_image_new_for_atlas (self,
                                         width,
                                         height);
}

static GskGpuImage *
gsk_vulkan_device_create_upload_image (GskGpuDevice    *device,
                                       GdkMemoryFormat  format,
                                       gsize            width,
                                       gsize            height)
{
  GskVulkanDevice *self = GSK_VULKAN_DEVICE (device);

  return gsk_vulkan_image_new_for_upload (self,
                                          format,
                                          width,
                                          height);
}

static void
gsk_vulkan_device_finalize (GObject *object)
{
  GskVulkanDevice *self = GSK_VULKAN_DEVICE (object);
  GskGpuDevice *device = GSK_GPU_DEVICE (self);
  GdkDisplay *display;
  GHashTableIter iter;
  gpointer key, value;
  gsize i;

  g_object_steal_data (G_OBJECT (gsk_gpu_device_get_display (device)), "-gsk-vulkan-device");

  display = gsk_gpu_device_get_display (device);

  g_hash_table_iter_init (&iter, self->pipeline_cache);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      g_free (key);
      vkDestroyPipeline (display->vk_device, value, NULL);
    }
  g_hash_table_unref (self->pipeline_cache);

  g_hash_table_iter_init (&iter, self->render_pass_cache);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      g_free (key);
      vkDestroyRenderPass (display->vk_device, value, NULL);
    }
  g_hash_table_unref (self->render_pass_cache);

  for (i = 0; i < G_N_ELEMENTS (self->vk_samplers); i++)
    {
      vkDestroySampler (display->vk_device,
                        self->vk_samplers[i],
                        NULL);
    }

  vkDestroyPipelineLayout (display->vk_device,
                           self->vk_pipeline_layout,
                           NULL);
  vkDestroyCommandPool (display->vk_device,
                        self->vk_command_pool,
                        NULL);

  for (i = 0; i < GSK_VULKAN_N_DESCRIPTOR_SETS; i++)
    vkDestroyDescriptorSetLayout (display->vk_device,
                                  self->vk_descriptor_set_layouts[i],
                                  NULL);

  for (i = 0; i < VK_MAX_MEMORY_TYPES; i++)
    g_clear_pointer (&self->allocators[i], gsk_vulkan_allocator_unref);

  gdk_display_unref_vulkan (display);

  G_OBJECT_CLASS (gsk_vulkan_device_parent_class)->finalize (object);
}

static void
gsk_vulkan_device_class_init (GskVulkanDeviceClass *klass)
{
  GskGpuDeviceClass *gpu_device_class = GSK_GPU_DEVICE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gpu_device_class->create_offscreen_image = gsk_vulkan_device_create_offscreen_image;
  gpu_device_class->create_atlas_image = gsk_vulkan_device_create_atlas_image;
  gpu_device_class->create_upload_image = gsk_vulkan_device_create_upload_image;

  object_class->finalize = gsk_vulkan_device_finalize;
}

static void
gsk_vulkan_device_init (GskVulkanDevice *self)
{
  self->pipeline_cache = g_hash_table_new (pipeline_cache_key_hash, pipeline_cache_key_equal);
  self->render_pass_cache = g_hash_table_new (render_pass_cache_key_hash, render_pass_cache_key_equal);
}

static void
gsk_vulkan_device_setup (GskVulkanDevice *self)
{
  GdkDisplay *display;

  display = gsk_gpu_device_get_display (GSK_GPU_DEVICE (self));

  GSK_VK_CHECK (vkCreateDescriptorSetLayout, display->vk_device,
                                             &(VkDescriptorSetLayoutCreateInfo) {
                                                 .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                 .bindingCount = 1,
                                                 .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
                                                 .pBindings = (VkDescriptorSetLayoutBinding[1]) {
                                                     {
                                                         .binding = 0,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                         .descriptorCount = DESCRIPTOR_POOL_MAXITEMS,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
                                                     }
                                                 },
                                                 .pNext = &(VkDescriptorSetLayoutBindingFlagsCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                                                   .bindingCount = 1,
                                                   .pBindingFlags = (VkDescriptorBindingFlags[1]) {
                                                     VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
                                                     | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
                                                     | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
                                                   },
                                                 }
                                             },
                                             NULL,
                                             &self->vk_descriptor_set_layouts[GSK_VULKAN_IMAGE_SET_LAYOUT]);

  GSK_VK_CHECK (vkCreateDescriptorSetLayout, display->vk_device,
                                             &(VkDescriptorSetLayoutCreateInfo) {
                                                 .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                 .bindingCount = 1,
                                                 .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
                                                 .pBindings = (VkDescriptorSetLayoutBinding[1]) {
                                                     {
                                                         .binding = 0,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                         .descriptorCount = DESCRIPTOR_POOL_MAXITEMS,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
                                                     },
                                                 },
                                                 .pNext = &(VkDescriptorSetLayoutBindingFlagsCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                                                   .bindingCount = 1,
                                                   .pBindingFlags = (VkDescriptorBindingFlags[1]) {
                                                     VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
                                                     | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
                                                     | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
                                                   },
                                                 }
                                             },
                                             NULL,
                                             &self->vk_descriptor_set_layouts[GSK_VULKAN_BUFFER_SET_LAYOUT]);

  GSK_VK_CHECK (vkCreatePipelineLayout, display->vk_device,
                                        &(VkPipelineLayoutCreateInfo) {
                                            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                            .setLayoutCount = G_N_ELEMENTS (self->vk_descriptor_set_layouts),
                                            .pSetLayouts = self->vk_descriptor_set_layouts,
                                            .pushConstantRangeCount = 1,
                                            .pPushConstantRanges = (VkPushConstantRange[1]) {
                                                {
                                                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                                    .offset = 0,
                                                    .size = sizeof (GskGpuGlobalsInstance)
                                                }
                                            }
                                        },
                                        NULL,
                                        &self->vk_pipeline_layout);

  GSK_VK_CHECK (vkCreateCommandPool, display->vk_device,
                                     &(const VkCommandPoolCreateInfo) {
                                         .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                         .queueFamilyIndex = display->vk_queue_family_index,
                                         .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                     },
                                     NULL,
                                     &self->vk_command_pool);

  GSK_VK_CHECK (vkCreateSampler, display->vk_device,
                                 &(VkSamplerCreateInfo) {
                                     .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                     .magFilter = VK_FILTER_LINEAR,
                                     .minFilter = VK_FILTER_LINEAR,
                                     .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                     .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                     .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                     .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                                     .unnormalizedCoordinates = VK_FALSE,
                                     .maxAnisotropy = 1.0,
                                 },
                                 NULL,
                                 &self->vk_samplers[GSK_GPU_SAMPLER_DEFAULT]);

  GSK_VK_CHECK (vkCreateSampler, display->vk_device,
                                 &(VkSamplerCreateInfo) {
                                     .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                     .magFilter = VK_FILTER_LINEAR,
                                     .minFilter = VK_FILTER_LINEAR,
                                     .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                     .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                     .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                     .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                                     .unnormalizedCoordinates = VK_FALSE,
                                     .maxAnisotropy = 1.0,
                                 },
                                 NULL,
                                 &self->vk_samplers[GSK_GPU_SAMPLER_TRANSPARENT]);

  GSK_VK_CHECK (vkCreateSampler, display->vk_device,
                                 &(VkSamplerCreateInfo) {
                                     .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                     .magFilter = VK_FILTER_LINEAR,
                                     .minFilter = VK_FILTER_LINEAR,
                                     .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                     .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                     .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                     .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                                     .unnormalizedCoordinates = VK_FALSE,
                                     .maxAnisotropy = 1.0,
                                 },
                                 NULL,
                                 &self->vk_samplers[GSK_GPU_SAMPLER_REPEAT]);
  
  GSK_VK_CHECK (vkCreateSampler, display->vk_device,
                                 &(VkSamplerCreateInfo) {
                                     .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                     .magFilter = VK_FILTER_NEAREST,
                                     .minFilter = VK_FILTER_NEAREST,
                                     .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                     .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                     .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                     .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                                     .unnormalizedCoordinates = VK_FALSE,
                                     .maxAnisotropy = 1.0,
                                 },
                                 NULL,
                                 &self->vk_samplers[GSK_GPU_SAMPLER_NEAREST]);
}

GskGpuDevice *
gsk_vulkan_device_get_for_display (GdkDisplay  *display,
                                   GError     **error)
{
  VkPhysicalDeviceProperties vk_props;
  GskVulkanDevice *self;

  self = g_object_get_data (G_OBJECT (display), "-gsk-vulkan-device");
  if (self)
    return GSK_GPU_DEVICE (g_object_ref (self));

  if (!gdk_display_init_vulkan (display, error))
    return NULL;

  self = g_object_new (GSK_TYPE_VULKAN_DEVICE, NULL);

  vkGetPhysicalDeviceProperties (display->vk_physical_device, &vk_props);
  gsk_gpu_device_setup (GSK_GPU_DEVICE (self),
                        display,
                        vk_props.limits.maxImageDimension2D);
  gsk_vulkan_device_setup (self);

  g_object_set_data (G_OBJECT (display), "-gsk-vulkan-device", self);

  return GSK_GPU_DEVICE (self);
}

gsize
gsk_vulkan_device_get_max_descriptors (GskVulkanDevice *self)
{
  return DESCRIPTOR_POOL_MAXITEMS;
}

VkDevice
gsk_vulkan_device_get_vk_device (GskVulkanDevice *self)
{
  return gsk_gpu_device_get_display (GSK_GPU_DEVICE (self))->vk_device;
}

VkPhysicalDevice
gsk_vulkan_device_get_vk_physical_device (GskVulkanDevice *self)
{
  return gsk_gpu_device_get_display (GSK_GPU_DEVICE (self))->vk_physical_device;
}

VkQueue
gsk_vulkan_device_get_vk_queue (GskVulkanDevice *self)
{
  return gsk_gpu_device_get_display (GSK_GPU_DEVICE (self))->vk_queue;
}

VkDescriptorSetLayout
gsk_vulkan_device_get_vk_image_set_layout (GskVulkanDevice *self)
{
  return self->vk_descriptor_set_layouts[GSK_VULKAN_IMAGE_SET_LAYOUT];
}

VkDescriptorSetLayout
gsk_vulkan_device_get_vk_buffer_set_layout (GskVulkanDevice *self)
{
  return self->vk_descriptor_set_layouts[GSK_VULKAN_BUFFER_SET_LAYOUT];
}

VkPipelineLayout
gsk_vulkan_device_get_vk_pipeline_layout (GskVulkanDevice *self)
{
  return self->vk_pipeline_layout;
}

VkCommandPool
gsk_vulkan_device_get_vk_command_pool (GskVulkanDevice *self)
{
  return self->vk_command_pool;
}

VkSampler
gsk_vulkan_device_get_vk_sampler (GskVulkanDevice *self,
                                  GskGpuSampler    sampler)
{
  return self->vk_samplers[sampler];
}

VkRenderPass
gsk_vulkan_device_get_vk_render_pass (GskVulkanDevice *self,
                                      VkFormat         format,
                                      VkImageLayout    from_layout,
                                      VkImageLayout    to_layout)
{
  RenderPassCacheKey cache_key;
  VkRenderPass render_pass;
  GdkDisplay *display;

  cache_key = (RenderPassCacheKey) {
    .format = format,
    .from_layout = from_layout,
    .to_layout = to_layout,
  };
  render_pass = g_hash_table_lookup (self->render_pass_cache, &cache_key);
  if (render_pass)
    return render_pass;

  display = gsk_gpu_device_get_display (GSK_GPU_DEVICE (self));

  GSK_VK_CHECK (vkCreateRenderPass, display->vk_device,
                                    &(VkRenderPassCreateInfo) {
                                        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                        .attachmentCount = 1,
                                        .pAttachments = (VkAttachmentDescription[]) {
                                           {
                                              .format = format,
                                              .samples = VK_SAMPLE_COUNT_1_BIT,
                                              .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                              .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                              .initialLayout = from_layout,
                                              .finalLayout = to_layout
                                           }
                                        },
                                        .subpassCount = 1,
                                        .pSubpasses = (VkSubpassDescription []) {
                                           {
                                              .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                                              .inputAttachmentCount = 0,
                                              .colorAttachmentCount = 1,
                                              .pColorAttachments = (VkAttachmentReference []) {
                                                 {
                                                    .attachment = 0,
                                                     .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                                                  }
                                               },
                                               .pResolveAttachments = (VkAttachmentReference []) {
                                                  {
                                                     .attachment = VK_ATTACHMENT_UNUSED,
                                                     .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                                                  }
                                               },
                                               .pDepthStencilAttachment = NULL,
                                            }
                                         },
                                         .dependencyCount = 0,
                                      },
                                      NULL,
                                      &render_pass);

  g_hash_table_insert (self->render_pass_cache, g_memdup (&cache_key, sizeof (RenderPassCacheKey)), render_pass);

  return render_pass;
}

typedef struct _GskVulkanShaderSpecialization GskVulkanShaderSpecialization;
struct _GskVulkanShaderSpecialization
{
  guint32 clip;
};

VkPipeline
gsk_vulkan_device_get_vk_pipeline (GskVulkanDevice           *self,
                                   const GskGpuShaderOpClass *op_class,
                                   GskGpuShaderClip           clip,
                                   VkFormat                   format,
                                   VkRenderPass               render_pass)
{
  PipelineCacheKey cache_key;
  VkPipeline pipeline;
  GdkDisplay *display;
  char *vertex_shader_name, *fragment_shader_name;

  cache_key = (PipelineCacheKey) {
    .op_class = op_class,
    .clip = clip,
    .format = format,
  };
  pipeline = g_hash_table_lookup (self->pipeline_cache, &cache_key);
  if (pipeline)
    return pipeline;

  display = gsk_gpu_device_get_display (GSK_GPU_DEVICE (self));
  vertex_shader_name = g_strconcat ("/org/gtk/libgsk/shaders/vulkan/", op_class->shader_name, ".vert.spv", NULL);
  fragment_shader_name = g_strconcat ("/org/gtk/libgsk/shaders/vulkan/", op_class->shader_name, ".frag.spv", NULL);

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
                                                       .module = gdk_display_get_vk_shader_module (display, vertex_shader_name),
                                                       .pName = "main",
                                                       .pSpecializationInfo = &(VkSpecializationInfo) {
                                                           .mapEntryCount = 1,
                                                           .pMapEntries = (VkSpecializationMapEntry[1]) {
                                                               {
                                                                   .constantID = 0,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, clip),
                                                                   .size = sizeof (guint32),
                                                               },
                                                           },
                                                           .dataSize = sizeof (GskVulkanShaderSpecialization),
                                                           .pData = &(GskVulkanShaderSpecialization) {
                                                               .clip = clip,
                                                           },
                                                       },
                                                   },
                                                   {
                                                       .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                       .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                       .module = gdk_display_get_vk_shader_module (display, fragment_shader_name),
                                                       .pName = "main",
                                                       .pSpecializationInfo = &(VkSpecializationInfo) {
                                                           .mapEntryCount = 1,
                                                           .pMapEntries = (VkSpecializationMapEntry[1]) {
                                                               {
                                                                   .constantID = 0,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, clip),
                                                                   .size = sizeof (guint32),
                                                               }
                                                           },
                                                           .dataSize = sizeof (GskVulkanShaderSpecialization),
                                                           .pData = &(GskVulkanShaderSpecialization) {
                                                               .clip = clip,
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
                                               .layout = self->vk_pipeline_layout,
                                               .renderPass = render_pass,
                                               .subpass = 0,
                                               .basePipelineHandle = VK_NULL_HANDLE,
                                               .basePipelineIndex = -1,
                                           },
                                           NULL,
                                           &pipeline);

  g_free (fragment_shader_name);
  g_free (vertex_shader_name);

  g_hash_table_insert (self->pipeline_cache, g_memdup (&cache_key, sizeof (PipelineCacheKey)), pipeline);
  //gdk_vulkan_context_pipeline_cache_updated (self->vulkan);

  return pipeline;
}

static GskVulkanAllocator *
gsk_vulkan_device_get_allocator (GskVulkanDevice    *self,
                                 gsize               index,
                                 const VkMemoryType *type)
{
  if (self->allocators[index] == NULL)
    {
      self->allocators[index] = gsk_vulkan_direct_allocator_new (gsk_vulkan_device_get_vk_device (self),
                                                                 index,
                                                                 type);
      self->allocators[index] = gsk_vulkan_buddy_allocator_new (self->allocators[index],
                                                                1024 * 1024);
      //allocators[index] = gsk_vulkan_stats_allocator_new (allocators[index]);
    }

  return self->allocators[index];
}

/* following code found in
 * https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPhysicalDeviceMemoryProperties.html */
GskVulkanAllocator *
gsk_vulkan_device_find_allocator (GskVulkanDevice       *self,
                                  uint32_t               allowed_types,
                                  VkMemoryPropertyFlags  required_flags,
                                  VkMemoryPropertyFlags  desired_flags)
{
  VkPhysicalDeviceMemoryProperties properties;
  uint32_t i, found;

  vkGetPhysicalDeviceMemoryProperties (gsk_vulkan_device_get_vk_physical_device (self),
                                       &properties);

  found = properties.memoryTypeCount;
  for (i = 0; i < properties.memoryTypeCount; i++)
    {
      if (!(allowed_types & (1 << i)))
        continue;

      if ((properties.memoryTypes[i].propertyFlags & required_flags) != required_flags)
        continue;

      found = MIN (i, found);

      if ((properties.memoryTypes[i].propertyFlags & desired_flags) == desired_flags)
        break;
  }

  g_assert (found < properties.memoryTypeCount);

  return gsk_vulkan_allocator_ref (gsk_vulkan_device_get_allocator (self, i, &properties.memoryTypes[i]));
}

