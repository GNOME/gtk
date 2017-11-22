#include "config.h"

#include "gskbroadwayrendererprivate.h"
#include "broadway/gdkprivate-broadway.h"

#include "gskdebugprivate.h"
#include "gskrendererprivate.h"
#include "gskrendernodeprivate.h"
#include "gdk/gdktextureprivate.h"


struct _GskBroadwayRenderer
{
  GskRenderer parent_instance;

};

struct _GskBroadwayRendererClass
{
  GskRendererClass parent_class;
};

G_DEFINE_TYPE (GskBroadwayRenderer, gsk_broadway_renderer, GSK_TYPE_RENDERER)

static gboolean
gsk_broadway_renderer_realize (GskRenderer  *self,
                               GdkWindow    *window,
                               GError      **error)
{
  return TRUE;
}

static void
gsk_broadway_renderer_unrealize (GskRenderer *self)
{

}

static GdkDrawingContext *
gsk_broadway_renderer_begin_draw_frame (GskRenderer          *renderer,
                                        const cairo_region_t *update_area)
{
  cairo_region_t *region;
  GdkDrawingContext *result;
  cairo_rectangle_int_t whole_window;
  GdkWindow *window;

  window = gsk_renderer_get_window (renderer);
  whole_window = (cairo_rectangle_int_t) {
    0, 0,
    gdk_window_get_width (window),
    gdk_window_get_height (window)
  };
  region = cairo_region_create_rectangle (&whole_window);
  result = gdk_window_begin_draw_frame (window, NULL, region);
  cairo_region_destroy (region);

  return result;
}


static GdkTexture *
gsk_broadway_renderer_render_texture (GskRenderer           *self,
                                      GskRenderNode         *root,
                                      const graphene_rect_t *viewport)
{
  GdkTexture *texture;
  cairo_surface_t *surface;
  cairo_t *cr;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, ceil (viewport->size.width), ceil (viewport->size.height));
  cr = cairo_create (surface);

  cairo_translate (cr, - viewport->origin.x, - viewport->origin.y);

  gsk_render_node_draw (root, cr);

  cairo_destroy (cr);

  texture = gdk_texture_new_for_surface (surface);
  cairo_surface_destroy (surface);

  return texture;
}

static void
add_uint32 (GArray *nodes, guint32 v)
{
  g_array_append_val (nodes, v);
}

static guint32
rgba_to_uint32 (const GdkRGBA *rgba)
{
  return
    ((guint32)(0.5 + CLAMP (rgba->alpha, 0., 1.) * 255.) << 24) |
    ((guint32)(0.5 + CLAMP (rgba->red, 0., 1.) * 255.) << 16) |
    ((guint32)(0.5 + CLAMP (rgba->green, 0., 1.) * 255.) << 8) |
    ((guint32)(0.5 + CLAMP (rgba->blue, 0., 1.) * 255.) << 0);
}


static void
add_rgba (GArray *nodes, const GdkRGBA *rgba)
{
  guint32 c = rgba_to_uint32 (rgba);
  g_array_append_val (nodes, c);
}

static void
add_float (GArray *nodes, float f)
{
  gint32 i = (gint32) (f * 256.0f);
  guint u = (guint32) i;
  g_array_append_val (nodes, u);
}

static void
add_point (GArray *nodes, const graphene_point_t *point)
{
  add_float (nodes, point->x);
  add_float (nodes, point->y);
}

static void
add_size (GArray *nodes, const graphene_size_t *size)
{
  add_float (nodes, size->width);
  add_float (nodes, size->height);
}

static void
add_rect (GArray *nodes, const graphene_rect_t *rect)
{
  add_point (nodes, &rect->origin);
  add_size (nodes, &rect->size);
}

static void
add_rounded_rect (GArray *nodes, const GskRoundedRect *rrect)
{
  int i;
  add_rect (nodes, &rrect->bounds);
  for (i = 0; i < 4; i++)
    add_size (nodes, &rrect->corner[i]);
}

static void
add_color_stop (GArray *nodes, const GskColorStop *stop)
{
  add_float (nodes, stop->offset);
  add_rgba (nodes, &stop->color);
}

