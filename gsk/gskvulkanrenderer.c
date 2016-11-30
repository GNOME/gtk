#include "config.h"

#include "gskvulkanrendererprivate.h"

#include "gskdebugprivate.h"
#include "gskrendererprivate.h"
#include "gskrendernodeiter.h"
#include "gskrendernodeprivate.h"
#include "gsktextureprivate.h"

typedef struct _GskVulkanTarget GskVulkanTarget;

#ifdef G_ENABLE_DEBUG
typedef struct {
  GQuark cpu_time;
  GQuark gpu_time;
} ProfileTimers;
#endif

struct _GskVulkanRenderer
{
  GskRenderer parent_instance;

  GdkVulkanContext *vulkan;

  guint n_targets;
  GskVulkanTarget **targets;

  VkRenderPass render_pass;
  VkCommandPool command_pool;

#ifdef G_ENABLE_DEBUG
  ProfileTimers profile_timers;
#endif
};

struct _GskVulkanRendererClass
{
  GskRendererClass parent_class;
};

G_DEFINE_TYPE (GskVulkanRenderer, gsk_vulkan_renderer, GSK_TYPE_RENDERER)

struct _GskVulkanTarget {
  VkImage image;
  VkImageView image_view;
  VkFramebuffer framebuffer;
}; 

static inline VkResult
gsk_vulkan_handle_result (VkResult    res,
                          const char *called_function)
{
  if (res != VK_SUCCESS)
    {
      GSK_NOTE (VULKAN,g_printerr ("%s(): %s (%d)\n", called_function, gdk_vulkan_strerror (res), res));
    }
  return res;
}

#define GSK_VK_CHECK(func, ...) gsk_vulkan_handle_result (func (__VA_ARGS__), G_STRINGIFY (func))

static GskVulkanTarget *
gsk_vulkan_target_new_for_image (GskVulkanRenderer *self,
                                 VkImage            image)
{
  GskVulkanTarget *target;
  GdkWindow *window;
  VkDevice device;

  device = gdk_vulkan_context_get_device (self->vulkan);
  window = gdk_draw_context_get_window (GDK_DRAW_CONTEXT (self->vulkan));

  target = g_slice_new0 (GskVulkanTarget);

  target->image = image;

  GSK_VK_CHECK (vkCreateImageView, device,
                                   &(VkImageViewCreateInfo) {
                                       .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                       .image = target->image,
                                       .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                       .format = gdk_vulkan_context_get_image_format (self->vulkan),
                                       .components = {
                                           .r = VK_COMPONENT_SWIZZLE_R,
                                           .g = VK_COMPONENT_SWIZZLE_G,
                                           .b = VK_COMPONENT_SWIZZLE_B,
                                           .a = VK_COMPONENT_SWIZZLE_A,
                                       },
                                       .subresourceRange = {
                                           .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                           .baseMipLevel = 0,
                                           .levelCount = 1,
                                           .baseArrayLayer = 0,
                                           .layerCount = 1,
                                       },
                                   },
                                   NULL,
                                   &target->image_view);
  GSK_VK_CHECK (vkCreateFramebuffer, device,
                                     &(VkFramebufferCreateInfo) {
                                         .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                         .renderPass = self->render_pass,
                                         .attachmentCount = 1,
                                         .pAttachments = &target->image_view,
                                         .width = gdk_window_get_width (window),
                                         .height = gdk_window_get_height (window),
                                         .layers = 1
                                     },
                                     NULL,
                                     &target->framebuffer);

  return target;
}

static void
gsk_vulkan_target_free (GskVulkanRenderer *self,
                        GskVulkanTarget   *target)
{
  VkDevice device;

  device = gdk_vulkan_context_get_device (self->vulkan);

  vkDestroyFramebuffer (device,
                        target->framebuffer,
                        NULL);
  vkDestroyImageView (device,
                      target->image_view,
                      NULL);

  g_slice_free (GskVulkanTarget, target);
}

static void
gsk_vulkan_renderer_free_targets (GskVulkanRenderer *self)
{
  guint i;

  for (i = 0; i < self->n_targets; i++)
    {
      gsk_vulkan_target_free (self, self->targets[i]);
    }

  g_clear_pointer (&self->targets, g_free);
  self->n_targets = 0;
}

