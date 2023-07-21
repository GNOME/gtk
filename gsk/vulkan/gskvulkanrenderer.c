#include "config.h"

#include "gskvulkanrendererprivate.h"

#include "gskdebugprivate.h"
#include "gskprivate.h"
#include "gskrendererprivate.h"
#include "gskrendernodeprivate.h"
#include "gskvulkanbufferprivate.h"
#include "gskvulkanimageprivate.h"
#include "gskvulkanprivate.h"
#include "gskvulkanrenderprivate.h"
#include "gskvulkanglyphcacheprivate.h"

#include "gdk/gdkdisplayprivate.h"
#include "gdk/gdkdrawcontextprivate.h"
#include "gdk/gdkprofilerprivate.h"
#include "gdk/gdktextureprivate.h"
#include "gdk/gdkvulkancontextprivate.h"

#include <graphene.h>

#define GSK_VULKAN_MAX_RENDERS 4

typedef struct _GskVulkanTextureData GskVulkanTextureData;

struct _GskVulkanTextureData {
  GdkTexture *texture;
  GskVulkanImage *image;
  GskVulkanRenderer *renderer;
};

#ifdef G_ENABLE_DEBUG
typedef struct {
  GQuark frames;
  GQuark render_passes;
  GQuark fallback_pixels;
  GQuark texture_pixels;
} ProfileCounters;

typedef struct {
  GQuark cpu_time;
  GQuark gpu_time;
} ProfileTimers;

static guint texture_pixels_counter;
static guint fallback_pixels_counter;
#endif

struct _GskVulkanRenderer
{
  GskRenderer parent_instance;

  GdkVulkanContext *vulkan;

  guint n_targets;
  GskVulkanImage **targets;

  GskVulkanRender *renders[GSK_VULKAN_MAX_RENDERS];

  GSList *textures;

  GskVulkanGlyphCache *glyph_cache;

#ifdef G_ENABLE_DEBUG
  ProfileCounters profile_counters;
  ProfileTimers profile_timers;
#endif
};

struct _GskVulkanRendererClass
{
  GskRendererClass parent_class;
};

G_DEFINE_TYPE (GskVulkanRenderer, gsk_vulkan_renderer, GSK_TYPE_RENDERER)

static cairo_region_t *
get_render_region (GskVulkanRenderer *self)
{
  const cairo_region_t *damage;
  cairo_region_t *render_region;
  cairo_region_t *scaled_damage;
  GdkRectangle whole_surface;
  GdkRectangle extents;
  GdkSurface *surface;
  double scale;

  render_region = NULL;
  surface = gdk_draw_context_get_surface (GDK_DRAW_CONTEXT (self->vulkan));
  scale = gdk_surface_get_scale (surface);

  whole_surface.x = 0;
  whole_surface.y = 0;
  whole_surface.width = gdk_surface_get_width (surface);
  whole_surface.height = gdk_surface_get_height (surface);

  damage = gdk_draw_context_get_frame_region (GDK_DRAW_CONTEXT (self->vulkan));
  scaled_damage = cairo_region_create ();
  for (int i = 0; i < cairo_region_num_rectangles (damage); i++)
    {
      cairo_rectangle_int_t rect;
      cairo_region_get_rectangle (damage, i, &rect);
      cairo_region_union_rectangle (scaled_damage, &(cairo_rectangle_int_t) {
                                      .x = (int) floor (rect.x * scale),
                                      .y = (int) floor (rect.y * scale),
                                      .width = (int) ceil ((rect.x + rect.width) * scale) - floor (rect.x * scale),
                                      .height = (int) ceil ((rect.y + rect.height) * scale) - floor (rect.y * scale),
                                    });
    }

  if (cairo_region_contains_rectangle (scaled_damage, &whole_surface) == CAIRO_REGION_OVERLAP_IN)
    goto out;

  cairo_region_get_extents (scaled_damage, &extents);
  if (gdk_rectangle_equal (&extents, &whole_surface))
    goto out;

  render_region = cairo_region_create_rectangle (&extents);

out:
  g_clear_pointer (&scaled_damage, cairo_region_destroy);

  return g_steal_pointer (&render_region);
}

static void
gsk_vulkan_renderer_free_targets (GskVulkanRenderer *self)
{
  guint i;

  for (i = 0; i < self->n_targets; i++)
    {
      g_object_unref (self->targets[i]);
    }

  g_clear_pointer (&self->targets, g_free);
  self->n_targets = 0;
}

