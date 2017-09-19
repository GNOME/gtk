#include "config.h"

#include "gskvulkanrendererprivate.h"

#include "gskdebugprivate.h"
#include "gskprivate.h"
#include "gskrendererprivate.h"
#include "gskrendernodeprivate.h"
#include "gsktextureprivate.h"
#include "gskvulkanbufferprivate.h"
#include "gskvulkanimageprivate.h"
#include "gskvulkanpipelineprivate.h"
#include "gskvulkanrenderprivate.h"

#include <graphene.h>

typedef struct _GskVulkanTextureData GskVulkanTextureData;

struct _GskVulkanTextureData {
  GskTexture *texture;
  GskVulkanImage *image;
  GskVulkanRenderer *renderer;
};

#ifdef G_ENABLE_DEBUG
typedef struct {
  GQuark cpu_time;
  GQuark gpu_time;
} ProfileTimers;
#endif

typedef struct _GlyphCache GlyphCache;

struct _GlyphCache {
  GHashTable *hash_table;

  cairo_surface_t *surface;
  int width, height;
  int x, y, y0;

  GskVulkanImage *image;
};


static GlyphCache *create_glyph_cache     (void);
static void        free_glyph_cache       (GlyphCache *cache);
static void        dump_glyph_cache_stats (GlyphCache *cache);

struct _GskVulkanRenderer
{
  GskRenderer parent_instance;

  GdkVulkanContext *vulkan;

  guint n_targets;
  GskVulkanImage **targets;

  VkSampler sampler;

  GskVulkanRender *render;

  GSList *textures;

  GlyphCache *glyph_cache;

#ifdef G_ENABLE_DEBUG
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
  GdkWindow *window;
  gint scale_factor;
  gsize width, height;
  guint i;

  gsk_vulkan_renderer_free_targets (self);

  self->n_targets = gdk_vulkan_context_get_n_images (context);
  self->targets = g_new (GskVulkanImage *, self->n_targets);

  window = gsk_renderer_get_window (GSK_RENDERER (self));
  scale_factor = gdk_window_get_scale_factor (window);
  width = gdk_window_get_width (window) * scale_factor;
  height = gdk_window_get_height (window) * scale_factor;

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
                             GdkWindow    *window,
                             GError      **error)
{
  GskVulkanRenderer *self = GSK_VULKAN_RENDERER (renderer);
  VkDevice device;

  self->vulkan = gdk_window_create_vulkan_context (window, error);
  if (self->vulkan == NULL)
    return FALSE;

  device = gdk_vulkan_context_get_device (self->vulkan);

  GSK_VK_CHECK (vkCreateSampler, device,
                                 &(VkSamplerCreateInfo) {
                                     .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                     .magFilter = VK_FILTER_LINEAR,
                                     .minFilter = VK_FILTER_LINEAR,
                                     .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                     .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                     .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                     .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                                     .unnormalizedCoordinates = VK_FALSE
                                 },
                                 NULL,
                                 &self->sampler);

  g_signal_connect (self->vulkan,
                    "images-updated",
                    G_CALLBACK (gsk_vulkan_renderer_update_images_cb),
                    self);
  gsk_vulkan_renderer_update_images_cb (self->vulkan, self);

  self->render = gsk_vulkan_render_new (renderer, self->vulkan);

  self->glyph_cache = create_glyph_cache ();

  return TRUE;
}

static void
gsk_vulkan_renderer_unrealize (GskRenderer *renderer)
{
  GskVulkanRenderer *self = GSK_VULKAN_RENDERER (renderer);
  VkDevice device;
  GSList *l;

  free_glyph_cache (self->glyph_cache);
  self->glyph_cache = NULL;

  for (l = self->textures; l; l = l->next)
    {
      GskVulkanTextureData *data = l->data;

      data->renderer = NULL;
      gsk_texture_clear_render_data (data->texture);
    }
  g_clear_pointer (&self->textures, (GDestroyNotify) g_slist_free);

  g_clear_pointer (&self->render, gsk_vulkan_render_free);

  device = gdk_vulkan_context_get_device (self->vulkan);

  gsk_vulkan_renderer_free_targets (self);
  g_signal_handlers_disconnect_by_func(self->vulkan,
                                       gsk_vulkan_renderer_update_images_cb,
                                       self);

  vkDestroySampler (device,
                    self->sampler,
                    NULL);
  self->sampler = VK_NULL_HANDLE;

  g_clear_object (&self->vulkan);
}

