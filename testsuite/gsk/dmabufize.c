#include "config.h"

#include <gtk/gtk.h>
#include "udmabuf.h"
#include "dmabufize.h"

#ifdef HAVE_DMABUF
#include <drm_fourcc.h>

static GdkTexture *
dmabufize_texture (GdkTexture *texture)
{
  guint32 fourcc = 0;
  guint32 bpp;
  gboolean premultiplied;
  gsize width;
  gsize height;
  gsize size;
  UDmabuf *udmabuf;
  GdkTextureDownloader *downloader;
  GdkDmabufTextureBuilder *builder;
  GdkTexture *texture2;

  if (strcmp (G_OBJECT_TYPE_NAME (texture), "GdkMemoryTexture") != 0)
    return g_object_ref (texture);

  switch ((int) gdk_texture_get_format (texture))
    {
    case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
      fourcc = DRM_FORMAT_ARGB8888;
      premultiplied = TRUE;
      bpp = 4;
      break;
    case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
      fourcc = DRM_FORMAT_BGRA8888;
      premultiplied = TRUE;
      bpp = 4;
      break;
    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
      fourcc = DRM_FORMAT_ABGR8888;
      premultiplied = TRUE;
      bpp = 4;
      break;
    case GDK_MEMORY_A8B8G8R8_PREMULTIPLIED:
      fourcc = DRM_FORMAT_RGBA8888;
      premultiplied = TRUE;
      bpp = 4;
      break;
    case GDK_MEMORY_B8G8R8A8:
      fourcc = DRM_FORMAT_ARGB8888;
      premultiplied = FALSE;
      bpp = 4;
      break;
    case GDK_MEMORY_A8R8G8B8:
      fourcc = DRM_FORMAT_BGRA8888;
      premultiplied = FALSE;
      bpp = 4;
      break;
    default:
      break;
    }

  if (fourcc == 0)
    return g_object_ref (texture);

  width = gdk_texture_get_width (texture);
  height = gdk_texture_get_height (texture);

  size = height * width * bpp;

  udmabuf = udmabuf_allocate (size, NULL);

  downloader = gdk_texture_downloader_new (texture);

  gdk_texture_downloader_set_format (downloader, gdk_texture_get_format (texture));
  gdk_texture_downloader_set_color_state (downloader, gdk_texture_get_color_state (texture));

  gdk_texture_downloader_download_into (downloader, (guchar *)udmabuf->data, width * bpp);

  gdk_texture_downloader_free (downloader);

  builder = gdk_dmabuf_texture_builder_new ();
  gdk_dmabuf_texture_builder_set_display (builder, gdk_display_get_default ());
  gdk_dmabuf_texture_builder_set_width (builder, width);
  gdk_dmabuf_texture_builder_set_height (builder, height);
  gdk_dmabuf_texture_builder_set_fourcc (builder, fourcc);
  gdk_dmabuf_texture_builder_set_modifier (builder, 0);
  gdk_dmabuf_texture_builder_set_premultiplied (builder, premultiplied);
  gdk_dmabuf_texture_builder_set_n_planes (builder, 1);
  gdk_dmabuf_texture_builder_set_fd (builder, 0, udmabuf->dmabuf_fd);
  gdk_dmabuf_texture_builder_set_stride (builder, 0, width * bpp);
  gdk_dmabuf_texture_builder_set_offset (builder, 0, 0);
  gdk_dmabuf_texture_builder_set_color_state (builder, gdk_texture_get_color_state (texture));
  texture2 = gdk_dmabuf_texture_builder_build (builder, udmabuf_free, udmabuf, NULL);

  g_object_unref (builder);

  return texture2;
}

static GskRenderNode *
dmabufize_container_node (GskRenderNode *node)
{
  guint n_nodes;
  GskRenderNode **nodes;

  n_nodes = gsk_container_node_get_n_children (node);
  nodes = g_newa (GskRenderNode *, n_nodes);

  for (int i = 0; i < n_nodes; i++)
    nodes[i] = dmabufize_node (gsk_container_node_get_child (node, i));

  node = gsk_container_node_new (nodes, n_nodes);

  for (int i = 0; i < n_nodes; i++)
    gsk_render_node_unref (nodes[i]);

  return node;
}

static GskRenderNode *
dmabufize_texture_node (GskRenderNode *node)
{
  GdkTexture *texture = gsk_texture_node_get_texture (node);
  graphene_rect_t bounds;

  gsk_render_node_get_bounds (node, &bounds);

  texture = dmabufize_texture (texture);

  node = gsk_texture_node_new (texture, &bounds);

  g_object_unref (texture);

  return node;
}

