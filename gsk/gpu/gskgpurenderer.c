#include "config.h"

#include "gskgpurendererprivate.h"

#include "gskdebugprivate.h"
#include "gskgpudeviceprivate.h"
#include "gskgpuframeprivate.h"
#include "gskprivate.h"
#include "gskrendererprivate.h"
#include "gskrendernodeprivate.h"
#include "gskgpuimageprivate.h"

#include "gdk/gdkdebugprivate.h"
#include "gdk/gdkdisplayprivate.h"
#include "gdk/gdkdmabuftextureprivate.h"
#include "gdk/gdkmemorytextureprivate.h"
#include "gdk/gdkdrawcontextprivate.h"
#include "gdk/gdkprofilerprivate.h"
#include "gdk/gdktextureprivate.h"
#include "gdk/gdkdrawcontextprivate.h"
#include "gdk/gdkcolorstateprivate.h"

#include <graphene.h>

#define GSK_GPU_MAX_FRAMES 4

static const GdkDebugKey gsk_gpu_optimization_keys[] = {
  { "clear",     GSK_GPU_OPTIMIZE_CLEAR,             "Use shaders instead of vkCmdClearAttachment()/glClear()" },
  { "merge",     GSK_GPU_OPTIMIZE_MERGE,             "Use one vkCmdDraw()/glDrawArrays() per operation" },
  { "blit",      GSK_GPU_OPTIMIZE_BLIT,              "Use shaders instead of vkCmdBlit()/glBlitFramebuffer()" },
  { "gradients", GSK_GPU_OPTIMIZE_GRADIENTS,         "Don't supersample gradients" },
  { "mipmap",    GSK_GPU_OPTIMIZE_MIPMAP,            "Avoid creating mipmaps" },
  { "to-image",  GSK_GPU_OPTIMIZE_TO_IMAGE,          "Don't fast-path creation of images for nodes" },
  { "occlusion", GSK_GPU_OPTIMIZE_OCCLUSION_CULLING, "Disable occlusion culling via opaque node tracking" },
  { "repeat",    GSK_GPU_OPTIMIZE_REPEAT,            "Repeat drawing operations instead of using offscreen and GL_REPEAT" },
};

typedef struct _GskGpuRendererPrivate GskGpuRendererPrivate;

struct _GskGpuRendererPrivate
{
  GskGpuDevice *device;
  GdkDrawContext *context;
  GskGpuOptimizations optimizations;

  GskGpuFrame *frames[GSK_GPU_MAX_FRAMES];
};

static void     gsk_gpu_renderer_dmabuf_downloader_init         (GdkDmabufDownloaderInterface   *iface);

G_DEFINE_TYPE_EXTENDED (GskGpuRenderer, gsk_gpu_renderer, GSK_TYPE_RENDERER, 0,
                        G_ADD_PRIVATE (GskGpuRenderer)
                        G_IMPLEMENT_INTERFACE (GDK_TYPE_DMABUF_DOWNLOADER,
                                               gsk_gpu_renderer_dmabuf_downloader_init))

static void
gsk_gpu_renderer_make_current (GskGpuRenderer *self)
{
  GSK_GPU_RENDERER_GET_CLASS (self)->make_current (self);
}

static gpointer
gsk_gpu_renderer_save_current (GskGpuRenderer *self)
{
  return GSK_GPU_RENDERER_GET_CLASS (self)->save_current (self);
}

static void
gsk_gpu_renderer_restore_current (GskGpuRenderer *self,
                                  gpointer        current)
{
  GSK_GPU_RENDERER_GET_CLASS (self)->restore_current (self, current);
}

static GskGpuFrame *
gsk_gpu_renderer_create_frame (GskGpuRenderer *self)
{
  GskGpuRendererPrivate *priv = gsk_gpu_renderer_get_instance_private (self);
  GskGpuRendererClass *klass = GSK_GPU_RENDERER_GET_CLASS (self);
  GskGpuFrame *result;

  result = g_object_new (klass->frame_type, NULL);

  gsk_gpu_frame_setup (result, self, priv->device, priv->optimizations);

  return result;
}