static GskTexture *
gsk_vulkan_renderer_render_texture (GskRenderer           *renderer,
                                    GskRenderNode         *root,
                                    const graphene_rect_t *viewport)
{
  GskVulkanRenderer *self = GSK_VULKAN_RENDERER (renderer);
  GskVulkanRender *render;
  GskVulkanImage *image;
  GskTexture *texture;
#ifdef G_ENABLE_DEBUG
  GskProfiler *profiler;
  gint64 cpu_time;
#endif

#ifdef G_ENABLE_DEBUG
  profiler = gsk_renderer_get_profiler (renderer);
  gsk_profiler_timer_begin (profiler, self->profile_timers.cpu_time);
#endif

  render = gsk_vulkan_render_new (renderer, self->vulkan);

  image = gsk_vulkan_image_new_for_framebuffer (self->vulkan,
                                                ceil (viewport->size.width),
                                                ceil (viewport->size.height));

  gsk_vulkan_render_reset (render, image, viewport);

  gsk_vulkan_render_add_node (render, root);

  gsk_vulkan_render_upload (render);

  gsk_vulkan_render_draw (render, self->sampler);

  texture = gsk_vulkan_render_download_target (render);

  g_object_unref (image);
  gsk_vulkan_render_free (render);

#ifdef G_ENABLE_DEBUG
  cpu_time = gsk_profiler_timer_end (profiler, self->profile_timers.cpu_time);
  gsk_profiler_timer_set (profiler, self->profile_timers.cpu_time, cpu_time);

  gsk_profiler_push_samples (profiler);
#endif

  return texture;
}

static void
gsk_vulkan_renderer_render (GskRenderer   *renderer,
                            GskRenderNode *root)
{
  GskVulkanRenderer *self = GSK_VULKAN_RENDERER (renderer);
  GskVulkanRender *render;
#ifdef G_ENABLE_DEBUG
  GskProfiler *profiler;
  gint64 cpu_time;
#endif

#ifdef G_ENABLE_DEBUG
  profiler = gsk_renderer_get_profiler (renderer);
  gsk_profiler_timer_begin (profiler, self->profile_timers.cpu_time);
#endif

  render = self->render;

  gsk_vulkan_render_reset (render, self->targets[gdk_vulkan_context_get_draw_index (self->vulkan)], NULL);

  gsk_vulkan_render_add_node (render, root);

  gsk_vulkan_render_upload (render);

  gsk_vulkan_render_draw (render, self->sampler);

#ifdef G_ENABLE_DEBUG
  cpu_time = gsk_profiler_timer_end (profiler, self->profile_timers.cpu_time);
  gsk_profiler_timer_set (profiler, self->profile_timers.cpu_time, cpu_time);

  gsk_profiler_push_samples (profiler);
#endif
}

static GdkDrawingContext *
gsk_vulkan_renderer_begin_draw_frame (GskRenderer          *renderer,
                                      const cairo_region_t *region)
{
  GskVulkanRenderer *self = GSK_VULKAN_RENDERER (renderer);
  GdkDrawingContext *result;

  GSK_NOTE(RENDERER, dump_glyph_cache_stats (self->glyph_cache));

  result = gdk_window_begin_draw_frame (gsk_renderer_get_window (renderer),
                                        GDK_DRAW_CONTEXT (self->vulkan),
                                        region);

  return result;
}

