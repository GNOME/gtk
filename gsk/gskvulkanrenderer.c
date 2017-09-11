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
  GHashTable *fonts;

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

GskVulkanImage *
gsk_vulkan_renderer_ref_glyph_image (GskVulkanRenderer  *self,
                                     GskVulkanUploader  *uploader,
                                     PangoFont          *font,
                                     PangoGlyphString   *glyphs)
{
  if (self->glyph_cache->image == NULL)
    self->glyph_cache->image = gsk_vulkan_image_new_from_data (uploader,
                                    cairo_image_surface_get_data (self->glyph_cache->surface),
                                    cairo_image_surface_get_width (self->glyph_cache->surface),
                                    cairo_image_surface_get_height (self->glyph_cache->surface),
                                    cairo_image_surface_get_stride (self->glyph_cache->surface));

  return g_object_ref (self->glyph_cache->image);
}

typedef struct _FontEntry FontEntry;
typedef struct _GlyphEntry GlyphEntry;

struct _FontEntry {
  PangoFont *font;
  PangoFontDescription *desc;
  guint hash;
  GArray *glyphs;
};

struct _GlyphEntry {
  PangoGlyph glyph;
  float tx, ty, tw, th;
  float ascent, height;
};

static FontEntry  *get_font_entry  (GlyphCache *cache,
                                    PangoFont  *font);
static GlyphEntry *get_glyph_entry (GlyphCache *cache,
                                    FontEntry  *fe,
                                    PangoGlyph  glyph,
                                    gboolean    may_add);

void
gsk_vulkan_renderer_get_glyph_coords (GskVulkanRenderer *self,
                                      PangoFont         *font,
                                      PangoGlyph         glyph,
                                      float             *tx,
                                      float             *ty,
                                      float             *tw,
                                      float             *th,
                                      float             *ascent,
                                      float             *height)
{
  FontEntry *fe;
  GlyphEntry *ge;

  fe = get_font_entry (self->glyph_cache, font);
  ge = get_glyph_entry (self->glyph_cache, fe, glyph, FALSE);

  if (ge)
    {
      *tx = ge->tx;
      *ty = ge->ty;
      *tw = ge->tw;
      *th = ge->th;
      *ascent = ge->ascent;
      *height = ge->height;
    }
}

/*** Glyph cache ***/

static gboolean
font_equal (gconstpointer v1, gconstpointer v2)
{
  const FontEntry *f1 = v1;
  const FontEntry *f2 = v2;

  return pango_font_description_equal (f1->desc, f2->desc);
}

static guint
font_hash (gconstpointer v)
{
  FontEntry *f = (FontEntry *)v;

  if (f->hash != 0)
    return f->hash;

  f->hash = pango_font_description_hash (f->desc);

  if (f->hash == 0)
    f->hash++;

  return f->hash;
}

static void
font_entry_free (gpointer v)
{
  FontEntry *f = v;

  pango_font_description_free (f->desc);
  g_object_unref (f->font);
  g_array_unref (f->glyphs);

  g_free (f);
}

static void
add_to_cache (GlyphCache *cache,
              PangoFont  *font,
              PangoGlyph  glyph,
              GlyphEntry *ge)
{
  PangoRectangle ink_rect;
  cairo_t *cr;
  cairo_scaled_font_t *scaled_font;
  cairo_glyph_t cg;

  pango_font_get_glyph_extents (font, glyph, &ink_rect, NULL);
  pango_extents_to_pixels (&ink_rect, NULL);

  if (cache->x + ink_rect.width >= cache->width)
    {
      /* start a new row */
      cache->y0 = cache->y + 1;
      cache->x = 1;
    }

  if (cache->y0 + ink_rect.height >= cache->height)
    {
      g_critical ("Drats! Out of cache space. We should really handle this");
      return;
    }

  cr = cairo_create (cache->surface);

  scaled_font = pango_cairo_font_get_scaled_font ((PangoCairoFont *)font);
  if (G_UNLIKELY (!scaled_font || cairo_scaled_font_status (scaled_font) != CAIRO_STATUS_SUCCESS))
    return;

  cairo_set_scaled_font (cr, scaled_font);
  cairo_set_source_rgba (cr, 0, 0, 0, 1);

  cg.index = glyph;
  cg.x = cache->x;
  cg.y = cache->y0 - ink_rect.y;

  cairo_show_glyphs (cr, &cg, 1);

  cairo_destroy (cr);

  cache->x = cache->x + ink_rect.width + 1;
  cache->y = MAX (cache->y, cache->y0 + ink_rect.height + 1);

  ge->glyph = glyph;
  ge->tx = cg.x / cache->width;
  ge->ty = (cg.y + ink_rect.y) / cache->height;
  ge->tw = (float)ink_rect.width / cache->width;
  ge->th = (float)ink_rect.height / cache->height;
  ge->ascent = (float) - ink_rect.y;
  ge->height = (float) ink_rect.height;
}