static GskGpuFrame *
gsk_gpu_renderer_get_frame (GskGpuRenderer *self)
{
  GskGpuRendererPrivate *priv = gsk_gpu_renderer_get_instance_private (self);
  GskGpuFrame *earliest_frame = NULL;
  gint64 earliest_time = G_MAXINT64;
  guint i;

  for (i = 0; i < G_N_ELEMENTS (priv->frames); i++)
    {
      gint64 timestamp;

      if (priv->frames[i] == NULL)
        {
          priv->frames[i] = gsk_gpu_renderer_create_frame (self);
          return priv->frames[i];
        }

      if (!gsk_gpu_frame_is_busy (priv->frames[i]))
        return priv->frames[i];

      timestamp = gsk_gpu_frame_get_timestamp (priv->frames[i]);
      if (timestamp < earliest_time)
        {
          earliest_time = timestamp;
          earliest_frame = priv->frames[i];
        }
    }

  g_assert (earliest_frame);

  gsk_gpu_frame_wait (earliest_frame);

  return earliest_frame;
}

static void
gsk_gpu_renderer_dmabuf_downloader_close (GdkDmabufDownloader *downloader)
{
  gsk_renderer_unrealize (GSK_RENDERER (downloader));
}

static gboolean
gsk_gpu_renderer_dmabuf_downloader_download (GdkDmabufDownloader   *downloader,
                                             GdkDmabufTexture      *texture,
                                             guchar                *data,
                                             const GdkMemoryLayout *layout,
                                             GdkColorState         *color_state)
{
  GskGpuRenderer *self = GSK_GPU_RENDERER (downloader);
  GskGpuFrame *frame;
  gpointer previous;
  gboolean retval = FALSE;

  previous = gsk_gpu_renderer_save_current (self);

  gsk_gpu_renderer_make_current (self);

  frame = gsk_gpu_renderer_get_frame (self);

  if (gsk_gpu_frame_download_texture (frame,
                                      g_get_monotonic_time (),
                                      GDK_TEXTURE (texture),
                                      data,
                                      layout,
                                      color_state))
    {
      retval = TRUE;

      GDK_DISPLAY_DEBUG (gdk_dmabuf_texture_get_display (texture), DMABUF,
                         "Used %s for downloading %dx%d dmabuf (format %.4s:%#" G_GINT64_MODIFIER "x)",
                         G_OBJECT_TYPE_NAME (downloader),
                         gdk_texture_get_width (GDK_TEXTURE (texture)),
                         gdk_texture_get_height (GDK_TEXTURE (texture)),
                         (char *)&(gdk_dmabuf_texture_get_dmabuf (texture)->fourcc),
                         gdk_dmabuf_texture_get_dmabuf (texture)->modifier);

      gsk_gpu_frame_wait (frame);
    }

  gsk_gpu_renderer_restore_current (self, previous);

  return retval;
}

static void
gsk_gpu_renderer_dmabuf_downloader_init (GdkDmabufDownloaderInterface *iface)
{
  iface->close = gsk_gpu_renderer_dmabuf_downloader_close;
  iface->download = gsk_gpu_renderer_dmabuf_downloader_download;
}