static void
gsk_vulkan_renderer_update_images_cb (GdkVulkanContext  *context,
                                      GskVulkanRenderer *self)
{
  guint i;

  gsk_vulkan_renderer_free_targets (self);

  self->n_targets = gdk_vulkan_context_get_n_images (context);
  self->targets = g_new (GskVulkanTarget *, self->n_targets);

  for (i = 0; i < self->n_targets; i++)
    {
      self->targets[i] = gsk_vulkan_target_new_for_image (self,
                                                          gdk_vulkan_context_get_image (context, i));
    }
}

static gboolean
gsk_vulkan_renderer_realize (GskRenderer  *renderer,
                             GdkWindow    *window,
                             GError      **error)
{
  GskVulkanRenderer *self = GSK_VULKAN_RENDERER (renderer);
  VkDevice device;

  self->vulkan = gdk_window_create_vulkan_context (window, error);
  if (self->vulkan == NULL)
    return FALSE;

  device = gdk_vulkan_context_get_device (self->vulkan);

  GSK_VK_CHECK (vkCreateRenderPass, gdk_vulkan_context_get_device (self->vulkan),
                                    &(VkRenderPassCreateInfo) {
                                        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                        .attachmentCount = 1,
                                        .pAttachments = (VkAttachmentDescription[]) {
                                           {
                                              .format = gdk_vulkan_context_get_image_format (self->vulkan),
                                              .samples = VK_SAMPLE_COUNT_1_BIT,
                                              .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
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
                                               .preserveAttachmentCount = 1,
                                               .pPreserveAttachments = (uint32_t []) { 0 },
                                            }
                                         },
                                         .dependencyCount = 0
                                      },
                                      NULL,
                                      &self->render_pass);
   GSK_VK_CHECK (vkCreateCommandPool, device,
                                      &(const VkCommandPoolCreateInfo) {
                                          .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                          .queueFamilyIndex = gdk_vulkan_context_get_queue_family_index (self->vulkan),
                                          .flags = 0
                                      },
                                      NULL,
                                      &self->command_pool);

  g_signal_connect (self->vulkan,
                    "images-updated",
                    G_CALLBACK (gsk_vulkan_renderer_update_images_cb),
                    self);
  gsk_vulkan_renderer_update_images_cb (self->vulkan, self);

  return TRUE;
}

static void
gsk_vulkan_renderer_unrealize (GskRenderer *renderer)
{
  GskVulkanRenderer *self = GSK_VULKAN_RENDERER (renderer);
  VkDevice device;

  device = gdk_vulkan_context_get_device (self->vulkan);

  gsk_vulkan_renderer_free_targets (self);
  g_signal_handlers_disconnect_by_func(self->vulkan,
                                       gsk_vulkan_renderer_update_images_cb,
                                       self);

  vkDestroyCommandPool (device,
                        self->command_pool,
                        NULL);
  self->command_pool = VK_NULL_HANDLE;

  vkDestroyRenderPass (device,
                       self->render_pass,
                       NULL);
  self->render_pass = VK_NULL_HANDLE;

  g_clear_object (&self->vulkan);
}

static void
gsk_vulkan_renderer_render (GskRenderer   *renderer,
                            GskRenderNode *root)
{
  GskVulkanRenderer *self = GSK_VULKAN_RENDERER (renderer);
#ifdef G_ENABLE_DEBUG
  GskProfiler *profiler;
  gint64 cpu_time;
#endif

#ifdef G_ENABLE_DEBUG
  profiler = gsk_renderer_get_profiler (renderer);
  gsk_profiler_timer_begin (profiler, self->profile_timers.cpu_time);
#endif

#ifdef G_ENABLE_DEBUG
  cpu_time = gsk_profiler_timer_end (profiler, self->profile_timers.cpu_time);
  gsk_profiler_timer_set (profiler, self->profile_timers.cpu_time, cpu_time);

  gsk_profiler_push_samples (profiler);
#endif
}

static void
gsk_vulkan_renderer_class_init (GskVulkanRendererClass *klass)
{
  GskRendererClass *renderer_class = GSK_RENDERER_CLASS (klass);

  renderer_class->realize = gsk_vulkan_renderer_realize;
  renderer_class->unrealize = gsk_vulkan_renderer_unrealize;
  renderer_class->render = gsk_vulkan_renderer_render;
}

static void
gsk_vulkan_renderer_init (GskVulkanRenderer *self)
{
#ifdef G_ENABLE_DEBUG
  GskProfiler *profiler = gsk_renderer_get_profiler (GSK_RENDERER (self));

  self->profile_timers.cpu_time = gsk_profiler_add_timer (profiler, "cpu-time", "CPU time", FALSE, TRUE);
#endif
}
