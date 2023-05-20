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

struct _GskVulkanRender
{
  GskRenderer *renderer;
  GdkVulkanContext *vulkan;

  double scale;
  graphene_rect_t viewport;
  cairo_region_t *clip;

  GHashTable *framebuffers;
  GskVulkanCommandPool *command_pool;
  VkFence fence;
  VkRenderPass render_pass;
  VkDescriptorSetLayout descriptor_set_layout;
  VkPipelineLayout pipeline_layout;
  GskVulkanUploader *uploader;

  GskDescriptorImageInfos descriptor_image_infos;
  VkDescriptorPool descriptor_pool;
  VkDescriptorSet descriptor_set;
  GskVulkanPipeline *pipelines[GSK_VULKAN_N_PIPELINES];

  GskVulkanImage *target;

  VkSampler sampler;
  VkSampler repeating_sampler;

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
  self->framebuffers = g_hash_table_new (g_direct_hash, g_direct_equal);
  gsk_descriptor_image_infos_init (&self->descriptor_image_infos);

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
                                            .maxSets = 1,
                                            .poolSizeCount = 1,
                                            .pPoolSizes = (VkDescriptorPoolSize[1]) {
                                                {
                                                    .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                    .descriptorCount = DESCRIPTOR_POOL_MAXITEMS
                                                }
                                            }
                                        },
                                        NULL,
                                        &self->descriptor_pool);

  GSK_VK_CHECK (vkCreateRenderPass, gdk_vulkan_context_get_device (self->vulkan),
                                    &(VkRenderPassCreateInfo) {
                                        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                        .attachmentCount = 1,
                                        .pAttachments = (VkAttachmentDescription[]) {
                                           {
                                              .format = gdk_vulkan_context_get_image_format (self->vulkan),
                                              .samples = VK_SAMPLE_COUNT_1_BIT,
                                              .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                              .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                              .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                              .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
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
                                         .dependencyCount = 0
                                      },
                                      NULL,
                                      &self->render_pass);

  GSK_VK_CHECK (vkCreateDescriptorSetLayout, device,
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
                                                   .pBindingFlags = &(VkDescriptorBindingFlags) {
                                                     VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
                                                     | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
                                                     | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
                                                   },
                                                 }
                                             },
                                             NULL,
                                             &self->descriptor_set_layout);

  GSK_VK_CHECK (vkCreatePipelineLayout, device,
                                        &(VkPipelineLayoutCreateInfo) {
                                            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                            .setLayoutCount = 1,
                                            .pSetLayouts = (VkDescriptorSetLayout[1]) {
                                              self->descriptor_set_layout
                                            },
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
                                 &self->sampler);

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
                                 &self->repeating_sampler);

  self->uploader = gsk_vulkan_uploader_new (self->vulkan, self->command_pool);

#ifdef G_ENABLE_DEBUG
  self->render_pass_counter = g_quark_from_static_string ("render-passes");
  self->gpu_time_timer = g_quark_from_static_string ("gpu-time");
#endif

  return self;
}

typedef struct {
  VkFramebuffer framebuffer;
} HashFramebufferEntry;

static void
gsk_vulkan_render_remove_framebuffer_from_image (gpointer  data,
                                                 GObject  *image)
{
  GskVulkanRender *self = data;
  HashFramebufferEntry *fb;

  fb = g_hash_table_lookup (self->framebuffers, image);
  g_hash_table_remove (self->framebuffers, image);

  vkDestroyFramebuffer (gdk_vulkan_context_get_device (self->vulkan),
                        fb->framebuffer,
                        NULL);

  g_free (fb);
}

VkFramebuffer
gsk_vulkan_render_get_framebuffer (GskVulkanRender *self,
                                   GskVulkanImage  *image)
{
  HashFramebufferEntry *fb;

  fb = g_hash_table_lookup (self->framebuffers, image);
  if (fb)
    return fb->framebuffer;

  fb = g_new0 (HashFramebufferEntry, 1);
  GSK_VK_CHECK (vkCreateFramebuffer, gdk_vulkan_context_get_device (self->vulkan),
                                     &(VkFramebufferCreateInfo) {
                                         .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                         .renderPass = self->render_pass,
                                         .attachmentCount = 1,
                                         .pAttachments = (VkImageView[1]) {
                                             gsk_vulkan_image_get_image_view (image)
                                         },
                                         .width = gsk_vulkan_image_get_width (image),
                                         .height = gsk_vulkan_image_get_height (image),
                                         .layers = 1
                                     },
                                     NULL,
                                     &fb->framebuffer);
  g_hash_table_insert (self->framebuffers, image, fb);
  g_object_weak_ref (G_OBJECT (image), gsk_vulkan_render_remove_framebuffer_from_image, self);

  return fb->framebuffer;
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
                                GskVulkanPipelineType  type)
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
    { "crossfade",                  2, gsk_vulkan_cross_fade_pipeline_new },
    { "crossfade-clip",             2, gsk_vulkan_cross_fade_pipeline_new },
    { "crossfade-clip-rounded",     2, gsk_vulkan_cross_fade_pipeline_new },
    { "blendmode",                  2, gsk_vulkan_blend_mode_pipeline_new },
    { "blendmode-clip",             2, gsk_vulkan_blend_mode_pipeline_new },
    { "blendmode-clip-rounded",     2, gsk_vulkan_blend_mode_pipeline_new },
  };

  g_return_val_if_fail (type < GSK_VULKAN_N_PIPELINES, NULL);

  if (self->pipelines[type] == NULL)
    self->pipelines[type] = pipeline_info[type].create_func (self->vulkan,
                                                             self->pipeline_layout,
                                                             pipeline_info[type].name,
                                                             self->render_pass);

  return self->pipelines[type];
}