static gboolean
gsk_gpu_renderer_realize (GskRenderer  *renderer,
                          GdkDisplay   *display,
                          GdkSurface   *surface,
                          gboolean      attach,
                          GError      **error)
{
  GskGpuRenderer *self = GSK_GPU_RENDERER (renderer);
  GskGpuRendererPrivate *priv = gsk_gpu_renderer_get_instance_private (self);
  GskGpuOptimizations context_optimizations;
  G_GNUC_UNUSED gint64 start_time = GDK_PROFILER_CURRENT_TIME;

  priv->device = GSK_GPU_RENDERER_GET_CLASS (self)->get_device (display, error);
  if (priv->device == NULL)
    return FALSE;

  priv->context = GSK_GPU_RENDERER_GET_CLASS (self)->create_context (self, display, surface, &context_optimizations, error);
  if (priv->context == NULL)
    {
      g_clear_object (&priv->device);
      return FALSE;
    }
  if (attach && !gdk_draw_context_attach (priv->context, error))
    {
      g_clear_object (&priv->context);
      g_clear_object (&priv->device);
      return FALSE;
    }

  priv->optimizations &= context_optimizations;

  gdk_profiler_end_mark (start_time, "Realize GskGpuRenderer", NULL);

  return TRUE;
}

static void
gsk_gpu_renderer_unrealize (GskRenderer *renderer)
{
  GskGpuRenderer *self = GSK_GPU_RENDERER (renderer);
  GskGpuRendererPrivate *priv = gsk_gpu_renderer_get_instance_private (self);
  gsize i;

  gsk_gpu_renderer_make_current (self);

  for (i = 0; i < G_N_ELEMENTS (priv->frames); i++)
    {
      if (priv->frames[i] == NULL)
        break;
      if (gsk_gpu_frame_is_busy (priv->frames[i]))
        gsk_gpu_frame_wait (priv->frames[i]);
      g_clear_object (&priv->frames[i]);
    }

  gdk_draw_context_detach (priv->context);

  g_clear_object (&priv->context);
  g_clear_object (&priv->device);
}

static GdkTexture *
gsk_gpu_renderer_fallback_render_texture (GskGpuRenderer        *self,
                                          GskRenderNode         *root,
                                          const graphene_rect_t *rounded_viewport)
{
  GskGpuRendererPrivate *priv = gsk_gpu_renderer_get_instance_private (self);
  GskGpuImage *image;
  gsize width, height, max_size, image_width, image_height;
  gsize x, y;
  GdkMemoryDepth depth;
  GdkColorState *color_state;
  GBytes *bytes;
  guchar *data;
  GdkTexture *texture;
  cairo_region_t *clip_region;
  GskGpuFrame *frame;
  GdkMemoryLayout layout;

  max_size = gsk_gpu_device_get_max_image_size (priv->device);
  depth = gsk_render_node_get_preferred_depth (root);
  do
    {
      image = gsk_gpu_device_create_download_image (priv->device,
                                                    gsk_render_node_get_preferred_depth (root),
                                                    MIN (max_size, rounded_viewport->size.width),
                                                    MIN (max_size, rounded_viewport->size.height));
      max_size /= 2;
    }
  while (image == NULL);

  width = rounded_viewport->size.width;
  height = rounded_viewport->size.height;

  if (!gdk_memory_layout_try_init (&layout, 
                                   gsk_gpu_image_get_format (image),
                                   width,
                                   height,
                                   1) ||
      !(data = g_malloc (layout.size)))
    {
      g_critical ("Image size %zux%zu too large", width, height);
      return NULL;
    }

  image_width = gsk_gpu_image_get_width (image);
  image_height = gsk_gpu_image_get_height (image);

  if (gsk_gpu_image_get_conversion (image) & GSK_GPU_CONVERSION_SRGB)
    color_state = GDK_COLOR_STATE_SRGB_LINEAR;
  else
    color_state = GDK_COLOR_STATE_SRGB;

  for (y = 0; y < height; y += image_height)
    {
      for (x = 0; x < width; x += image_width)
        {
          gsize tile_width, tile_height;
          GdkMemoryLayout sub;

          tile_width = MIN (image_width, width - x);
          tile_height = MIN (image_height, height - y);
          texture = NULL;

          if (image == NULL)
            {
              image = gsk_gpu_device_create_download_image (priv->device,
                                                            depth,
                                                            tile_width, tile_height);

              if (gsk_gpu_image_get_conversion (image) & GSK_GPU_CONVERSION_SRGB)
                color_state = GDK_COLOR_STATE_SRGB_LINEAR;
              else
                color_state = GDK_COLOR_STATE_SRGB;
            }

          clip_region = cairo_region_create_rectangle (&(cairo_rectangle_int_t) {
                                                           0, 0,
                                                           tile_width, tile_height
                                                       });
          frame = gsk_gpu_renderer_get_frame (self);
          gsk_gpu_frame_render (frame,
                                g_get_monotonic_time (),
                                image,
                                color_state,
                                clip_region,
                                root,
                                &GRAPHENE_RECT_INIT (rounded_viewport->origin.x + x,
                                                     rounded_viewport->origin.y + y,
                                                     tile_width,
                                                     tile_height),
                                &texture);
          gsk_gpu_frame_sync (frame);
          gsk_gpu_frame_wait (frame);

          g_assert (texture);
          gdk_memory_layout_init_sublayout (&sub,
                                            &layout,
                                            &(cairo_rectangle_int_t) {
                                                x,
                                                y,
                                                tile_width,
                                                tile_height
                                            });
          gdk_texture_do_download (texture, data, &sub, color_state);

          g_object_unref (texture);
          g_clear_object (&image);

          /* Let's GC like a madman, we draw oversized stuff and don't want to OOM */
          gsk_gpu_device_maybe_gc (priv->device);
          gsk_gpu_renderer_make_current (self);
        }
    }

  bytes = g_bytes_new_take (data, layout.size);
  texture = gdk_memory_texture_new_from_layout (bytes, &layout, color_state, NULL, NULL);
  g_bytes_unref (bytes);
  return texture;
}