static GskRenderNode *
dmabufize_texture_scale_node (GskRenderNode *node)
{
  GdkTexture *texture = gsk_texture_scale_node_get_texture (node);
  graphene_rect_t bounds;

  gsk_render_node_get_bounds (node, &bounds);

  texture = dmabufize_texture (texture);

  node = gsk_texture_scale_node_new (texture, &bounds, gsk_texture_scale_node_get_filter (node));

  g_object_unref (texture);

  return node;
}

static GskRenderNode *
dmabufize_transform_node (GskRenderNode *node)
{
  GskRenderNode *child = gsk_transform_node_get_child (node);

  child = dmabufize_node (child);
  node = gsk_transform_node_new (child, gsk_transform_node_get_transform (node));
  gsk_render_node_unref (child);

  return node;
}

static GskRenderNode *
dmabufize_opacity_node (GskRenderNode *node)
{
  GskRenderNode *child = gsk_opacity_node_get_child (node);

  child = dmabufize_node (child);
  node = gsk_opacity_node_new (child, gsk_opacity_node_get_opacity (node));
  gsk_render_node_unref (child);

  return node;
}

static GskRenderNode *
dmabufize_color_matrix_node (GskRenderNode *node)
{
  GskRenderNode *child = gsk_color_matrix_node_get_child (node);

  child = dmabufize_node (child);
  node = gsk_color_matrix_node_new (child, gsk_color_matrix_node_get_color_matrix (node),
                                           gsk_color_matrix_node_get_color_offset (node));
  gsk_render_node_unref (child);

  return node;
}

static GskRenderNode *
dmabufize_repeat_node (GskRenderNode *node)
{
  GskRenderNode *child = gsk_repeat_node_get_child (node);
  graphene_rect_t bounds;

  gsk_render_node_get_bounds (node, &bounds);

  child = dmabufize_node (child);
  node = gsk_repeat_node_new (&bounds,
                              child,
                              gsk_repeat_node_get_child_bounds (node));
  gsk_render_node_unref (child);

  return node;
}

static GskRenderNode *
dmabufize_clip_node (GskRenderNode *node)
{
  GskRenderNode *child = gsk_clip_node_get_child (node);

  child = dmabufize_node (child);
  node = gsk_clip_node_new (child, gsk_clip_node_get_clip (node));
  gsk_render_node_unref (child);

  return node;
}

static GskRenderNode *
dmabufize_rounded_clip_node (GskRenderNode *node)
{
  GskRenderNode *child = gsk_rounded_clip_node_get_child (node);

  child = dmabufize_node (child);
  node = gsk_rounded_clip_node_new (child, gsk_rounded_clip_node_get_clip (node));
  gsk_render_node_unref (child);

  return node;
}

static GskRenderNode *
dmabufize_shadow_node (GskRenderNode *node)
{
  GskRenderNode *child = gsk_shadow_node_get_child (node);

  child = dmabufize_node (child);
  node = gsk_shadow_node_new (child,
                              gsk_shadow_node_get_shadow (node, 0),
                              gsk_shadow_node_get_n_shadows (node));
  gsk_render_node_unref (child);

  return node;
}

static GskRenderNode *
dmabufize_blend_node (GskRenderNode *node)
{
  GskRenderNode *top = gsk_blend_node_get_top_child (node);
  GskRenderNode *bottom = gsk_blend_node_get_bottom_child (node);

  top = dmabufize_node (top);
  bottom = dmabufize_node (bottom);
  node = gsk_blend_node_new (bottom, top, gsk_blend_node_get_blend_mode (node));
  gsk_render_node_unref (top);
  gsk_render_node_unref (bottom);

  return node;
}

static GskRenderNode *
dmabufize_cross_fade_node (GskRenderNode *node)
{
  GskRenderNode *start = gsk_cross_fade_node_get_start_child (node);
  GskRenderNode *end = gsk_cross_fade_node_get_end_child (node);

  start = dmabufize_node (start);
  end = dmabufize_node (end);
  node = gsk_cross_fade_node_new (start, end, gsk_cross_fade_node_get_progress (node));
  gsk_render_node_unref (start);
  gsk_render_node_unref (end);

  return node;
}

static GskRenderNode *
dmabufize_blur_node (GskRenderNode *node)
{
  GskRenderNode *child = gsk_blur_node_get_child (node);

  child = dmabufize_node (child);
  node = gsk_blur_node_new (child, gsk_blur_node_get_radius (node));
  gsk_render_node_unref (child);

  return node;
}