static void
gsk_broadway_renderer_add_node (GskRenderer *self,
                                GArray *nodes,
                                GPtrArray *node_textures,
                                GskRenderNode *node)
{
  GdkDisplay *display = gsk_renderer_get_display (self);
  int x = floorf (node->bounds.origin.x);
  int y = floorf (node->bounds.origin.y);
  int width = ceil (node->bounds.origin.x + node->bounds.size.width) - x;
  int height = ceil (node->bounds.origin.y + node->bounds.size.height) - y;

  switch (gsk_render_node_get_node_type (node))
    {
    case GSK_NOT_A_RENDER_NODE:
      g_assert_not_reached ();
      return;
    case GSK_CONTAINER_NODE:
      {
        guint i;

        add_uint32 (nodes, BROADWAY_NODE_CONTAINER);
        add_uint32 (nodes, gsk_container_node_get_n_children (node));

        for (i = 0; i < gsk_container_node_get_n_children (node); i++)
          gsk_broadway_renderer_add_node (self, nodes, node_textures,
                                          gsk_container_node_get_child (node, i));
      }
      return;

    case GSK_COLOR_NODE:
      {
        add_uint32 (nodes, BROADWAY_NODE_COLOR);
        add_rect (nodes, &node->bounds);
        add_rgba (nodes, gsk_color_node_peek_color (node));
      }
      return;

    case GSK_BORDER_NODE:
      {
        int i;
        add_uint32 (nodes, BROADWAY_NODE_BORDER);
        add_rounded_rect (nodes, gsk_border_node_peek_outline (node));
        for (i = 0; i < 4; i++)
          add_float (nodes, gsk_border_node_peek_widths (node)[i]);
        for (i = 0; i < 4; i++)
          add_rgba (nodes, &gsk_border_node_peek_colors (node)[i]);
      }
      return;

    case GSK_OUTSET_SHADOW_NODE:
      {
        add_uint32 (nodes, BROADWAY_NODE_OUTSET_SHADOW);
        add_rounded_rect (nodes, gsk_outset_shadow_node_peek_outline (node));
        add_rgba (nodes, gsk_outset_shadow_node_peek_color (node));
        add_float (nodes, gsk_outset_shadow_node_get_dx (node));
        add_float (nodes, gsk_outset_shadow_node_get_dy (node));
        add_float (nodes, gsk_outset_shadow_node_get_spread (node));
        add_float (nodes, gsk_outset_shadow_node_get_blur_radius (node));
      }
      return;

    case GSK_INSET_SHADOW_NODE:
      {
        add_uint32 (nodes, BROADWAY_NODE_INSET_SHADOW);
        add_rounded_rect (nodes, gsk_inset_shadow_node_peek_outline (node));
        add_rgba (nodes, gsk_inset_shadow_node_peek_color (node));
        add_float (nodes, gsk_inset_shadow_node_get_dx (node));
        add_float (nodes, gsk_inset_shadow_node_get_dy (node));
        add_float (nodes, gsk_inset_shadow_node_get_spread (node));
        add_float (nodes, gsk_inset_shadow_node_get_blur_radius (node));
      }
      return;

    case GSK_ROUNDED_CLIP_NODE:
      {
        add_uint32 (nodes, BROADWAY_NODE_ROUNDED_CLIP);
        add_rounded_rect (nodes, gsk_rounded_clip_node_peek_clip (node));
        gsk_broadway_renderer_add_node (self, nodes, node_textures,
                                        gsk_rounded_clip_node_get_child (node));
      }
      return;

    case GSK_LINEAR_GRADIENT_NODE:
      {
        guint i, n;

        add_uint32 (nodes, BROADWAY_NODE_LINEAR_GRADIENT);
        add_rect (nodes, &node->bounds);
        add_point (nodes, gsk_linear_gradient_node_peek_start (node));
        add_point (nodes, gsk_linear_gradient_node_peek_end (node));
        n = gsk_linear_gradient_node_get_n_color_stops (node);
        add_uint32 (nodes, n);
        for (i = 0; i < n; i++)
          add_color_stop (nodes, &gsk_linear_gradient_node_peek_color_stops (node)[i]);
      }
      return;

    default:
      {
        cairo_surface_t *surface;
        GdkTexture *texture;
        guint32 texture_id;
        cairo_t *cr;

        surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
        cr = cairo_create (surface);
        cairo_translate (cr, -x, -y);
        gsk_render_node_draw (node, cr);
        cairo_destroy (cr);

        texture = gdk_texture_new_for_surface (surface);
        g_ptr_array_add (node_textures, texture); /* Transfers ownership to node_textures */
        texture_id = gdk_broadway_display_ensure_texture (display, texture);
        add_uint32 (nodes, BROADWAY_NODE_TEXTURE);
        add_uint32 (nodes, x);
        add_uint32 (nodes, y);
        add_uint32 (nodes, width);
        add_uint32 (nodes, height);
        add_uint32 (nodes, texture_id);
      }
      return;
    }
}

static void
gsk_broadway_renderer_render (GskRenderer   *self,
                              GskRenderNode *root)
{
  GdkWindow *window = gsk_renderer_get_window (self);
  GArray *nodes = g_array_new (FALSE, FALSE, sizeof(guint32));
  GPtrArray *node_textures = g_ptr_array_new_with_free_func (g_object_unref);

  gsk_broadway_renderer_add_node (self, nodes, node_textures, root);
  gdk_broadway_window_set_nodes (window, nodes, node_textures);
  g_array_unref (nodes);
  g_ptr_array_unref (node_textures);
}

static void
gsk_broadway_renderer_class_init (GskBroadwayRendererClass *klass)
{
  GskRendererClass *renderer_class = GSK_RENDERER_CLASS (klass);

  renderer_class->begin_draw_frame = gsk_broadway_renderer_begin_draw_frame;
  renderer_class->realize = gsk_broadway_renderer_realize;
  renderer_class->unrealize = gsk_broadway_renderer_unrealize;
  renderer_class->render = gsk_broadway_renderer_render;
  renderer_class->render_texture = gsk_broadway_renderer_render_texture;
}

static void
gsk_broadway_renderer_init (GskBroadwayRenderer *self)
{
}