static void
gsk_vulkan_renderer_update_images_cb (GdkVulkanContext  *context,
                                      GskVulkanRenderer *self)
{
  GdkSurface *surface;
  double scale;
  gsize width, height;
  guint i;

  surface = gsk_renderer_get_surface (GSK_RENDERER (self));
  if (surface == NULL)
    return;

  gsk_vulkan_renderer_free_targets (self);

  self->n_targets = gdk_vulkan_context_get_n_images (context);
  self->targets = g_new (GskVulkanImage *, self->n_targets);

  scale = gdk_surface_get_scale (surface);
  width = (int) ceil (gdk_surface_get_width (surface) * scale);
  height = (int) ceil (gdk_surface_get_height (surface) * scale);

  for (i = 0; i < self->n_targets; i++)
    {
      self->targets[i] = gsk_vulkan_image_new_for_swapchain (self->vulkan,
                                                             gdk_vulkan_context_get_image (context, i),
                                                             gdk_vulkan_context_get_image_format (self->vulkan),
                                                             width, height);
    }
}

static GskVulkanRender *
gsk_vulkan_renderer_get_render (GskVulkanRenderer *self)
{
  VkFence fences[G_N_ELEMENTS (self->renders)];
  VkDevice device;
  guint i;

  device = gdk_vulkan_context_get_device (self->vulkan);

  while (TRUE)
    {
      for (i = 0; i < G_N_ELEMENTS (self->renders); i++)
        {
          if (self->renders[i] == NULL)
            {
              self->renders[i] = gsk_vulkan_render_new (GSK_RENDERER (self), self->vulkan);
              return self->renders[i];
            }

          fences[i] = gsk_vulkan_render_get_fence (self->renders[i]);
          if (vkGetFenceStatus (device, fences[i]) == VK_SUCCESS)
            return self->renders[i];
        }

      GSK_VK_CHECK (vkWaitForFences, device,
                                     G_N_ELEMENTS (fences),
                                     fences,
                                     VK_FALSE,
                                     INT64_MAX);
    }
}

static gboolean
gsk_vulkan_renderer_realize (GskRenderer  *renderer,
                             GdkSurface   *surface,
                             GError      **error)
{
  GskVulkanRenderer *self = GSK_VULKAN_RENDERER (renderer);

  if (surface == NULL)
    self->vulkan = gdk_display_create_vulkan_context (gdk_display_get_default (), error);
  else
    self->vulkan = gdk_surface_create_vulkan_context (surface, error);

  if (self->vulkan == NULL)
    return FALSE;

  g_signal_connect (self->vulkan,
                    "images-updated",
                    G_CALLBACK (gsk_vulkan_renderer_update_images_cb),
                    self);
  gsk_vulkan_renderer_update_images_cb (self->vulkan, self);

  self->glyph_cache = gsk_vulkan_glyph_cache_new (self->vulkan);

  return TRUE;
}

static void
gsk_vulkan_renderer_unrealize (GskRenderer *renderer)
{
  GskVulkanRenderer *self = GSK_VULKAN_RENDERER (renderer);
  GSList *l;
  guint i;

  g_clear_object (&self->glyph_cache);

  for (l = self->textures; l; l = l->next)
    {
      GskVulkanTextureData *data = l->data;

      data->renderer = NULL;
      gdk_texture_clear_render_data (data->texture);
    }
  g_clear_pointer (&self->textures, g_slist_free);

  for (i = 0; i < G_N_ELEMENTS (self->renders); i++)
    g_clear_pointer (&self->renders[i], gsk_vulkan_render_free);

  gsk_vulkan_renderer_free_targets (self);
  g_signal_handlers_disconnect_by_func(self->vulkan,
                                       gsk_vulkan_renderer_update_images_cb,
                                       self);

  g_clear_object (&self->vulkan);
}

static void
gsk_vulkan_renderer_download_texture_cb (gpointer         user_data,
                                         GdkMemoryFormat  format,
                                         const guchar    *data,
                                         int              width,
                                         int              height,
                                         gsize            stride)
{
  GdkTexture **texture = (GdkTexture **) user_data;
  GBytes *bytes;

  bytes = g_bytes_new (data, stride * height);
  *texture = gdk_memory_texture_new (width, height,
                                     format,
                                     bytes,
                                     stride);
  g_bytes_unref (bytes);
}