static GdkTexture *
gsk_gpu_renderer_render_texture (GskRenderer           *renderer,
                                 GskRenderNode         *root,
                                 const graphene_rect_t *viewport)
{
  GskGpuRenderer *self = GSK_GPU_RENDERER (renderer);
  GskGpuRendererPrivate *priv = gsk_gpu_renderer_get_instance_private (self);
  GskGpuFrame *frame;
  GskGpuImage *image;
  GdkTexture *texture;
  graphene_rect_t rounded_viewport;
  GdkColorState *color_state;
  cairo_region_t *clip_region;

  gsk_gpu_device_maybe_gc (priv->device);

  gsk_gpu_renderer_make_current (self);

  rounded_viewport = GRAPHENE_RECT_INIT (viewport->origin.x,
                                         viewport->origin.y,
                                         ceil (viewport->size.width),
                                         ceil (viewport->size.height));
  image = gsk_gpu_device_create_download_image (priv->device,
                                                gsk_render_node_get_preferred_depth (root),
                                                rounded_viewport.size.width,
                                                rounded_viewport.size.height);

  if (image == NULL)
    return gsk_gpu_renderer_fallback_render_texture (self, root, &rounded_viewport);

  if (gsk_gpu_image_get_conversion (image) == GSK_GPU_CONVERSION_SRGB)
    color_state = GDK_COLOR_STATE_SRGB_LINEAR;
  else
    color_state = GDK_COLOR_STATE_SRGB;

  frame = gsk_gpu_renderer_get_frame (self);

  clip_region = cairo_region_create_rectangle (&(cairo_rectangle_int_t) {
                                                   0, 0,
                                                   gsk_gpu_image_get_width (image),
                                                   gsk_gpu_image_get_height (image)
                                               });

  texture = NULL;
  gsk_gpu_frame_render (frame,
                        g_get_monotonic_time (),
                        image,
                        color_state,
                        clip_region,
                        root,
                        &rounded_viewport,
                        &texture);
  gsk_gpu_frame_sync (frame);

  gsk_gpu_frame_wait (frame);
  g_object_unref (image);

  gsk_gpu_device_queue_gc (priv->device);

  /* check that callback setting texture was actually called, as its technically async */
  g_assert (texture);

  return texture;
}

