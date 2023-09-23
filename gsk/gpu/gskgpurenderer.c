#include "config.h"

#include "gskgpurendererprivate.h"

#include "gskdebugprivate.h"
#include "gskgpudeviceprivate.h"
#include "gskgpuframeprivate.h"
#include "gskprivate.h"
#include "gskrendererprivate.h"
#include "gskrendernodeprivate.h"
#include "gskgpuimageprivate.h"

#include "gdk/gdkdisplayprivate.h"
#include "gdk/gdkdebugprivate.h"
#include "gdk/gdkdrawcontextprivate.h"
#include "gdk/gdkprofilerprivate.h"
#include "gdk/gdktextureprivate.h"
#include "gdk/gdkdrawcontextprivate.h"

#include <graphene.h>

#define GSK_GPU_MAX_FRAMES 4

static const GdkDebugKey gsk_gpu_optimization_keys[] = {
  { "uber", GSK_GPU_OPTIMIZE_UBER, "Don't use the uber shader" },
  { "clear", GSK_GPU_OPTIMIZE_CLEAR, "Don't vkCmdClearAttachment()/glClear() instead of shaders" },
};

typedef struct _GskGpuRendererPrivate GskGpuRendererPrivate;

struct _GskGpuRendererPrivate
{
  GskGpuDevice *device;
  GdkDrawContext *context;

  GskGpuFrame *frames[GSK_GPU_MAX_FRAMES];
};

G_DEFINE_TYPE_WITH_PRIVATE (GskGpuRenderer, gsk_gpu_renderer, GSK_TYPE_RENDERER)

static void
gsk_gpu_renderer_make_current (GskGpuRenderer *self)
{
  GSK_GPU_RENDERER_GET_CLASS (self)->make_current (self);
}

static cairo_region_t *
get_render_region (GskGpuRenderer *self)
{
  GskGpuRendererPrivate *priv = gsk_gpu_renderer_get_instance_private (self);
  const cairo_region_t *damage;
  cairo_region_t *scaled_damage;
  GdkSurface *surface;
  double scale;

  surface = gdk_draw_context_get_surface (priv->context);
  scale = gdk_surface_get_scale (surface);

  damage = gdk_draw_context_get_frame_region (priv->context);
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

  return scaled_damage;
}

static GskGpuFrame *
gsk_gpu_renderer_create_frame (GskGpuRenderer *self)
{
  GskGpuRendererPrivate *priv = gsk_gpu_renderer_get_instance_private (self);
  GskGpuRendererClass *klass = GSK_GPU_RENDERER_GET_CLASS (self);
  GskGpuFrame *result;

  result = g_object_new (klass->frame_type, NULL);

  gsk_gpu_frame_setup (result, self, priv->device, klass->optimizations);

  return result;
}

static GskGpuFrame *
gsk_gpu_renderer_get_frame (GskGpuRenderer *self)
{
  GskGpuRendererPrivate *priv = gsk_gpu_renderer_get_instance_private (self);
  guint i;

  while (TRUE)
    {
      for (i = 0; i < G_N_ELEMENTS (priv->frames); i++)
        {
          if (priv->frames[i] == NULL)
            {
              priv->frames[i] = gsk_gpu_renderer_create_frame (self);
              return priv->frames[i];
            }

          if (!gsk_gpu_frame_is_busy (priv->frames[i]))
            return priv->frames[i];
        }

      GSK_GPU_RENDERER_GET_CLASS (self)->wait (self, priv->frames, GSK_GPU_MAX_FRAMES);
    }
}

static gboolean
gsk_gpu_renderer_realize (GskRenderer  *renderer,
                          GdkSurface   *surface,
                          GError      **error)
{
  GskGpuRenderer *self = GSK_GPU_RENDERER (renderer);
  GskGpuRendererPrivate *priv = gsk_gpu_renderer_get_instance_private (self);
  GdkDisplay *display;

  if (surface)
    display = gdk_surface_get_display (surface);
  else
    display = gdk_display_get_default ();

  priv->device = GSK_GPU_RENDERER_GET_CLASS (self)->get_device (display, error);
  if (priv->device == NULL)
    return FALSE;

  priv->context = GSK_GPU_RENDERER_GET_CLASS (self)->create_context (self, display, surface, error);
  if (priv->context == NULL)
    {
      g_clear_object (&priv->device);
      return FALSE;
    }

  return TRUE;
}