static GdkTexture *
gsk_vulkan_renderer_render_texture (GskRenderer           *renderer,
                                    GskRenderNode         *root,
                                    const graphene_rect_t *viewport)
{
  GskVulkanRenderer *self = GSK_VULKAN_RENDERER (renderer);
  GskVulkanRender *render;
  GskVulkanImage *image;
  GdkTexture *texture;
  graphene_rect_t rounded_viewport;
#ifdef G_ENABLE_DEBUG
  GskProfiler *profiler;
  gint64 cpu_time;
  gint64 start_time G_GNUC_UNUSED;
#endif

#ifdef G_ENABLE_DEBUG
  profiler = gsk_renderer_get_profiler (renderer);
  gsk_profiler_counter_set (profiler, self->profile_counters.fallback_pixels, 0);
  gsk_profiler_counter_set (profiler, self->profile_counters.texture_pixels, 0);
  gsk_profiler_counter_set (profiler, self->profile_counters.render_passes, 0);
  gsk_profiler_timer_begin (profiler, self->profile_timers.cpu_time);
#endif

  render = gsk_vulkan_render_new (renderer, self->vulkan);

  rounded_viewport = GRAPHENE_RECT_INIT (viewport->origin.x,
                                         viewport->origin.y,
                                         ceil (viewport->size.width),
                                         ceil (viewport->size.height));
  image = gsk_vulkan_image_new_for_offscreen (self->vulkan,
                                              gdk_vulkan_context_get_offscreen_format (self->vulkan,
                                                  gsk_render_node_get_preferred_depth (root)),
                                              rounded_viewport.size.width,
                                              rounded_viewport.size.height);

  texture = NULL;
  gsk_vulkan_render_render (render,
                            image,
                            &rounded_viewport,
                            NULL,
                            root,
                            gsk_vulkan_renderer_download_texture_cb,
                            &texture);

  gsk_vulkan_render_free (render);
  g_object_unref (image);

  /* check that callback setting texture was actually called, as its technically async */
  g_assert (texture);

#ifdef G_ENABLE_DEBUG
  start_time = gsk_profiler_timer_get_start (profiler, self->profile_timers.cpu_time);
  cpu_time = gsk_profiler_timer_end (profiler, self->profile_timers.cpu_time);
  gsk_profiler_timer_set (profiler, self->profile_timers.cpu_time, cpu_time);

  gsk_profiler_push_samples (profiler);

  if (GDK_PROFILER_IS_RUNNING)
    {
      gdk_profiler_add_mark (start_time * 1000, cpu_time * 1000, "render", "");
      gdk_profiler_set_int_counter (texture_pixels_counter,
                                    gsk_profiler_counter_get (profiler, self->profile_counters.texture_pixels));
      gdk_profiler_set_int_counter (fallback_pixels_counter,
                                    gsk_profiler_counter_get (profiler, self->profile_counters.fallback_pixels));
    }
#endif

  return texture;
}

static void
gsk_vulkan_renderer_render (GskRenderer          *renderer,
                            GskRenderNode        *root,
                            const cairo_region_t *region)
{
  GskVulkanRenderer *self = GSK_VULKAN_RENDERER (renderer);
  GskVulkanRender *render;
  GdkSurface *surface;
  cairo_region_t *render_region;
#ifdef G_ENABLE_DEBUG
  GskProfiler *profiler;
  gint64 cpu_time;
#endif
  uint32_t draw_index;

#ifdef G_ENABLE_DEBUG
  profiler = gsk_renderer_get_profiler (renderer);
  gsk_profiler_counter_set (profiler, self->profile_counters.fallback_pixels, 0);
  gsk_profiler_counter_set (profiler, self->profile_counters.texture_pixels, 0);
  gsk_profiler_counter_set (profiler, self->profile_counters.render_passes, 0);
  gsk_profiler_timer_begin (profiler, self->profile_timers.cpu_time);
#endif

  gdk_draw_context_begin_frame_full (GDK_DRAW_CONTEXT (self->vulkan),
                                     gsk_render_node_get_preferred_depth (root),
                                     region);
  render = gsk_vulkan_renderer_get_render (self);
  surface = gdk_draw_context_get_surface (GDK_DRAW_CONTEXT (self->vulkan));

  render_region = get_render_region (self);
  draw_index = gdk_vulkan_context_get_draw_index (self->vulkan);

  gsk_vulkan_render_render (render,
                            self->targets[draw_index],
                            &GRAPHENE_RECT_INIT(
                              0, 0,
                              gdk_surface_get_width (surface),
                              gdk_surface_get_height (surface)
                            ),
                            render_region,
                            root,
                            NULL, NULL);

#ifdef G_ENABLE_DEBUG
  gsk_profiler_counter_inc (profiler, self->profile_counters.frames);

  cpu_time = gsk_profiler_timer_end (profiler, self->profile_timers.cpu_time);
  gsk_profiler_timer_set (profiler, self->profile_timers.cpu_time, cpu_time);

  gsk_profiler_push_samples (profiler);
#endif

  gdk_draw_context_end_frame (GDK_DRAW_CONTEXT (self->vulkan));

  g_clear_pointer (&render_region, cairo_region_destroy);
}