static void
gsk_vulkan_renderer_class_init (GskVulkanRendererClass *klass)
{
  GskRendererClass *renderer_class = GSK_RENDERER_CLASS (klass);

  renderer_class->realize = gsk_vulkan_renderer_realize;
  renderer_class->unrealize = gsk_vulkan_renderer_unrealize;
  renderer_class->render = gsk_vulkan_renderer_render;
  renderer_class->render_texture = gsk_vulkan_renderer_render_texture;
  renderer_class->begin_draw_frame = gsk_vulkan_renderer_begin_draw_frame;
}

static void
gsk_vulkan_renderer_init (GskVulkanRenderer *self)
{
#ifdef G_ENABLE_DEBUG
  GskProfiler *profiler = gsk_renderer_get_profiler (GSK_RENDERER (self));
#endif

  gsk_ensure_resources ();

#ifdef G_ENABLE_DEBUG
  self->profile_timers.cpu_time = gsk_profiler_add_timer (profiler, "cpu-time", "CPU time", FALSE, TRUE);
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
                                       GskTexture        *texture,
                                       GskVulkanUploader *uploader)
{
  GskVulkanTextureData *data;
  cairo_surface_t *surface;
  GskVulkanImage *image;

  data = gsk_texture_get_render_data (texture, self);
  if (data)
    return g_object_ref (data->image);

  surface = gsk_texture_download_surface (texture);
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

  if (gsk_texture_set_render_data (texture, self, data, gsk_vulkan_renderer_clear_texture))
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

typedef struct _GlyphCacheKey GlyphCacheKey;
typedef struct _GlyphCacheValue GlyphCacheValue;

struct _GlyphCacheKey {
  PangoFont *font;
  PangoGlyph glyph;
};

/*** Glyph cache ***/

static gboolean
glyph_cache_equal (gconstpointer v1, gconstpointer v2)
{
  const GlyphCacheKey *key1 = v1;
  const GlyphCacheKey *key2 = v2;

  return key1->font == key2->font &&
         key1->glyph == key2->glyph;
}

static guint
glyph_cache_hash (gconstpointer v)
{
  const GlyphCacheKey *key = v;

  return GPOINTER_TO_UINT (key->font) ^ key->glyph;
}

static void
glyph_cache_key_free (gpointer v)
{
  GlyphCacheKey *f = v;

  g_object_unref (f->font);
  g_free (f);
}

static void
glyph_cache_value_free (gpointer v)
{
  g_free (v);
}

static void
add_to_cache (GlyphCache           *cache,
              PangoFont            *font,
              PangoGlyph            glyph,
              GskVulkanCachedGlyph *value)
{
  cairo_t *cr;
  cairo_scaled_font_t *scaled_font;
  cairo_glyph_t cg;

  if (cache->x + value->draw_width + 1 >= cache->width)
    {
      /* start a new row */
      cache->y0 = cache->y + 1;
      cache->x = 1;
    }

  if (cache->y0 + value->draw_height + 1 >= cache->height)
    {
      g_critical ("Drats! Out of cache space. We should really handle this");
      return;
    }

  cr = cairo_create (cache->surface);

  scaled_font = pango_cairo_font_get_scaled_font ((PangoCairoFont *)font);
  if (G_UNLIKELY (!scaled_font || cairo_scaled_font_status (scaled_font) != CAIRO_STATUS_SUCCESS))
    return;

  cairo_set_scaled_font (cr, scaled_font);
  cairo_set_source_rgba (cr, 1, 1, 1, 1);

  cg.index = glyph;
  cg.x = cache->x - value->draw_x;
  cg.y = cache->y0 - value->draw_y;

  cairo_show_glyphs (cr, &cg, 1);

  cairo_destroy (cr);

  cache->x = cache->x + value->draw_width + 1;
  cache->y = MAX (cache->y, cache->y0 + value->draw_height + 1);

  value->tx = (cg.x + value->draw_x) / cache->width;
  value->ty = (cg.y + value->draw_y) / cache->height;
  value->tw = (float)value->draw_width / cache->width;
  value->th = (float)value->draw_height / cache->height;

  value->texture_index = 0;
}

static GskVulkanCachedGlyph *
glyph_cache_lookup (GlyphCache *cache,
                    gboolean    create,
                    PangoFont  *font,
                    PangoGlyph  glyph)
{
  GlyphCacheKey lookup_key;
  GskVulkanCachedGlyph *value;

  lookup_key.font = font;
  lookup_key.glyph = glyph;

  value = g_hash_table_lookup (cache->hash_table, &lookup_key);

  if (create && value == NULL)
    {
      GlyphCacheKey *key;
      PangoRectangle ink_rect;

      value = g_new (GskVulkanCachedGlyph, 1);

      value->texture_index = 0;

      pango_font_get_glyph_extents (font, glyph, &ink_rect, NULL);
      pango_extents_to_pixels (&ink_rect, NULL);

      value->draw_x = ink_rect.x;
      value->draw_y = ink_rect.y;
      value->draw_width = ink_rect.width;
      value->draw_height = ink_rect.height;

      if (ink_rect.width > 0 && ink_rect.height > 0)
        add_to_cache (cache, font, glyph, value);

      key = g_new (GlyphCacheKey, 1);
      key->font = g_object_ref (font);
      key->glyph = glyph;

      g_hash_table_insert (cache->hash_table, key, value);
    }

  return value;
}

static void
free_glyph_cache (GlyphCache *cache)
{
  g_hash_table_unref (cache->hash_table);
  cairo_surface_destroy (cache->surface);
  g_free (cache);
}

static void
dump_glyph_cache_stats (GlyphCache *cache)
{
  static gint64 time;
  gint64 now;

  if (!cache->hash_table)
    return;

  now = g_get_monotonic_time ();
  if (now - time < 1000000)
    return;

  time = now;

  cairo_surface_write_to_png (cache->surface, "gsk-glyph-cache.png");
}

guint
gsk_vulkan_renderer_cache_glyph (GskVulkanRenderer *self,
                                 PangoFont         *font,
                                 PangoGlyph         glyph)
{
  GskVulkanCachedGlyph *value;

  value = glyph_cache_lookup (self->glyph_cache, TRUE, font, glyph);

  return value->texture_index;
}

static GlyphCache *
create_glyph_cache (void)
{
  GlyphCache *cache;

  cache = g_new0 (GlyphCache, 1);
  cache->hash_table = g_hash_table_new_full (glyph_cache_hash, glyph_cache_equal,
                                             glyph_cache_key_free, glyph_cache_value_free);
  cache->width = 1024;
  cache->height = 1024;
  cache->surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, cache->width, cache->height);
  cache->y0 = 1;
  cache->y = 1;
  cache->x = 1;

  return cache;
}

GskVulkanImage *
gsk_vulkan_renderer_ref_glyph_image (GskVulkanRenderer  *self,
                                     GskVulkanUploader  *uploader,
                                     guint               index)
{
  if (self->glyph_cache->image == NULL)
    self->glyph_cache->image = gsk_vulkan_image_new_from_data (uploader,
                                    cairo_image_surface_get_data (self->glyph_cache->surface),
                                    cairo_image_surface_get_width (self->glyph_cache->surface),
                                    cairo_image_surface_get_height (self->glyph_cache->surface),
                                    cairo_image_surface_get_stride (self->glyph_cache->surface));

  return g_object_ref (self->glyph_cache->image);
}

GskVulkanCachedGlyph *
gsk_vulkan_renderer_get_cached_glyph (GskVulkanRenderer *self,
                                      PangoFont         *font,
                                      PangoGlyph         glyph)
{
  GlyphCacheKey lookup_key;

  lookup_key.font = font;
  lookup_key.glyph = glyph;

  return g_hash_table_lookup (self->glyph_cache->hash_table, &lookup_key);
}