VkDescriptorSet
gsk_vulkan_render_get_descriptor_set (GskVulkanRender *self)
{
  return self->descriptor_set;
}

gsize
gsk_vulkan_render_reserve_descriptor_set (GskVulkanRender *self,
                                          GskVulkanImage  *source,
                                          gboolean         repeat)
{
  VkDescriptorImageInfo info = {
    .sampler = repeat ? self->repeating_sampler : self->sampler,
    .imageView = gsk_vulkan_image_get_image_view (source),
    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
  };
  gsize result;

  result = gsk_descriptor_image_infos_get_size (&self->descriptor_image_infos);
  gsk_descriptor_image_infos_append (&self->descriptor_image_infos, &info);

  g_assert (result < DESCRIPTOR_POOL_MAXITEMS);

  return result;
}

static void
gsk_vulkan_render_prepare_descriptor_sets (GskVulkanRender *self)
{
  VkDevice device;
  GList *l;

  device = gdk_vulkan_context_get_device (self->vulkan);

  for (l = self->render_passes; l; l = l->next)
    {
      GskVulkanRenderPass *pass = l->data;
      gsk_vulkan_render_pass_reserve_descriptor_sets (pass, self);
    }
  
  if (gsk_descriptor_image_infos_get_size (&self->descriptor_image_infos) == 0)
    return;

  GSK_VK_CHECK (vkAllocateDescriptorSets, device,
                                          &(VkDescriptorSetAllocateInfo) {
                                              .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                              .descriptorPool = self->descriptor_pool,
                                              .descriptorSetCount = 1,
                                              .pSetLayouts = &self->descriptor_set_layout,
                                              .pNext = &(VkDescriptorSetVariableDescriptorCountAllocateInfo) {
                                                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
                                                .descriptorSetCount = 1,
                                                .pDescriptorCounts = (uint32_t[1]) {
                                                  gsk_descriptor_image_infos_get_size (&self->descriptor_image_infos)
                                                }
                                              }
                                          },
                                          &self->descriptor_set);

  vkUpdateDescriptorSets (device,
                          1,
                          (VkWriteDescriptorSet[1]) {
                              {
                                  .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                  .dstSet = self->descriptor_set,
                                  .dstBinding = 0,
                                  .dstArrayElement = 0,
                                  .descriptorCount = gsk_descriptor_image_infos_get_size (&self->descriptor_image_infos),
                                  .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                  .pImageInfo = gsk_descriptor_image_infos_get_data (&self->descriptor_image_infos)
                              }
                          },
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
  gsk_descriptor_image_infos_set_size (&self->descriptor_image_infos, 0);

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
  GHashTableIter iter;
  gpointer key, value;
  VkDevice device;
  guint i;
  
  gsk_vulkan_render_cleanup (self);

  device = gdk_vulkan_context_get_device (self->vulkan);

  g_hash_table_iter_init (&iter, self->framebuffers);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      HashFramebufferEntry *fb = value;

      vkDestroyFramebuffer (gdk_vulkan_context_get_device (self->vulkan),
                            fb->framebuffer,
                            NULL);
      g_free (fb);
      g_object_weak_unref (G_OBJECT (key), gsk_vulkan_render_remove_framebuffer_from_image, self);
      g_hash_table_iter_remove (&iter);
    }
  g_hash_table_unref (self->framebuffers);

  for (i = 0; i < GSK_VULKAN_N_PIPELINES; i++)
    g_clear_object (&self->pipelines[i]);

  g_clear_pointer (&self->uploader, gsk_vulkan_uploader_free);


  vkDestroyPipelineLayout (device,
                           self->pipeline_layout,
                           NULL);

  vkDestroyRenderPass (device,
                       self->render_pass,
                       NULL);

  vkDestroyDescriptorPool (device,
                           self->descriptor_pool,
                           NULL);
  gsk_descriptor_image_infos_clear (&self->descriptor_image_infos);

  vkDestroyDescriptorSetLayout (device,
                                self->descriptor_set_layout,
                                NULL);

  vkDestroyFence (device,
                  self->fence,
                  NULL);

  vkDestroySampler (device,
                    self->sampler,
                    NULL);

  vkDestroySampler (device,
                    self->repeating_sampler,
                    NULL);

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