static void
gsk_vulkan_renderer_class_init (GskVulkanRendererClass *klass)
{
  GskRendererClass *renderer_class = GSK_RENDERER_CLASS (klass);

  renderer_class->realize = gsk_vulkan_renderer_realize;
  renderer_class->unrealize = gsk_vulkan_renderer_unrealize;
  renderer_class->render = gsk_vulkan_renderer_render;
  renderer_class->render_texture = gsk_vulkan_renderer_render_texture;
}

static void
gsk_vulkan_renderer_init (GskVulkanRenderer *self)
{
#ifdef G_ENABLE_DEBUG
  GskProfiler *profiler = gsk_renderer_get_profiler (GSK_RENDERER (self));
#endif

  gsk_ensure_resources ();

#ifdef G_ENABLE_DEBUG
  self->profile_counters.frames = gsk_profiler_add_counter (profiler, "frames", "Frames", FALSE);
  self->profile_counters.render_passes = gsk_profiler_add_counter (profiler, "render-passes", "Render passes", FALSE);
  self->profile_counters.fallback_pixels = gsk_profiler_add_counter (profiler, "fallback-pixels", "Fallback pixels", TRUE);
  self->profile_counters.texture_pixels = gsk_profiler_add_counter (profiler, "texture-pixels", "Texture pixels", TRUE);

  self->profile_timers.cpu_time = gsk_profiler_add_timer (profiler, "cpu-time", "CPU time", FALSE, TRUE);
  if (GSK_RENDERER_DEBUG_CHECK (GSK_RENDERER (self), SYNC))
    self->profile_timers.gpu_time = gsk_profiler_add_timer (profiler, "gpu-time", "GPU time", FALSE, TRUE);

  if (texture_pixels_counter == 0)
    {
      texture_pixels_counter = gdk_profiler_define_int_counter ("texture-pixels", "Texture Pixels");
      fallback_pixels_counter = gdk_profiler_define_int_counter ("fallback-pixels", "Fallback Pixels");
    }

#endif
}

static void
gsk_vulkan_renderer_clear_texture (gpointer p)
{
  GskVulkanTextureData *data = p;

  if (data->renderer != NULL)
    data->renderer->textures = g_slist_remove (data->renderer->textures, data);

  g_object_unref (data->image);

  g_free (data);
}

GskVulkanImage *
gsk_vulkan_renderer_get_texture_image (GskVulkanRenderer *self,
                                       GdkTexture        *texture)
{
  GskVulkanTextureData *data;

  data = gdk_texture_get_render_data (texture, self);
  if (data)
    return data->image;

  return NULL;
}

void
gsk_vulkan_renderer_add_texture_image (GskVulkanRenderer *self,
                                       GdkTexture        *texture,
                                       GskVulkanImage    *image)
{
  GskVulkanTextureData *data;

  data = g_new0 (GskVulkanTextureData, 1);
  data->image = image;
  data->texture = texture;
  data->renderer = self;

  if (gdk_texture_set_render_data (texture, self, data, gsk_vulkan_renderer_clear_texture))
    {
      g_object_ref (data->image);
      self->textures = g_slist_prepend (self->textures, data);
    }
  else
    {
      g_free (data);
    }
}

GskVulkanGlyphCache *
gsk_vulkan_renderer_get_glyph_cache (GskVulkanRenderer  *self)
{
  return self->glyph_cache;
}

/**
 * gsk_vulkan_renderer_new:
 *
 * Creates a new Vulkan renderer.
 *
 * The Vulkan renderer is a renderer that uses the Vulkan library for
 * rendering.
 *
 * This function is only available when GTK was compiled with Vulkan
 * support.
 *
 * Returns: a new Vulkan renderer
 **/
GskRenderer *
gsk_vulkan_renderer_new (void)
{
  return g_object_new (GSK_TYPE_VULKAN_RENDERER, NULL);
}
