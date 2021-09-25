#include "config.h"

#include "gskvulkanrendererprivate.h"

#include "gskdebugprivate.h"
#include "gskprivate.h"
#include "gskrendererprivate.h"
#include "gskrendernodeprivate.h"
#include "gskvulkanbufferprivate.h"
#include "gskvulkanimageprivate.h"
#include "gskvulkanpipelineprivate.h"
#include "gskvulkanrenderprivate.h"
#include "gskvulkanglyphcacheprivate.h"

#include "gdk/gdktextureprivate.h"
#include "gdk/gdkprofilerprivate.h"

#include <graphene.h>

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

  GskVulkanRender *render;

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
  GdkSurface *window;
  int scale_factor;
  gsize width, height;
  guint i;

  gsk_vulkan_renderer_free_targets (self);

  self->n_targets = gdk_vulkan_context_get_n_images (context);
  self->targets = g_new (GskVulkanImage *, self->n_targets);

  window = gsk_renderer_get_surface (GSK_RENDERER (self));
  scale_factor = gdk_surface_get_scale_factor (window);
  width = gdk_surface_get_width (window) * scale_factor;
  height = gdk_surface_get_height (window) * scale_factor;

  for (i = 0; i < self->n_targets; i++)
    {
      self->targets[i] = gsk_vulkan_image_new_for_swapchain (self->vulkan,
                                                             gdk_vulkan_context_get_image (context, i),
                                                             gdk_vulkan_context_get_image_format (self->vulkan),
                                                             width, height);
    }
}

static gboolean
gsk_vulkan_renderer_realize (GskRenderer  *renderer,
                             GdkSurface   *surface,
                             GError      **error)
{
  GskVulkanRenderer *self = GSK_VULKAN_RENDERER (renderer);

  if (surface == NULL)
    {
      g_set_error (error, GDK_VULKAN_ERROR, GDK_VULKAN_ERROR_UNSUPPORTED,
                   "The Vulkan renderer does not support surfaceless rendering yet.");
      return FALSE;
    }

  self->vulkan = gdk_surface_create_vulkan_context (surface, error);
  if (self->vulkan == NULL)
    return FALSE;

  g_signal_connect (self->vulkan,
                    "images-updated",
                    G_CALLBACK (gsk_vulkan_renderer_update_images_cb),
                    self);
  gsk_vulkan_renderer_update_images_cb (self->vulkan, self);

  self->render = gsk_vulkan_render_new (renderer, self->vulkan);

  self->glyph_cache = gsk_vulkan_glyph_cache_new (renderer, self->vulkan);

  return TRUE;
}

static void
gsk_vulkan_renderer_unrealize (GskRenderer *renderer)
{
  GskVulkanRenderer *self = GSK_VULKAN_RENDERER (renderer);
  GSList *l;

  g_clear_object (&self->glyph_cache);

  for (l = self->textures; l; l = l->next)
    {
      GskVulkanTextureData *data = l->data;

      data->renderer = NULL;
      gdk_texture_clear_render_data (data->texture);
    }
  g_clear_pointer (&self->textures, g_slist_free);

  g_clear_pointer (&self->render, gsk_vulkan_render_free);

  gsk_vulkan_renderer_free_targets (self);
  g_signal_handlers_disconnect_by_func(self->vulkan,
                                       gsk_vulkan_renderer_update_images_cb,
                                       self);

  g_clear_object (&self->vulkan);
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

  image = gsk_vulkan_image_new_for_framebuffer (self->vulkan,
                                                ceil (viewport->size.width),
                                                ceil (viewport->size.height));

  gsk_vulkan_render_reset (render, image, viewport, NULL);

  gsk_vulkan_render_add_node (render, root);

  gsk_vulkan_render_upload (render);

  gsk_vulkan_render_draw (render);

  texture = gsk_vulkan_render_download_target (render);

  g_object_unref (image);
  gsk_vulkan_render_free (render);

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
  const cairo_region_t *clip;
#ifdef G_ENABLE_DEBUG
  GskProfiler *profiler;
  gint64 cpu_time;
#endif

#ifdef G_ENABLE_DEBUG
  profiler = gsk_renderer_get_profiler (renderer);
  gsk_profiler_counter_set (profiler, self->profile_counters.fallback_pixels, 0);
  gsk_profiler_counter_set (profiler, self->profile_counters.texture_pixels, 0);
  gsk_profiler_counter_set (profiler, self->profile_counters.render_passes, 0);
  gsk_profiler_timer_begin (profiler, self->profile_timers.cpu_time);
#endif

  gdk_draw_context_begin_frame (GDK_DRAW_CONTEXT (self->vulkan), region);
  render = self->render;

  clip = gdk_draw_context_get_frame_region (GDK_DRAW_CONTEXT (self->vulkan));
  gsk_vulkan_render_reset (render, self->targets[gdk_vulkan_context_get_draw_index (self->vulkan)], NULL, clip);

  gsk_vulkan_render_add_node (render, root);

  gsk_vulkan_render_upload (render);

  gsk_vulkan_render_draw (render);

#ifdef G_ENABLE_DEBUG
  gsk_profiler_counter_inc (profiler, self->profile_counters.frames);

  cpu_time = gsk_profiler_timer_end (profiler, self->profile_timers.cpu_time);
  gsk_profiler_timer_set (profiler, self->profile_timers.cpu_time, cpu_time);

  gsk_profiler_push_samples (profiler);
#endif

  gdk_draw_context_end_frame (GDK_DRAW_CONTEXT (self->vulkan));
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

  g_slice_free (GskVulkanTextureData, data);
}

GskVulkanImage *
gsk_vulkan_renderer_ref_texture_image (GskVulkanRenderer *self,
                                       GdkTexture        *texture,
                                       GskVulkanUploader *uploader)
{
  GskVulkanTextureData *data;
  cairo_surface_t *surface;
  GskVulkanImage *image;

  data = gdk_texture_get_render_data (texture, self);
  if (data)
    return g_object_ref (data->image);

  surface = gdk_texture_download_surface (texture,NULL);
  image = gsk_vulkan_image_new_from_data (uploader,
                                          cairo_image_surface_get_data (surface),
                                          cairo_image_surface_get_width (surface),
                                          cairo_image_surface_get_height (surface),
                                          cairo_image_surface_get_stride (surface));
  cairo_surface_destroy (surface);

  data = g_slice_new0 (GskVulkanTextureData);
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
      g_slice_free (GskVulkanTextureData, data);
    }

  return image;
}

GskVulkanImage *
gsk_vulkan_renderer_ref_glyph_image (GskVulkanRenderer  *self,
                                     GskVulkanUploader  *uploader,
                                     guint               index)
{
  return g_object_ref (gsk_vulkan_glyph_cache_get_glyph_image (self->glyph_cache, uploader, index));
}

guint
gsk_vulkan_renderer_cache_glyph (GskVulkanRenderer *self,
                                 PangoFont         *font,
                                 PangoGlyph         glyph,
                                 int                x,
                                 int                y,
                                 float              scale)
{
  return gsk_vulkan_glyph_cache_lookup (self->glyph_cache, TRUE, font, glyph, x, y, scale)->texture_index;
}

GskVulkanCachedGlyph *
gsk_vulkan_renderer_get_cached_glyph (GskVulkanRenderer *self,
                                      PangoFont         *font,
                                      PangoGlyph         glyph,
                                      int                x,
                                      int                y,
                                      float              scale)
{
  return gsk_vulkan_glyph_cache_lookup (self->glyph_cache, FALSE, font, glyph, x, y, scale);
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
