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
  GdkBroadwayDrawContext *draw_context;
};

struct _GskBroadwayRendererClass
{
  GskRendererClass parent_class;
};

G_DEFINE_TYPE (GskBroadwayRenderer, gsk_broadway_renderer, GSK_TYPE_RENDERER)

static gboolean
gsk_broadway_renderer_realize (GskRenderer  *renderer,
                               GdkSurface   *surface,
                               GError      **error)
{
  GskBroadwayRenderer *self = GSK_BROADWAY_RENDERER (renderer);

  if (!GDK_IS_BROADWAY_SURFACE (surface))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                   "Broadway renderer only works for broadway surfaces");
      return FALSE;
    }

  self->draw_context = gdk_broadway_draw_context_context (surface);

  return TRUE;
}

static void
gsk_broadway_renderer_unrealize (GskRenderer *renderer)
{
  GskBroadwayRenderer *self = GSK_BROADWAY_RENDERER (renderer);
  g_clear_object (&self->draw_context);
}

static GdkTexture *
gsk_broadway_renderer_render_texture (GskRenderer           *renderer,
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
add_point (GArray *nodes, const graphene_point_t *point, float offset_x, float offset_y)
{
  add_float (nodes, point->x - offset_x);
  add_float (nodes, point->y - offset_y);
}

static void
add_size (GArray *nodes, const graphene_size_t *size)
{
  add_float (nodes, size->width);
  add_float (nodes, size->height);
}

static void
add_rect (GArray *nodes, const graphene_rect_t *rect, float offset_x, float offset_y)
{
  add_point (nodes, &rect->origin, offset_x, offset_y);
  add_size (nodes, &rect->size);
}

static void
add_rounded_rect (GArray *nodes, const GskRoundedRect *rrect, float offset_x, float offset_y)
{
  int i;
  add_rect (nodes, &rrect->bounds, offset_x, offset_y);
  for (i = 0; i < 4; i++)
    add_size (nodes, &rrect->corner[i]);
}

static void
add_color_stop (GArray *nodes, const GskColorStop *stop)
{
  add_float (nodes, stop->offset);
  add_rgba (nodes, &stop->color);
}

static gboolean
float_is_int32 (float f)
{
  gint32 i = (gint32)f;
  float f2 = (float)i;
  return f2 == f;
}

static GHashTable *gsk_broadway_node_cache;

typedef struct {
  GdkTexture *texture;
  GskRenderNode *node;
  float off_x;
  float off_y;
} NodeCacheElement;

static void
node_cache_element_free (NodeCacheElement *element)
{
  gsk_render_node_unref (element->node);
  g_free (element);
}

static guint
glyph_info_hash (const PangoGlyphInfo *info)
{
  return info->glyph ^
    info->geometry.width << 6 ^
    info->geometry.x_offset << 12 ^
    info->geometry.y_offset << 18 ^
    info->attr.is_cluster_start << 30;
}

static gboolean
glyph_info_equal (const PangoGlyphInfo *a,
                  const PangoGlyphInfo *b)
{
  return
    a->glyph == b->glyph &&
    a->geometry.width == b->geometry.width &&
    a->geometry.x_offset == b->geometry.x_offset &&
    a->geometry.y_offset == b->geometry.y_offset &&
    a->attr.is_cluster_start == b->attr.is_cluster_start;
 }

static guint
hash_matrix (const graphene_matrix_t *matrix)
{
  float m[16];
  guint h = 0;
  int i;

  graphene_matrix_to_float (matrix, m);
  for (i = 0; i < 16; i++)
    h ^= (guint) m[i];

  return h;
}

static gboolean
matrix_equal (const graphene_matrix_t *a,
              const graphene_matrix_t *b)
{
  float ma[16];
  float mb[16];
  int i;

  graphene_matrix_to_float (a, ma);
  graphene_matrix_to_float (b, mb);
  for (i = 0; i < 16; i++)
    {
      if (ma[i] != mb[i])
        return FALSE;
    }

  return TRUE;
}

static guint
hash_vec4 (const graphene_vec4_t *vec4)
{
  float v[4];
  guint h = 0;
  int i;

  graphene_vec4_to_float (vec4, v);
  for (i = 0; i < 4; i++)
    h ^= (guint) v[i];

  return h;
}

static gboolean
vec4_equal (const graphene_vec4_t *a,
            const graphene_vec4_t *b)
{
  float va[4];
  float vb[4];
  int i;

  graphene_vec4_to_float (a, va);
  graphene_vec4_to_float (b, vb);
  for (i = 0; i < 4; i++)
    {
      if (va[i] != vb[i])
        return FALSE;
    }

  return TRUE;
}

static guint
node_cache_hash (GskRenderNode *node)
{
  GskRenderNodeType type;
  guint h;

  type = gsk_render_node_get_node_type (node);
  h = type << 28;
  if (type == GSK_TEXT_NODE &&
      float_is_int32 (gsk_text_node_get_x (node)) &&
      float_is_int32 (gsk_text_node_get_y (node)))
    {
      guint i;
      const PangoFont *font = gsk_text_node_peek_font (node);
      guint n_glyphs = gsk_text_node_get_num_glyphs (node);
      const PangoGlyphInfo *infos = gsk_text_node_peek_glyphs (node);
      const GdkRGBA *color = gsk_text_node_peek_color (node);

      h ^= g_direct_hash (font) ^ n_glyphs << 16 ^ gdk_rgba_hash (color);
      for (i = 0; i < n_glyphs; i++)
        h ^= glyph_info_hash (&infos[i]);

      return h;
    }

  if (type == GSK_COLOR_MATRIX_NODE &&
      gsk_render_node_get_node_type (gsk_color_matrix_node_get_child (node)) == GSK_TEXTURE_NODE)
    {
      const graphene_matrix_t *matrix = gsk_color_matrix_node_peek_color_matrix (node);
      const graphene_vec4_t *offset = gsk_color_matrix_node_peek_color_offset (node);
      GskRenderNode *child = gsk_color_matrix_node_get_child (node);
      GdkTexture *texture = gsk_texture_node_get_texture (child);

      h ^= g_direct_hash (texture) ^ hash_matrix (matrix) ^ hash_vec4 (offset);

      return h;
    }

  return 0;
}

static gboolean
node_cache_equal (GskRenderNode *a,
                  GskRenderNode *b)
{
  GskRenderNodeType type;

  type = gsk_render_node_get_node_type (a);
  if (type != gsk_render_node_get_node_type (b))
    return FALSE;

  if (type == GSK_TEXT_NODE &&
      float_is_int32 (gsk_text_node_get_x (a)) &&
      float_is_int32 (gsk_text_node_get_y (a)) &&
      float_is_int32 (gsk_text_node_get_x (b)) &&
      float_is_int32 (gsk_text_node_get_y (b)))
    {
      const PangoFont *a_font = gsk_text_node_peek_font (a);
      guint a_n_glyphs = gsk_text_node_get_num_glyphs (a);
      const PangoGlyphInfo *a_infos = gsk_text_node_peek_glyphs (a);
      const GdkRGBA *a_color = gsk_text_node_peek_color (a);
      const PangoFont *b_font = gsk_text_node_peek_font (b);
      guint b_n_glyphs = gsk_text_node_get_num_glyphs (b);
      const PangoGlyphInfo *b_infos = gsk_text_node_peek_glyphs (b);
      const GdkRGBA *b_color = gsk_text_node_peek_color (a);
      guint i;

      if (a_font != b_font)
        return FALSE;

      if (a_n_glyphs != b_n_glyphs)
        return FALSE;

      for (i = 0; i < a_n_glyphs; i++)
        {
          if (!glyph_info_equal (&a_infos[i], &b_infos[i]))
            return FALSE;
        }

      if (!gdk_rgba_equal (a_color, b_color))
        return FALSE;

      return TRUE;
    }

  if (type == GSK_COLOR_MATRIX_NODE &&
      gsk_render_node_get_node_type (gsk_color_matrix_node_get_child (a)) == GSK_TEXTURE_NODE &&
      gsk_render_node_get_node_type (gsk_color_matrix_node_get_child (b)) == GSK_TEXTURE_NODE)
    {
      const graphene_matrix_t *a_matrix = gsk_color_matrix_node_peek_color_matrix (a);
      const graphene_vec4_t *a_offset = gsk_color_matrix_node_peek_color_offset (a);
      GskRenderNode *a_child = gsk_color_matrix_node_get_child (a);
      GdkTexture *a_texture = gsk_texture_node_get_texture (a_child);
      const graphene_matrix_t *b_matrix = gsk_color_matrix_node_peek_color_matrix (b);
      const graphene_vec4_t *b_offset = gsk_color_matrix_node_peek_color_offset (b);
      GskRenderNode *b_child = gsk_color_matrix_node_get_child (b);
      GdkTexture *b_texture = gsk_texture_node_get_texture (b_child);

      if (a_texture != b_texture)
        return FALSE;

      if (!matrix_equal (a_matrix, b_matrix))
        return FALSE;

      if (!vec4_equal (a_offset, b_offset))
        return FALSE;

      return TRUE;
    }

  return FALSE;
}

static GdkTexture *
node_cache_lookup (GskRenderNode *node,
                   float *off_x, float *off_y)
{
  NodeCacheElement *hit;

  if (gsk_broadway_node_cache == NULL)
    gsk_broadway_node_cache = g_hash_table_new_full ((GHashFunc)node_cache_hash,
                                                     (GEqualFunc)node_cache_equal,
                                                     NULL,
                                                     (GDestroyNotify)node_cache_element_free);

  hit = g_hash_table_lookup (gsk_broadway_node_cache, node);
  if (hit)
    {
      *off_x = hit->off_x;
      *off_y = hit->off_y;
      return g_object_ref (hit->texture);
    }

  return NULL;
}

static void
cached_texture_gone (gpointer data,
                     GObject *where_the_object_was)
{
  NodeCacheElement *element = data;
  g_hash_table_remove (gsk_broadway_node_cache, element->node);
}

static void
node_cache_store (GskRenderNode *node,
                  GdkTexture *texture,
                  float off_x,
                  float off_y)
{
  GskRenderNodeType type;

  type = gsk_render_node_get_node_type (node);
  if ((type == GSK_TEXT_NODE &&
       float_is_int32 (gsk_text_node_get_x (node)) &&
       float_is_int32 (gsk_text_node_get_y (node))) ||
      (type == GSK_COLOR_MATRIX_NODE &&
       gsk_render_node_get_node_type (gsk_color_matrix_node_get_child (node)) == GSK_TEXTURE_NODE))
    {
      NodeCacheElement *element = g_new0 (NodeCacheElement, 1);
      element->texture = texture;
      element->node = gsk_render_node_ref (node);
      element->off_x = off_x;
      element->off_y = off_y;
      g_object_weak_ref (G_OBJECT (texture), cached_texture_gone, element);
      g_hash_table_insert (gsk_broadway_node_cache, element->node, element);

      return;
    }
}

static GdkTexture *
node_texture_fallback (GskRenderNode *node,
                       float *off_x,
                       float *off_y)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  int x = floorf (node->bounds.origin.x);
  int y = floorf (node->bounds.origin.y);
  int width = ceil (node->bounds.origin.x + node->bounds.size.width) - x;
  int height = ceil (node->bounds.origin.y + node->bounds.size.height) - y;
  GdkTexture *texture;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  cr = cairo_create (surface);
  cairo_translate (cr, -x, -y);
  gsk_render_node_draw (node, cr);
  cairo_destroy (cr);

  texture = gdk_texture_new_for_surface (surface);
  *off_x =  x - node->bounds.origin.x;
  *off_y =  y - node->bounds.origin.y;

  return texture;
}