static void
gsk_gpu_renderer_unrealize (GskRenderer *renderer)
{
  GskGpuRenderer *self = GSK_GPU_RENDERER (renderer);
  GskGpuRendererPrivate *priv = gsk_gpu_renderer_get_instance_private (self);
  gsize i, j;

  gsk_gpu_renderer_make_current (self);

  while (TRUE)
    {
      for (i = 0, j = 0; i < G_N_ELEMENTS (priv->frames); i++)
        {
          if (priv->frames[i] == NULL)
            break;
          if (gsk_gpu_frame_is_busy (priv->frames[i]))
            {
              if (i > j)
                {
                  priv->frames[j] = priv->frames[i];
                  priv->frames[i] = NULL;
                }
              j++;
              continue;
            }
          g_clear_object (&priv->frames[i]);
        }
      if (j == 0)
        break;
      GSK_GPU_RENDERER_GET_CLASS (self)->wait (self, priv->frames, j);
    }

  g_clear_object (&priv->context);
  g_clear_object (&priv->device);
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

  gsk_gpu_renderer_make_current (self);

  frame = gsk_gpu_renderer_create_frame (self);

  rounded_viewport = GRAPHENE_RECT_INIT (viewport->origin.x,
                                         viewport->origin.y,
                                         ceil (viewport->size.width),
                                         ceil (viewport->size.height));
  image = gsk_gpu_device_create_offscreen_image (priv->device,
                                                 gsk_render_node_get_preferred_depth (root),
                                                 rounded_viewport.size.width,
                                                 rounded_viewport.size.height);

  texture = NULL;
  gsk_gpu_frame_render (frame,
                        g_get_monotonic_time(),
                        image,
                        NULL,
                        root,
                        &rounded_viewport,
                        &texture);

  g_object_unref (frame);
  g_object_unref (image);

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
  GdkSurface *surface;

  if (cairo_region_is_empty (region))
    {
      gdk_draw_context_empty_frame (priv->context);
      return;
    }

  gdk_draw_context_begin_frame_full (priv->context,
                                     gsk_render_node_get_preferred_depth (root),
                                     region);

  gsk_gpu_renderer_make_current (self);

  backbuffer = GSK_GPU_RENDERER_GET_CLASS (self)->get_backbuffer (self);

  frame = gsk_gpu_renderer_get_frame (self);
  render_region = get_render_region (self);
  surface = gdk_draw_context_get_surface (priv->context);

  gsk_gpu_frame_render (frame,
                        g_get_monotonic_time(),
                        backbuffer,
                        render_region,
                        root,
                        &GRAPHENE_RECT_INIT (
                          0, 0,
                          gdk_surface_get_width (surface),
                          gdk_surface_get_height (surface)
                        ),
                        NULL);

  gdk_draw_context_end_frame (priv->context);

  g_clear_pointer (&render_region, cairo_region_destroy);
}

static void
gsk_gpu_renderer_class_init (GskGpuRendererClass *klass)
{
  GskRendererClass *renderer_class = GSK_RENDERER_CLASS (klass);

  renderer_class->realize = gsk_gpu_renderer_realize;
  renderer_class->unrealize = gsk_gpu_renderer_unrealize;
  renderer_class->render = gsk_gpu_renderer_render;
  renderer_class->render_texture = gsk_gpu_renderer_render_texture;

  gsk_ensure_resources ();

  klass->optimizations = -1;
  klass->optimizations &= ~gdk_parse_debug_var ("GSK_GPU_SKIP",
                                                gsk_gpu_optimization_keys,
                                                G_N_ELEMENTS (gsk_gpu_optimization_keys));
}

static void
gsk_gpu_renderer_init (GskGpuRenderer *self)
{
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