static GskRenderNode *
dmabufize_debug_node (GskRenderNode *node)
{
  GskRenderNode *child = gsk_debug_node_get_child (node);

  child = dmabufize_node (child);
  node = gsk_debug_node_new (child, (char *)gsk_debug_node_get_message (node));
  gsk_render_node_unref (child);

  return node;
}

static GskRenderNode *
dmabufize_mask_node (GskRenderNode *node)
{
  GskRenderNode *source = gsk_mask_node_get_source (node);
  GskRenderNode *mask = gsk_mask_node_get_mask (node);

  source = dmabufize_node (source);
  mask = dmabufize_node (mask);
  node = gsk_mask_node_new (source, mask, gsk_mask_node_get_mask_mode (node));
  gsk_render_node_unref (source);
  gsk_render_node_unref (mask);

  return node;
}

static GskRenderNode *
dmabufize_fill_node (GskRenderNode *node)
{
  GskRenderNode *child = gsk_fill_node_get_child (node);

  child = dmabufize_node (child);
  node = gsk_fill_node_new (child, gsk_fill_node_get_path (node), gsk_fill_node_get_fill_rule (node));
  gsk_render_node_unref (child);

  return node;
}

static GskRenderNode *
dmabufize_stroke_node (GskRenderNode *node)
{
  GskRenderNode *child = gsk_stroke_node_get_child (node);

  child = dmabufize_node (child);
  node = gsk_stroke_node_new (child, gsk_stroke_node_get_path (node), gsk_stroke_node_get_stroke (node));
  gsk_render_node_unref (child);

  return node;
}

static GskRenderNode *
dmabufize_subsurface_node (GskRenderNode *node)
{
  GskRenderNode *child = gsk_subsurface_node_get_child (node);

  child = dmabufize_node (child);
  node = gsk_subsurface_node_new (child, gsk_subsurface_node_get_subsurface (node));
  gsk_render_node_unref (child);

  return node;
}

GskRenderNode *
dmabufize_node (GskRenderNode *node)
{
  switch (gsk_render_node_get_node_type (node))
    {
    case GSK_CONTAINER_NODE:
      return dmabufize_container_node (node);

    case GSK_CAIRO_NODE:
    case GSK_COLOR_NODE:
    case GSK_LINEAR_GRADIENT_NODE:
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
    case GSK_RADIAL_GRADIENT_NODE:
    case GSK_REPEATING_RADIAL_GRADIENT_NODE:
    case GSK_CONIC_GRADIENT_NODE:
    case GSK_BORDER_NODE:
    case GSK_INSET_SHADOW_NODE:
    case GSK_OUTSET_SHADOW_NODE:
    case GSK_TEXT_NODE:
    case GSK_GL_SHADER_NODE:
      return gsk_render_node_ref (node);

    case GSK_TEXTURE_NODE:
      return dmabufize_texture_node (node);

    case GSK_TEXTURE_SCALE_NODE:
      return dmabufize_texture_scale_node (node);

    case GSK_TRANSFORM_NODE:
      return dmabufize_transform_node (node);

    case GSK_OPACITY_NODE:
      return dmabufize_opacity_node (node);

    case GSK_COLOR_MATRIX_NODE:
      return dmabufize_color_matrix_node (node);

    case GSK_REPEAT_NODE:
      return dmabufize_repeat_node (node);

    case GSK_CLIP_NODE:
      return dmabufize_clip_node (node);

    case GSK_ROUNDED_CLIP_NODE:
      return dmabufize_rounded_clip_node (node);

    case GSK_SHADOW_NODE:
      return dmabufize_shadow_node (node);

    case GSK_BLEND_NODE:
      return dmabufize_blend_node (node);

    case GSK_CROSS_FADE_NODE:
      return dmabufize_cross_fade_node (node);

    case GSK_BLUR_NODE:
      return dmabufize_blur_node (node);

    case GSK_DEBUG_NODE:
      return dmabufize_debug_node (node);

    case GSK_MASK_NODE:
      return dmabufize_mask_node (node);

    case GSK_FILL_NODE:
      return dmabufize_fill_node (node);

    case GSK_STROKE_NODE:
      return dmabufize_stroke_node (node);

    case GSK_SUBSURFACE_NODE:
      return dmabufize_subsurface_node (node);

    case GSK_NOT_A_RENDER_NODE:
    default:
      g_assert_not_reached ();
    }
}

#else

GskRenderNode *
dmabufize_node (GskRenderNode *node)
{
  return gsk_render_node_ref (node);
}

#endif