/* Note: This tracks the offset so that we can convert
   the absolute coordinates of the GskRenderNodes to
   parent-relative which is what the dom uses, and
   which is good for re-using subtrees. */
static void
gsk_broadway_renderer_add_node (GskRenderer *renderer,
                                GArray *nodes,
                                GPtrArray *node_textures,
                                GskRenderNode *node,
                                float offset_x,
                                float offset_y)
{
  GdkDisplay *display = gsk_renderer_get_display (renderer);

  switch (gsk_render_node_get_node_type (node))
    {
    case GSK_NOT_A_RENDER_NODE:
      g_assert_not_reached ();
      return;

    /* Leaf nodes */

    case GSK_TEXTURE_NODE:
      {
        GdkTexture *texture = gsk_texture_node_get_texture (node);
        guint32 texture_id;

        g_ptr_array_add (node_textures, g_object_ref (texture)); /* Transfers ownership to node_textures */
        texture_id = gdk_broadway_display_ensure_texture (display, texture);

        add_uint32 (nodes, BROADWAY_NODE_TEXTURE);
        add_rect (nodes, &node->bounds, offset_x, offset_y);
        add_uint32 (nodes, texture_id);
      }
      return;

    case GSK_CAIRO_NODE:
      {
        cairo_surface_t *surface = (cairo_surface_t *)gsk_cairo_node_peek_surface (node);
        cairo_surface_t *image_surface = NULL;
        GdkTexture *texture;
        guint32 texture_id;

        if (cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_IMAGE)
          image_surface = cairo_surface_reference (surface);
        else
          {
            cairo_t *cr;
            image_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                                        ceilf (node->bounds.size.width),
                                                        ceilf (node->bounds.size.height));
            cr = cairo_create (image_surface);
            cairo_set_source_surface (cr, surface, 0, 0);
            cairo_rectangle (cr, 0, 0, node->bounds.size.width, node->bounds.size.height);
            cairo_fill (cr);
            cairo_destroy (cr);
          }

        texture = gdk_texture_new_for_surface (image_surface);
        g_ptr_array_add (node_textures, g_object_ref (texture)); /* Transfers ownership to node_textures */
        texture_id = gdk_broadway_display_ensure_texture (display, texture);

        add_uint32 (nodes, BROADWAY_NODE_TEXTURE);
        add_rect (nodes, &node->bounds, offset_x, offset_y);
        add_uint32 (nodes, texture_id);

        cairo_surface_destroy (image_surface);
      }
      return;

    case GSK_COLOR_NODE:
      {
        add_uint32 (nodes, BROADWAY_NODE_COLOR);
        add_rect (nodes, &node->bounds, offset_x, offset_y);
        add_rgba (nodes, gsk_color_node_peek_color (node));
      }
      return;

    case GSK_BORDER_NODE:
      {
        int i;
        add_uint32 (nodes, BROADWAY_NODE_BORDER);
        add_rounded_rect (nodes, gsk_border_node_peek_outline (node), offset_x, offset_y);
        for (i = 0; i < 4; i++)
          add_float (nodes, gsk_border_node_peek_widths (node)[i]);
        for (i = 0; i < 4; i++)
          add_rgba (nodes, &gsk_border_node_peek_colors (node)[i]);
      }
      return;

    case GSK_OUTSET_SHADOW_NODE:
      {
        add_uint32 (nodes, BROADWAY_NODE_OUTSET_SHADOW);
        add_rounded_rect (nodes, gsk_outset_shadow_node_peek_outline (node), offset_x, offset_y);
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
        add_rounded_rect (nodes, gsk_inset_shadow_node_peek_outline (node), offset_x, offset_y);
        add_rgba (nodes, gsk_inset_shadow_node_peek_color (node));
        add_float (nodes, gsk_inset_shadow_node_get_dx (node));
        add_float (nodes, gsk_inset_shadow_node_get_dy (node));
        add_float (nodes, gsk_inset_shadow_node_get_spread (node));
        add_float (nodes, gsk_inset_shadow_node_get_blur_radius (node));
      }
      return;

    case GSK_LINEAR_GRADIENT_NODE:
      {
        guint i, n;

        add_uint32 (nodes, BROADWAY_NODE_LINEAR_GRADIENT);
        add_rect (nodes, &node->bounds, offset_x, offset_y);
        add_point (nodes, gsk_linear_gradient_node_peek_start (node), offset_x, offset_y);
        add_point (nodes, gsk_linear_gradient_node_peek_end (node), offset_x, offset_y);
        n = gsk_linear_gradient_node_get_n_color_stops (node);
        add_uint32 (nodes, n);
        for (i = 0; i < n; i++)
          add_color_stop (nodes, &gsk_linear_gradient_node_peek_color_stops (node)[i]);
      }
      return;

      /* Bin nodes */

    case GSK_OFFSET_NODE:
      {
        gsk_broadway_renderer_add_node (renderer, nodes, node_textures,
                                        gsk_offset_node_get_child (node),
                                        offset_x - gsk_offset_node_get_x_offset (node),
                                        offset_y - gsk_offset_node_get_y_offset (node));
      }
      return;

    case GSK_SHADOW_NODE:
      {
        gsize i, n_shadows = gsk_shadow_node_get_n_shadows (node);
        add_uint32 (nodes, BROADWAY_NODE_SHADOW);
        add_uint32 (nodes, n_shadows);
        for (i = 0; i < n_shadows; i++)
          {
            const GskShadow *shadow = gsk_shadow_node_peek_shadow (node, i);
            add_rgba (nodes, &shadow->color);
            add_float (nodes, shadow->dx);
            add_float (nodes, shadow->dy);
            add_float (nodes, shadow->radius);
          }
        gsk_broadway_renderer_add_node (renderer, nodes, node_textures,
                                        gsk_shadow_node_get_child (node),
                                        offset_x, offset_y);
      }
      return;

    case GSK_OPACITY_NODE:
      {
        add_uint32 (nodes, BROADWAY_NODE_OPACITY);
        add_float (nodes, gsk_opacity_node_get_opacity (node));
        gsk_broadway_renderer_add_node (renderer, nodes, node_textures,
                                        gsk_opacity_node_get_child (node),
                                        offset_x, offset_y);
      }
      return;

    case GSK_ROUNDED_CLIP_NODE:
      {
        const GskRoundedRect *rclip = gsk_rounded_clip_node_peek_clip (node);
        add_uint32 (nodes, BROADWAY_NODE_ROUNDED_CLIP);
        add_rounded_rect (nodes, rclip, offset_x, offset_y);
        gsk_broadway_renderer_add_node (renderer, nodes, node_textures,
                                        gsk_rounded_clip_node_get_child (node),
                                        rclip->bounds.origin.x,
                                        rclip->bounds.origin.y);
      }
      return;

    case GSK_CLIP_NODE:
      {
        const graphene_rect_t *clip = gsk_clip_node_peek_clip (node);
        add_uint32 (nodes, BROADWAY_NODE_CLIP);
        add_rect (nodes, clip, offset_x, offset_y);
        gsk_broadway_renderer_add_node (renderer, nodes, node_textures,
                                        gsk_clip_node_get_child (node),
                                        clip->origin.x,
                                        clip->origin.y);
      }
      return;

      /* Generic nodes */

    case GSK_CONTAINER_NODE:
      {
        guint i;

        add_uint32 (nodes, BROADWAY_NODE_CONTAINER);
        add_uint32 (nodes, gsk_container_node_get_n_children (node));

        for (i = 0; i < gsk_container_node_get_n_children (node); i++)
          gsk_broadway_renderer_add_node (renderer, nodes, node_textures,
                                          gsk_container_node_get_child (node, i), offset_x, offset_y);
      }
      return;

    case GSK_DEBUG_NODE:
      gsk_broadway_renderer_add_node (renderer, nodes, node_textures,
                                      gsk_debug_node_get_child (node), offset_x, offset_y);
      return;

    case GSK_COLOR_MATRIX_NODE:
    case GSK_TEXT_NODE:
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
    case GSK_TRANSFORM_NODE:
    case GSK_REPEAT_NODE:
    case GSK_BLEND_NODE:
    case GSK_CROSS_FADE_NODE:
    case GSK_BLUR_NODE:
    default:
      break; /* Fallback */
    }

  {
    GdkTexture *texture;
    guint32 texture_id;
    float t_off_x = 0, t_off_y = 0;

    texture = node_cache_lookup (node, &t_off_x, &t_off_y);

    if (!texture)
      {
        texture = node_texture_fallback (node, &t_off_x, &t_off_y);
#if 0
        g_print ("Fallback %p for %s\n", texture, node->node_class->type_name);
#endif

        node_cache_store (node, texture, t_off_x, t_off_y);
      }

    g_ptr_array_add (node_textures, texture); /* Transfers ownership to node_textures */
    texture_id = gdk_broadway_display_ensure_texture (display, texture);
    add_uint32 (nodes, BROADWAY_NODE_TEXTURE);
    add_float (nodes, node->bounds.origin.x + t_off_x - offset_x);
    add_float (nodes, node->bounds.origin.y + t_off_y - offset_y);
    add_float (nodes, gdk_texture_get_width (texture));
    add_float (nodes, gdk_texture_get_height (texture));
    add_uint32 (nodes, texture_id);
  }
}

static void
gsk_broadway_renderer_render (GskRenderer          *renderer,
                              GskRenderNode        *root,
                              const cairo_region_t *update_area)
{
  GskBroadwayRenderer *self = GSK_BROADWAY_RENDERER (renderer);

  gdk_draw_context_begin_frame (GDK_DRAW_CONTEXT (self->draw_context), update_area);
  gsk_broadway_renderer_add_node (renderer, self->draw_context->nodes, self->draw_context->node_textures, root, 0, 0);
  gdk_draw_context_end_frame (GDK_DRAW_CONTEXT (self->draw_context));
}

static void
gsk_broadway_renderer_class_init (GskBroadwayRendererClass *klass)
{
  GskRendererClass *renderer_class = GSK_RENDERER_CLASS (klass);

  renderer_class->realize = gsk_broadway_renderer_realize;
  renderer_class->unrealize = gsk_broadway_renderer_unrealize;
  renderer_class->render = gsk_broadway_renderer_render;
  renderer_class->render_texture = gsk_broadway_renderer_render_texture;
}

static void
gsk_broadway_renderer_init (GskBroadwayRenderer *self)
{
}