static void
gsk_gpu_renderer_render (GskRenderer          *renderer,
                         GskRenderNode        *root,
                         const cairo_region_t *region)
{
  GskGpuRenderer *self = GSK_GPU_RENDERER (renderer);
  GskGpuRendererPrivate *priv = gsk_gpu_renderer_get_instance_private (self);
  GskGpuFrame *frame;
  GskGpuImage *backbuffer;
  cairo_region_t *render_region;
  graphene_rect_t opaque_tmp;
  const graphene_rect_t *opaque;
  double scale;
  GdkMemoryDepth depth;

  if (cairo_region_is_empty (region))
    {
      gdk_draw_context_empty_frame (priv->context);
      return;
    }

  gsk_gpu_device_maybe_gc (priv->device);

  gsk_gpu_renderer_make_current (self);

  depth = gsk_render_node_get_preferred_depth (root);
  frame = gsk_gpu_renderer_get_frame (self);
  scale = gdk_surface_get_scale (gdk_draw_context_get_surface (priv->context));

  if (gsk_render_node_get_opaque_rect (root, &opaque_tmp))
    opaque = &opaque_tmp;
  else
    opaque = NULL;
  gsk_gpu_frame_begin (frame, priv->context, depth, region, opaque);

  backbuffer = GSK_GPU_RENDERER_GET_CLASS (self)->get_backbuffer (self);

  render_region = cairo_region_copy (gdk_draw_context_get_render_region (priv->context));

  gsk_gpu_frame_render (frame,
                        g_get_monotonic_time (),
                        backbuffer,
                        gdk_draw_context_get_color_state (priv->context),
                        render_region,
                        root,
                        &GRAPHENE_RECT_INIT (
                          0, 0,
                          gsk_gpu_image_get_width (backbuffer) / scale,
                          gsk_gpu_image_get_height (backbuffer) / scale
                        ),
                        NULL);

  gsk_gpu_frame_end (frame, priv->context);

  g_object_unref (backbuffer);

  gsk_gpu_device_queue_gc (priv->device);
}

static void
gsk_gpu_renderer_class_init (GskGpuRendererClass *klass)
{
  GskRendererClass *renderer_class = GSK_RENDERER_CLASS (klass);

  renderer_class->supports_offload = TRUE;

  renderer_class->realize = gsk_gpu_renderer_realize;
  renderer_class->unrealize = gsk_gpu_renderer_unrealize;
  renderer_class->render = gsk_gpu_renderer_render;
  renderer_class->render_texture = gsk_gpu_renderer_render_texture;

  gsk_ensure_resources ();

  klass->optimizations = -1;
  klass->optimizations &= ~gdk_parse_debug_var ("GSK_GPU_DISABLE",
      "GSK_GPU_DISABLE can be set to of values which cause GSK to disable\n"
      "certain optimizations in the \'ngl\' and \'vulkan\' renderers.\n",
      gsk_gpu_optimization_keys,
      G_N_ELEMENTS (gsk_gpu_optimization_keys));
}

static void
gsk_gpu_renderer_init (GskGpuRenderer *self)
{
  GskGpuRendererPrivate *priv = gsk_gpu_renderer_get_instance_private (self);

  priv->optimizations = GSK_GPU_RENDERER_GET_CLASS (self)->optimizations;
}

GdkDrawContext *
gsk_gpu_renderer_get_context (GskGpuRenderer *self)
{
  GskGpuRendererPrivate *priv = gsk_gpu_renderer_get_instance_private (self);

  return priv->context;
}

GskGpuDevice *
gsk_gpu_renderer_get_device (GskGpuRenderer *self)
{
  GskGpuRendererPrivate *priv = gsk_gpu_renderer_get_instance_private (self);

  return priv->device;
}