static GlyphEntry *
get_glyph_entry (GlyphCache *cache,
                 FontEntry  *fe,
                 PangoGlyph  glyph,
                 gboolean    may_add)
{
  int i;
  GlyphEntry g;
  GlyphEntry *ge;

  for (i = 0; i < fe->glyphs->len; i++)
    {
      ge = &g_array_index (fe->glyphs, GlyphEntry, i);

      if (ge->glyph == glyph)
        return ge;
    }

  if (!may_add)
    return NULL;

  g_clear_object (&cache->image);

  add_to_cache (cache, fe->font, glyph, &g);

  g_array_append_val (fe->glyphs, g);
  ge = &g_array_index (fe->glyphs, GlyphEntry, fe->glyphs->len - 1);

  return ge;
}

static FontEntry *
get_font_entry (GlyphCache *cache,
                PangoFont  *font)
{
  FontEntry key;
  FontEntry *fe;

  key.font = font;
  key.desc = pango_font_describe (font);
  key.hash = 0;

  fe = g_hash_table_lookup (cache->fonts, &key);
  if (fe == NULL)
    {
      fe = g_new0 (FontEntry, 1);
      fe->font = g_object_ref (font);
      fe->desc = key.desc;
      fe->hash = 0;
      fe->glyphs = g_array_sized_new (FALSE, FALSE, sizeof (GlyphEntry), 256);
      g_hash_table_add (cache->fonts, fe);
    }
  else
    pango_font_description_free (key.desc);

  return fe;
}

void
gsk_vulkan_renderer_cache_glyphs (GskVulkanRenderer *self,
                                  PangoFont         *font,
                                  PangoGlyphString  *glyphs)
{
  FontEntry *fe;
  int i;

  fe = get_font_entry (self->glyph_cache, font);

  for (i = 0; i < glyphs->num_glyphs; i++)
    {
      PangoGlyphInfo *gi = &glyphs->glyphs[i];

      if (gi->glyph != PANGO_GLYPH_EMPTY)
        {
          if (!(gi->glyph & PANGO_GLYPH_UNKNOWN_FLAG))
            get_glyph_entry (self->glyph_cache, fe, gi->glyph, TRUE);
        }
    }
}

static GlyphCache *
create_glyph_cache (void)
{
  GlyphCache *cache;

  cache = g_new0 (GlyphCache, 1);
  cache->fonts = g_hash_table_new_full (font_hash, font_equal, NULL, font_entry_free);
  cache->surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 1024, 1024);
  cache->width = 1024;
  cache->height = 1024;
  cache->y0 = 1;
  cache->y = 1;
  cache->x = 1;

  return cache;
}

static void
free_glyph_cache (GlyphCache *cache)
{
  g_hash_table_unref (cache->fonts);
  cairo_surface_destroy (cache->surface);
  g_free (cache);
}

static void
dump_glyph_cache_stats (GlyphCache *cache)
{
  GHashTableIter iter;
  FontEntry *fe;
  static gint64 time;
  gint64 now;

  if (!cache->fonts)
    return;

  now = g_get_monotonic_time ();
  if (now - time < 1000000)
    return;

  time = now;

  g_print ("Glyph cache:\n");
  g_hash_table_iter_init (&iter, cache->fonts);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&fe))
    {
      char *s;

      s = pango_font_description_to_string (fe->desc);
      g_print ("%s: %d glyphs cached\n", s, fe->glyphs->len);
      g_free (s);
    }
  g_print ("---\n");
  cairo_surface_write_to_png (cache->surface, "glyph-cache.png");
}
