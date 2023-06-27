#include "config.h"

#include "gskprivate.h"

#include "gskvulkanrenderprivate.h"

#include "gskrendererprivate.h"
#include "gskvulkanbufferprivate.h"
#include "gskvulkancommandpoolprivate.h"
#include "gskvulkanpipelineprivate.h"
#include "gskvulkanrenderpassprivate.h"

#include "gskvulkanblendmodepipelineprivate.h"
#include "gskvulkanblurpipelineprivate.h"
#include "gskvulkanborderpipelineprivate.h"
#include "gskvulkanboxshadowpipelineprivate.h"
#include "gskvulkancolorpipelineprivate.h"
#include "gskvulkancolortextpipelineprivate.h"
#include "gskvulkancrossfadepipelineprivate.h"
#include "gskvulkaneffectpipelineprivate.h"
#include "gskvulkanlineargradientpipelineprivate.h"
#include "gskvulkantextpipelineprivate.h"
#include "gskvulkantexturepipelineprivate.h"
#include "gskvulkanpushconstantsprivate.h"

#define DESCRIPTOR_POOL_MAXITEMS 50000

#define GDK_ARRAY_NAME gsk_descriptor_image_infos
#define GDK_ARRAY_TYPE_NAME GskDescriptorImageInfos
#define GDK_ARRAY_ELEMENT_TYPE VkDescriptorImageInfo
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 1024
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

#define GDK_ARRAY_NAME gsk_descriptor_buffer_infos
#define GDK_ARRAY_TYPE_NAME GskDescriptorBufferInfos
#define GDK_ARRAY_ELEMENT_TYPE VkDescriptorBufferInfo
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 1024
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

#define N_DESCRIPTOR_SETS 3

struct _GskVulkanRender
{
  GskRenderer *renderer;
  GdkVulkanContext *vulkan;

  double scale;
  graphene_rect_t viewport;
  cairo_region_t *clip;

  GskVulkanCommandPool *command_pool;
  VkFence fence;
  VkDescriptorSetLayout descriptor_set_layouts[N_DESCRIPTOR_SETS];
  VkPipelineLayout pipeline_layout;
  GskVulkanUploader *uploader;

  GskDescriptorImageInfos descriptor_images;
  GskDescriptorImageInfos descriptor_samplers;
  GskDescriptorBufferInfos descriptor_buffers;
  VkDescriptorPool descriptor_pool;
  VkDescriptorSet descriptor_sets[N_DESCRIPTOR_SETS];
  GskVulkanPipeline *pipelines[GSK_VULKAN_N_PIPELINES];

  GskVulkanImage *target;

  VkSampler samplers[3];
  GskVulkanBuffer *storage_buffer;
  guchar *storage_buffer_memory;
  gsize storage_buffer_used;

  GList *render_passes;
  GSList *cleanup_images;

  GQuark render_pass_counter;
  GQuark gpu_time_timer;
};

static void
gsk_vulkan_render_setup (GskVulkanRender       *self,
                         GskVulkanImage        *target,
                         const graphene_rect_t *rect,
                         const cairo_region_t  *clip)
{
  GdkSurface *surface = gsk_renderer_get_surface (self->renderer);

  self->target = g_object_ref (target);

  if (rect)
    {
      self->viewport = *rect;
      self->scale = 1.0;
    }
  else
    {
      self->scale = gdk_surface_get_scale (surface);
      self->viewport = GRAPHENE_RECT_INIT (0, 0,
                                           (int) ceil (gdk_surface_get_width (surface) * self->scale),
                                           (int) ceil (gdk_surface_get_height (surface) * self->scale));
    }
  if (clip)
    {
      cairo_rectangle_int_t extents;
      cairo_region_get_extents (clip, &extents);
      self->clip = cairo_region_create_rectangle (&(cairo_rectangle_int_t) {
                                                      extents.x, extents.y,
                                                      extents.width, extents.height
                                                  });
    }
  else
    {
      self->clip = cairo_region_create_rectangle (&(cairo_rectangle_int_t) {
                                                      0, 0,
                                                      gsk_vulkan_image_get_width (target),
                                                      gsk_vulkan_image_get_height (target)
                                                  });
    }
}

GskVulkanRender *
gsk_vulkan_render_new (GskRenderer      *renderer,
                       GdkVulkanContext *context)
{
  GskVulkanRender *self;
  VkDevice device;

  self = g_new0 (GskVulkanRender, 1);

  self->vulkan = context;
  self->renderer = renderer;
  gsk_descriptor_image_infos_init (&self->descriptor_images);
  gsk_descriptor_image_infos_init (&self->descriptor_samplers);
  gsk_descriptor_buffer_infos_init (&self->descriptor_buffers);

  device = gdk_vulkan_context_get_device (self->vulkan);

  self->command_pool = gsk_vulkan_command_pool_new (self->vulkan);
  GSK_VK_CHECK (vkCreateFence, device,
                               &(VkFenceCreateInfo) {
                                   .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                   .flags = VK_FENCE_CREATE_SIGNALED_BIT
                               },
                               NULL,
                               &self->fence);

  GSK_VK_CHECK (vkCreateDescriptorPool, device,
                                        &(VkDescriptorPoolCreateInfo) {
                                            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                            .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
                                            .maxSets = N_DESCRIPTOR_SETS,
                                            .poolSizeCount = N_DESCRIPTOR_SETS,
                                            .pPoolSizes = (VkDescriptorPoolSize[N_DESCRIPTOR_SETS]) {
                                                {
                                                    .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                                    .descriptorCount = DESCRIPTOR_POOL_MAXITEMS
                                                },
                                                {
                                                    .type = VK_DESCRIPTOR_TYPE_SAMPLER,
                                                    .descriptorCount = DESCRIPTOR_POOL_MAXITEMS
                                                },
                                                {
                                                    .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                    .descriptorCount = DESCRIPTOR_POOL_MAXITEMS
                                                }
                                            }
                                        },
                                        NULL,
                                        &self->descriptor_pool);

  GSK_VK_CHECK (vkCreateDescriptorSetLayout, device,
                                             &(VkDescriptorSetLayoutCreateInfo) {
                                                 .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                 .bindingCount = 1,
                                                 .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
                                                 .pBindings = (VkDescriptorSetLayoutBinding[3]) {
                                                     {
                                                         .binding = 0,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
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
                                             &self->descriptor_set_layouts[0]);

  GSK_VK_CHECK (vkCreateDescriptorSetLayout, device,
                                             &(VkDescriptorSetLayoutCreateInfo) {
                                                 .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                 .bindingCount = 1,
                                                 .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
                                                 .pBindings = (VkDescriptorSetLayoutBinding[1]) {
                                                     {
                                                         .binding = 0,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
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
                                             &self->descriptor_set_layouts[1]);

  GSK_VK_CHECK (vkCreateDescriptorSetLayout, device,
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
                                             &self->descriptor_set_layouts[2]);

  GSK_VK_CHECK (vkCreatePipelineLayout, device,
                                        &(VkPipelineLayoutCreateInfo) {
                                            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                            .setLayoutCount = G_N_ELEMENTS (self->descriptor_set_layouts),
                                            .pSetLayouts = self->descriptor_set_layouts,
                                            .pushConstantRangeCount = gsk_vulkan_push_constants_get_range_count (),
                                            .pPushConstantRanges = gsk_vulkan_push_constants_get_ranges ()
                                        },
                                        NULL,
                                        &self->pipeline_layout);

  GSK_VK_CHECK (vkCreateSampler, device,
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
                                 &self->samplers[GSK_VULKAN_SAMPLER_DEFAULT]);

  GSK_VK_CHECK (vkCreateSampler, device,
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
                                 &self->samplers[GSK_VULKAN_SAMPLER_REPEAT]);
  
  GSK_VK_CHECK (vkCreateSampler, device,
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
                                 &self->samplers[GSK_VULKAN_SAMPLER_NEAREST]);
  

  self->uploader = gsk_vulkan_uploader_new (self->vulkan, self->command_pool);

#ifdef G_ENABLE_DEBUG
  self->render_pass_counter = g_quark_from_static_string ("render-passes");
  self->gpu_time_timer = g_quark_from_static_string ("gpu-time");
#endif

  return self;
}

VkFence
gsk_vulkan_render_get_fence (GskVulkanRender *self)
{
  return self->fence;
}

void
gsk_vulkan_render_add_cleanup_image (GskVulkanRender *self,
                                     GskVulkanImage  *image)
{
  self->cleanup_images = g_slist_prepend (self->cleanup_images, image);
}

void
gsk_vulkan_render_add_render_pass (GskVulkanRender     *self,
                                   GskVulkanRenderPass *pass)
{
  self->render_passes = g_list_prepend (self->render_passes, pass);

#ifdef G_ENABLE_DEBUG
  gsk_profiler_counter_inc (gsk_renderer_get_profiler (self->renderer), self->render_pass_counter);
#endif
}

void
gsk_vulkan_render_add_node (GskVulkanRender *self,
                            GskRenderNode   *node)
{
  GskVulkanRenderPass *pass;
  graphene_vec2_t scale;

  graphene_vec2_init (&scale, self->scale, self->scale);

  pass = gsk_vulkan_render_pass_new (self->vulkan,
                                     self->target,
                                     &scale,
                                     &self->viewport,
                                     self->clip,
                                     VK_NULL_HANDLE);

  gsk_vulkan_render_add_render_pass (self, pass);

  gsk_vulkan_render_pass_add (pass, self, node);
}

void
gsk_vulkan_render_upload (GskVulkanRender *self)
{
  GList *l;

  /* gsk_vulkan_render_pass_upload may call gsk_vulkan_render_add_node_for_texture,
   * prepending new render passes to the list. Therefore, we walk the list from
   * the end.
   */
  for (l = g_list_last (self->render_passes); l; l = l->prev)
    {
      GskVulkanRenderPass *pass = l->data;
      gsk_vulkan_render_pass_upload (pass, self, self->uploader);
    }

  gsk_vulkan_uploader_upload (self->uploader);
}

GskVulkanPipeline *
gsk_vulkan_render_get_pipeline (GskVulkanRender       *self,
                                GskVulkanPipelineType  type,
                                VkRenderPass           render_pass)
{
  static const struct {
    const char *name;
    guint num_textures;
    GskVulkanPipeline * (* create_func) (GdkVulkanContext *context, VkPipelineLayout layout, const char *name, VkRenderPass render_pass);
  } pipeline_info[GSK_VULKAN_N_PIPELINES] = {
    { "texture",                    1, gsk_vulkan_texture_pipeline_new },
    { "texture-clip",               1, gsk_vulkan_texture_pipeline_new },
    { "texture-clip-rounded",       1, gsk_vulkan_texture_pipeline_new },
    { "color",                      0, gsk_vulkan_color_pipeline_new },
    { "color-clip",                 0, gsk_vulkan_color_pipeline_new },
    { "color-clip-rounded",         0, gsk_vulkan_color_pipeline_new },
    { "linear",                     0, gsk_vulkan_linear_gradient_pipeline_new },
    { "linear-clip",                0, gsk_vulkan_linear_gradient_pipeline_new },
    { "linear-clip-rounded",        0, gsk_vulkan_linear_gradient_pipeline_new },
    { "color-matrix",               1, gsk_vulkan_effect_pipeline_new },
    { "color-matrix-clip",          1, gsk_vulkan_effect_pipeline_new },
    { "color-matrix-clip-rounded",  1, gsk_vulkan_effect_pipeline_new },
    { "border",                     0, gsk_vulkan_border_pipeline_new },
    { "border-clip",                0, gsk_vulkan_border_pipeline_new },
    { "border-clip-rounded",        0, gsk_vulkan_border_pipeline_new },
    { "inset-shadow",               0, gsk_vulkan_box_shadow_pipeline_new },
    { "inset-shadow-clip",          0, gsk_vulkan_box_shadow_pipeline_new },
    { "inset-shadow-clip-rounded",  0, gsk_vulkan_box_shadow_pipeline_new },
    { "outset-shadow",              0, gsk_vulkan_box_shadow_pipeline_new },
    { "outset-shadow-clip",         0, gsk_vulkan_box_shadow_pipeline_new },
    { "outset-shadow-clip-rounded", 0, gsk_vulkan_box_shadow_pipeline_new },
    { "blur",                       1, gsk_vulkan_blur_pipeline_new },
    { "blur-clip",                  1, gsk_vulkan_blur_pipeline_new },
    { "blur-clip-rounded",          1, gsk_vulkan_blur_pipeline_new },
    { "mask",                       1, gsk_vulkan_text_pipeline_new },
    { "mask-clip",                  1, gsk_vulkan_text_pipeline_new },
    { "mask-clip-rounded",          1, gsk_vulkan_text_pipeline_new },
    { "texture",                    1, gsk_vulkan_color_text_pipeline_new },
    { "texture-clip",               1, gsk_vulkan_color_text_pipeline_new },
    { "texture-clip-rounded",       1, gsk_vulkan_color_text_pipeline_new },
    { "cross-fade",                 2, gsk_vulkan_cross_fade_pipeline_new },
    { "cross-fade-clip",            2, gsk_vulkan_cross_fade_pipeline_new },
    { "cross-fade-clip-rounded",    2, gsk_vulkan_cross_fade_pipeline_new },
    { "blend-mode",                 2, gsk_vulkan_blend_mode_pipeline_new },
    { "blend-mode-clip",            2, gsk_vulkan_blend_mode_pipeline_new },
    { "blend-mode-clip-rounded",    2, gsk_vulkan_blend_mode_pipeline_new },
  };

  g_return_val_if_fail (type < GSK_VULKAN_N_PIPELINES, NULL);

  if (self->pipelines[type] == NULL)
    self->pipelines[type] = pipeline_info[type].create_func (self->vulkan,
                                                             self->pipeline_layout,
                                                             pipeline_info[type].name,
                                                             render_pass);

  return self->pipelines[type];
}

void
gsk_vulkan_render_bind_descriptor_sets (GskVulkanRender *self,
                                        VkCommandBuffer  command_buffer)
{
  vkCmdBindDescriptorSets (command_buffer,
                           VK_PIPELINE_BIND_POINT_GRAPHICS,
                           self->pipeline_layout,
                           0,
                           3,
                           self->descriptor_sets,
                           0,
                           NULL);
}

gsize
gsk_vulkan_render_get_sampler_descriptor (GskVulkanRender        *self,
                                          GskVulkanRenderSampler  render_sampler)
{
  VkSampler sampler = self->samplers[render_sampler];
  gsize i;

  /* If this ever shows up in profiles, add a hash table */
  for (i = 0; i < gsk_descriptor_image_infos_get_size (&self->descriptor_samplers); i++)
    {
      if (gsk_descriptor_image_infos_get (&self->descriptor_samplers, i)->sampler == sampler)
        return i;
    }

  g_assert (i < DESCRIPTOR_POOL_MAXITEMS);

  gsk_descriptor_image_infos_append (&self->descriptor_samplers,
                                     &(VkDescriptorImageInfo) {
                                       .sampler = sampler,
                                     });

  return i;
}

gsize
gsk_vulkan_render_get_image_descriptor (GskVulkanRender *self,
                                        GskVulkanImage  *image)
{
  gsize result;

  result = gsk_descriptor_image_infos_get_size (&self->descriptor_images);
  gsk_descriptor_image_infos_append (&self->descriptor_images,
                                     &(VkDescriptorImageInfo) {
                                       .sampler = VK_NULL_HANDLE,
                                       .imageView = gsk_vulkan_image_get_image_view (image),
                                       .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                     });

  g_assert (result < DESCRIPTOR_POOL_MAXITEMS);

  return result;
}

static void
gsk_vulkan_render_ensure_storage_buffer (GskVulkanRender *self)
{
  if (self->storage_buffer_memory != NULL)
    return;

  if (self->storage_buffer == NULL)
    {
      self->storage_buffer = gsk_vulkan_buffer_new_storage (self->vulkan,
                                                            /* random */
                                                            sizeof (float) * 1024 * 1024);
    }

  self->storage_buffer_memory = gsk_vulkan_buffer_map (self->storage_buffer);

  if (gsk_vulkan_render_get_buffer_descriptor (self, self->storage_buffer) != 0)
    {
      g_assert_not_reached ();
    }
}

gsize
gsk_vulkan_render_get_buffer_descriptor (GskVulkanRender *self,
                                         GskVulkanBuffer *buffer)
{
  gsize result;

  gsk_vulkan_render_ensure_storage_buffer (self);

  result = gsk_descriptor_buffer_infos_get_size (&self->descriptor_buffers);
  gsk_descriptor_buffer_infos_append (&self->descriptor_buffers,
                                      &(VkDescriptorBufferInfo) {
                                        .buffer = gsk_vulkan_buffer_get_buffer (buffer),
                                        .offset = 0,
                                        .range = VK_WHOLE_SIZE
                                      });

  g_assert (result < DESCRIPTOR_POOL_MAXITEMS);

  return result;
}

static inline gsize
round_up (gsize number, gsize divisor)
{
  return (number + divisor - 1) / divisor * divisor;
}

guchar *
gsk_vulkan_render_get_buffer_memory (GskVulkanRender *self,
                                     gsize            size,
                                     gsize            alignment,
                                     gsize           *out_offset)
{
  guchar *result;

  g_assert (alignment >= sizeof (float));

  gsk_vulkan_render_ensure_storage_buffer (self);

  self->storage_buffer_used = round_up (self->storage_buffer_used, alignment);
  result = self->storage_buffer_memory + self->storage_buffer_used;
  *out_offset = self->storage_buffer_used / sizeof (float);

  self->storage_buffer_used += size;

  return result;
}

static void
gsk_vulkan_render_prepare_descriptor_sets (GskVulkanRender *self)
{
  VkDevice device;
  VkWriteDescriptorSet descriptor_sets[3];
  gsize n_descriptor_sets;
  GList *l;

  device = gdk_vulkan_context_get_device (self->vulkan);

  for (l = self->render_passes; l; l = l->next)
    {
      GskVulkanRenderPass *pass = l->data;
      gsk_vulkan_render_pass_reserve_descriptor_sets (pass, self);
    }
  
  if (self->storage_buffer_memory)
    {
      gsk_vulkan_buffer_unmap (self->storage_buffer);
      self->storage_buffer_memory = NULL;
      self->storage_buffer_used = 0;
    }

  GSK_VK_CHECK (vkAllocateDescriptorSets, device,
                                          &(VkDescriptorSetAllocateInfo) {
                                              .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                              .descriptorPool = self->descriptor_pool,
                                              .descriptorSetCount = N_DESCRIPTOR_SETS,
                                              .pSetLayouts = self->descriptor_set_layouts,
                                              .pNext = &(VkDescriptorSetVariableDescriptorCountAllocateInfo) {
                                                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
                                                .descriptorSetCount = N_DESCRIPTOR_SETS,
                                                .pDescriptorCounts = (uint32_t[N_DESCRIPTOR_SETS]) {
                                                  MAX (1, gsk_descriptor_image_infos_get_size (&self->descriptor_images)),
                                                  MAX (1, gsk_descriptor_image_infos_get_size (&self->descriptor_samplers)),
                                                  MAX (1, gsk_descriptor_buffer_infos_get_size (&self->descriptor_buffers))
                                                }
                                              }
                                          },
                                          self->descriptor_sets);

  n_descriptor_sets = 0;
  if (gsk_descriptor_image_infos_get_size (&self->descriptor_images) > 0)
    {
      descriptor_sets[n_descriptor_sets++] = (VkWriteDescriptorSet) {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = self->descriptor_sets[0],
          .dstBinding = 0,
          .dstArrayElement = 0,
          .descriptorCount = gsk_descriptor_image_infos_get_size (&self->descriptor_images),
          .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
          .pImageInfo = gsk_descriptor_image_infos_get_data (&self->descriptor_images)
      };
    }
  if (gsk_descriptor_image_infos_get_size (&self->descriptor_samplers) > 0)
    {
      descriptor_sets[n_descriptor_sets++] = (VkWriteDescriptorSet) {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = self->descriptor_sets[1],
          .dstBinding = 0,
          .dstArrayElement = 0,
          .descriptorCount = gsk_descriptor_image_infos_get_size (&self->descriptor_samplers),
          .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
          .pImageInfo = gsk_descriptor_image_infos_get_data (&self->descriptor_samplers)
      };
    }
  if (gsk_descriptor_buffer_infos_get_size (&self->descriptor_buffers) > 0)
    {
      descriptor_sets[n_descriptor_sets++] = (VkWriteDescriptorSet) {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = self->descriptor_sets[2],
          .dstBinding = 0,
          .dstArrayElement = 0,
          .descriptorCount = gsk_descriptor_buffer_infos_get_size (&self->descriptor_buffers),
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .pBufferInfo = gsk_descriptor_buffer_infos_get_data (&self->descriptor_buffers)
      };
    }

  vkUpdateDescriptorSets (device,
                          n_descriptor_sets,
                          descriptor_sets,
                          0, NULL);
}

void
gsk_vulkan_render_draw (GskVulkanRender *self)
{
  GList *l;

#ifdef G_ENABLE_DEBUG
  if (GSK_RENDERER_DEBUG_CHECK (self->renderer, SYNC))
    gsk_profiler_timer_begin (gsk_renderer_get_profiler (self->renderer), self->gpu_time_timer);
#endif

  gsk_vulkan_render_prepare_descriptor_sets (self);

  for (l = self->render_passes; l; l = l->next)
    {
      GskVulkanRenderPass *pass = l->data;
      VkCommandBuffer command_buffer;
      gsize wait_semaphore_count;
      gsize signal_semaphore_count;
      VkSemaphore *wait_semaphores;
      VkSemaphore *signal_semaphores;

      wait_semaphore_count = gsk_vulkan_render_pass_get_wait_semaphores (pass, &wait_semaphores);
      signal_semaphore_count = gsk_vulkan_render_pass_get_signal_semaphores (pass, &signal_semaphores);

      command_buffer = gsk_vulkan_command_pool_get_buffer (self->command_pool);

      gsk_vulkan_render_pass_draw (pass, self, self->pipeline_layout, command_buffer);

      gsk_vulkan_command_pool_submit_buffer (self->command_pool,
                                             command_buffer,
                                             wait_semaphore_count,
                                             wait_semaphores,
                                             signal_semaphore_count,
                                             signal_semaphores,
                                             l->next != NULL ? VK_NULL_HANDLE : self->fence);
    }

#ifdef G_ENABLE_DEBUG
  if (GSK_RENDERER_DEBUG_CHECK (self->renderer, SYNC))
    {
      GskProfiler *profiler;
      gint64 gpu_time;

      GSK_VK_CHECK (vkWaitForFences, gdk_vulkan_context_get_device (self->vulkan),
                                     1,
                                     &self->fence,
                                     VK_TRUE,
                                     INT64_MAX);

      profiler = gsk_renderer_get_profiler (self->renderer);
      gpu_time = gsk_profiler_timer_end (profiler, self->gpu_time_timer);
      gsk_profiler_timer_set (profiler, self->gpu_time_timer, gpu_time);
    }
#endif
}

GdkTexture *
gsk_vulkan_render_download_target (GskVulkanRender *self)
{
  GdkTexture *texture;

  texture = gsk_vulkan_image_download (self->target, self->uploader);
  gsk_vulkan_uploader_reset (self->uploader);

  return texture;
}

static void
gsk_vulkan_render_cleanup (GskVulkanRender *self)
{
  VkDevice device = gdk_vulkan_context_get_device (self->vulkan);

  /* XXX: Wait for fence here or just in reset()? */
  GSK_VK_CHECK (vkWaitForFences, device,
                                 1,
                                 &self->fence,
                                 VK_TRUE,
                                 INT64_MAX);

  GSK_VK_CHECK (vkResetFences, device,
                               1,
                               &self->fence);

  gsk_vulkan_uploader_reset (self->uploader);

  gsk_vulkan_command_pool_reset (self->command_pool);

  GSK_VK_CHECK (vkResetDescriptorPool, device,
                                       self->descriptor_pool,
                                       0);
  gsk_descriptor_image_infos_set_size (&self->descriptor_images, 0);
  gsk_descriptor_image_infos_set_size (&self->descriptor_samplers, 0);
  gsk_descriptor_buffer_infos_set_size (&self->descriptor_buffers, 0);

  g_list_free_full (self->render_passes, (GDestroyNotify) gsk_vulkan_render_pass_free);
  self->render_passes = NULL;
  g_slist_free_full (self->cleanup_images, g_object_unref);
  self->cleanup_images = NULL;

  g_clear_pointer (&self->clip, cairo_region_destroy);
  g_clear_object (&self->target);
}

void
gsk_vulkan_render_free (GskVulkanRender *self)
{
  VkDevice device;
  guint i;
  
  gsk_vulkan_render_cleanup (self);

  device = gdk_vulkan_context_get_device (self->vulkan);

  for (i = 0; i < GSK_VULKAN_N_PIPELINES; i++)
    g_clear_object (&self->pipelines[i]);

  g_clear_pointer (&self->uploader, gsk_vulkan_uploader_free);


  vkDestroyPipelineLayout (device,
                           self->pipeline_layout,
                           NULL);

  vkDestroyDescriptorPool (device,
                           self->descriptor_pool,
                           NULL);
  gsk_descriptor_image_infos_clear (&self->descriptor_images);
  gsk_descriptor_image_infos_clear (&self->descriptor_samplers);
  gsk_descriptor_buffer_infos_clear (&self->descriptor_buffers);

  for (i = 0; i < N_DESCRIPTOR_SETS; i++)
    vkDestroyDescriptorSetLayout (device,
                                  self->descriptor_set_layouts[i],
                                  NULL);

  vkDestroyFence (device,
                  self->fence,
                  NULL);

  for (i = 0; i < G_N_ELEMENTS (self->samplers); i++)
    {
      vkDestroySampler (device,
                        self->samplers[i],
                        NULL);
    }

  gsk_vulkan_command_pool_free (self->command_pool);

  g_free (self);
}

gboolean
gsk_vulkan_render_is_busy (GskVulkanRender *self)
{
  return vkGetFenceStatus (gdk_vulkan_context_get_device (self->vulkan), self->fence) != VK_SUCCESS;
}

void
gsk_vulkan_render_reset (GskVulkanRender       *self,
                         GskVulkanImage        *target,
                         const graphene_rect_t *rect,
                         const cairo_region_t  *clip)
{
  gsk_vulkan_render_cleanup (self);

  gsk_vulkan_render_setup (self, target, rect, clip);
}

GskRenderer *
gsk_vulkan_render_get_renderer (GskVulkanRender *self)
{
  return self->renderer;
}
