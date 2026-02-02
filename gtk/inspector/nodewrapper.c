/*
 * Copyright (c) 2025 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "nodewrapper.h"

#include "gsk/gskdebugnodeprivate.h"
#include "gsk/gskdisplacementnodeprivate.h"
#include "gsk/gsklineargradientnodeprivate.h"
#include "gsk/gskrectprivate.h"
#include "gsk/gskrendernodeprivate.h"
#include "gdk/gdkcairoprivate.h"
#include "gdk/gdktextureprivate.h"

#include <glib/gi18n-lib.h>

struct _GtkInspectorNodeWrapper
{
  GObject parent;

  GskRenderNode *node;
  GskRenderNode *profile_node;
  GskRenderNode *draw_node;
  char *role;
};

struct _GtkInspectorNodeWrapperClass
{
  GObjectClass parent;
};

enum
{
  PROP_0,
  PROP_DRAW_NODE,
  PROP_NODE,
  PROP_PROFILE_NODE,
  PROP_ROLE,

  N_PROPS,
};

static GParamSpec *props[N_PROPS] = { NULL, };


G_DEFINE_TYPE (GtkInspectorNodeWrapper, gtk_inspector_node_wrapper, G_TYPE_OBJECT)

static void
gtk_inspector_node_wrapper_get_property (GObject    *object,
                                         guint       param_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  GtkInspectorNodeWrapper *self = GTK_INSPECTOR_NODE_WRAPPER (object);

  switch (param_id)
    {
    case PROP_DRAW_NODE:
      g_value_set_boxed (value, self->draw_node);
      break;

    case PROP_NODE:
      g_value_set_boxed (value, self->node);
      break;

    case PROP_PROFILE_NODE:
      g_value_set_boxed (value, self->profile_node);
      break;

    case PROP_ROLE:
      g_value_set_string (value, self->role);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gtk_inspector_node_wrapper_set_property (GObject      *object,
                                         guint         param_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  GtkInspectorNodeWrapper *self = GTK_INSPECTOR_NODE_WRAPPER (object);

  switch (param_id)
    {
    case PROP_DRAW_NODE:
      if (g_value_get_pointer (value))
        self->draw_node = gsk_render_node_ref (g_value_get_pointer (value));
      break;

    case PROP_NODE:
      self->node = gsk_render_node_ref (g_value_get_pointer (value));
      break;

    case PROP_PROFILE_NODE:
      if (g_value_get_pointer (value))
        self->profile_node = gsk_render_node_ref (g_value_get_pointer (value));
      break;

    case PROP_ROLE:
      self->role = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gtk_inspector_node_wrapper_dispose (GObject *object)
{
  GtkInspectorNodeWrapper *self = GTK_INSPECTOR_NODE_WRAPPER (object);

  g_clear_pointer (&self->node, gsk_render_node_unref);
  g_clear_pointer (&self->profile_node, gsk_render_node_unref);
  g_clear_pointer (&self->draw_node, gsk_render_node_unref);

  G_OBJECT_CLASS (gtk_inspector_node_wrapper_parent_class)->dispose (object);
}

static void
gtk_inspector_node_wrapper_class_init (GtkInspectorNodeWrapperClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_inspector_node_wrapper_dispose;
  object_class->get_property = gtk_inspector_node_wrapper_get_property;
  object_class->set_property = gtk_inspector_node_wrapper_set_property;

  props[PROP_DRAW_NODE] =
    g_param_spec_pointer ("draw-node", NULL, NULL,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  props[PROP_NODE] =
    g_param_spec_pointer ("node", NULL, NULL,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  props[PROP_PROFILE_NODE] =
    g_param_spec_pointer ("profile-node", NULL, NULL,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  props[PROP_ROLE] =
    g_param_spec_string ("role", NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_properties (object_class, N_PROPS, props);
}

static void
gtk_inspector_node_wrapper_init (GtkInspectorNodeWrapper *vis)
{
}

GtkInspectorNodeWrapper *
gtk_inspector_node_wrapper_new (GskRenderNode *node,
                                GskRenderNode *profile_node,
                                GskRenderNode *draw_node,
                                const char    *role)
{
  return g_object_new (GTK_TYPE_INSPECTOR_NODE_WRAPPER,
                       "node", node,
                       "profile-node", profile_node,
                       "draw-node", draw_node,
                       "role", role,
                       NULL);
}

GskRenderNode *
gtk_inspector_node_wrapper_get_node (GtkInspectorNodeWrapper *self)
{
  return self->node;
}

GskRenderNode *
gtk_inspector_node_wrapper_get_profile_node (GtkInspectorNodeWrapper *self)
{
  return self->profile_node;
}

const GskDebugProfile *
gtk_inspector_node_wrapper_get_profile (GtkInspectorNodeWrapper *self)
{
  if (self->profile_node == NULL ||
      gsk_render_node_get_node_type (self->profile_node) != GSK_DEBUG_NODE)
    return NULL;

  return gsk_debug_node_get_profile (self->profile_node);
}

GskRenderNode *
gtk_inspector_node_wrapper_get_draw_node (GtkInspectorNodeWrapper *self)
{
  return self->draw_node;
}

const char *
gtk_inspector_node_wrapper_get_role (GtkInspectorNodeWrapper *self)
{
  return self->role;
}

static const char **
get_roles (GskRenderNodeType node_type)
{
  static const char *blend_node_roles[] = { "Bottom", "Top", NULL };
  static const char *mask_node_roles[] = { "Source", "Mask", NULL };
  static const char *cross_fade_node_roles[] = { "Start", "End", NULL };
  static const char *composite_node_roles[] = { "Child", "Mask", NULL };
  static const char *displacement_node_roles[] = { "Child", "Displacement", NULL };
  static const char *arithmetic_node_roles[] = { "First", "Second", NULL };

  switch (node_type)
    {
    case GSK_BLEND_NODE:
      return blend_node_roles;
    case GSK_MASK_NODE:
      return mask_node_roles;
    case GSK_CROSS_FADE_NODE:
      return cross_fade_node_roles;
    case GSK_COMPOSITE_NODE:
      return composite_node_roles;
    case GSK_DISPLACEMENT_NODE:
      return displacement_node_roles;
    case GSK_ARITHMETIC_NODE:
      return arithmetic_node_roles;
    case GSK_CONTAINER_NODE:
    case GSK_CAIRO_NODE:
    case GSK_LINEAR_GRADIENT_NODE:
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
    case GSK_RADIAL_GRADIENT_NODE:
    case GSK_REPEATING_RADIAL_GRADIENT_NODE:
    case GSK_CONIC_GRADIENT_NODE:
    case GSK_BORDER_NODE:
    case GSK_INSET_SHADOW_NODE:
    case GSK_OUTSET_SHADOW_NODE:
    case GSK_TRANSFORM_NODE:
    case GSK_OPACITY_NODE:
    case GSK_COLOR_MATRIX_NODE:
    case GSK_REPEAT_NODE:
    case GSK_CLIP_NODE:
    case GSK_ROUNDED_CLIP_NODE:
    case GSK_FILL_NODE:
    case GSK_STROKE_NODE:
    case GSK_SHADOW_NODE:
    case GSK_TEXT_NODE:
    case GSK_BLUR_NODE:
    case GSK_GL_SHADER_NODE:
    case GSK_SUBSURFACE_NODE:
    case GSK_COMPONENT_TRANSFER_NODE:
    case GSK_COPY_NODE:
    case GSK_PASTE_NODE:
    case GSK_DEBUG_NODE:
    case GSK_COLOR_NODE:
    case GSK_TEXTURE_NODE:
    case GSK_TEXTURE_SCALE_NODE:
    case GSK_ISOLATION_NODE:
    case GSK_NOT_A_RENDER_NODE:
    default:
      return NULL;
    }
};

GListModel *
gtk_inspector_node_wrapper_create_children_model (GtkInspectorNodeWrapper *self)
{
  GskRenderNode **children, **draw_children, **profile_children;
  gsize i, n_children, n_draw_children, n_profile_children;
  GListStore *store;
  const char **roles;

  children = gsk_render_node_get_children (self->node, &n_children);

  if (n_children == 0)
    return NULL;

  store = g_list_store_new (GTK_TYPE_INSPECTOR_NODE_WRAPPER);

  if (self->draw_node)
    {
      if (gsk_render_node_get_node_type (self->node) == GSK_COPY_NODE)
        {
          draw_children = &self->draw_node;
          n_draw_children = 1;
        }
      else if (gsk_render_node_get_node_type (self->node) == GSK_PASTE_NODE)
        {
          draw_children = NULL;
          n_draw_children = 0;
        }
      else
        {
          draw_children = gsk_render_node_get_children (self->draw_node, &n_draw_children);
        }
    }
  else
    {
      draw_children = NULL;
      n_draw_children = 0;
    }

  if (self->profile_node)
    {
      if (gsk_render_node_get_node_type (self->profile_node) == GSK_DEBUG_NODE &&
          gsk_debug_node_get_profile (self->profile_node))
        {
          profile_children = gsk_render_node_get_children (gsk_debug_node_get_child (self->profile_node), &n_profile_children);
        }
      else
        {
          profile_children = gsk_render_node_get_children (self->profile_node, &n_profile_children);
        }
      g_assert (n_profile_children == n_children);
    }
  else
    {
      profile_children = NULL;
      n_profile_children = 0;
    }

  roles = get_roles (gsk_render_node_get_node_type (self->node));

  for (i = 0; i < n_children; i++)
    {
      GtkInspectorNodeWrapper *child;

      child = gtk_inspector_node_wrapper_new (children[i],
                                              i < n_profile_children ? profile_children[i] : NULL,
                                              i < n_draw_children ? draw_children[i] : NULL,
                                              roles ? roles[i] : NULL);
      g_list_store_append (store, child);
      g_object_unref (child);
    }

  return G_LIST_MODEL (store);
}

static double
get_heatmap_value (const GskDebugProfile *profile,
                   const graphene_rect_t *bounds,
                   const graphene_size_t *scale,
                   NodeWrapperRendering   rendering,
                   gsize                  max_value)
{
  switch (rendering)
  {
    case NODE_WRAPPER_RENDER_GPU_TIME:
      return ((double) profile->self.gpu_ns) / (bounds->size.width * scale->width * bounds->size.height * scale->height) / max_value;

    case NODE_WRAPPER_RENDER_OFFSCREENS:
      return profile->self.n_offscreens / 4.0;

    case NODE_WRAPPER_RENDER_UPLOADS:
      return profile->self.n_uploads / 4.0;

    default:
    case NODE_WRAPPER_RENDER_DEFAULT:
      g_assert_not_reached ();
      return 0;
  }
}

static void
render_heatmap_node (cairo_t               *cr,
                     GskRenderNode         *node,
                     const graphene_size_t *scale,
                     const graphene_rect_t *clip,
                     NodeWrapperRendering   rendering,
                     gsize                  max_value)
{
  switch (gsk_render_node_get_node_type (node))
  {
    case GSK_TRANSFORM_NODE:
      {
        float xx, yx, xy, yy, dx, dy;
        GskTransform *transform, *inverse;
        cairo_matrix_t ctm;
        graphene_rect_t new_clip;

        transform = gsk_transform_node_get_transform (node);
        if (gsk_transform_get_category (transform) < GSK_TRANSFORM_CATEGORY_2D)
          break;

        gsk_transform_to_2d (transform, &xx, &yx, &xy, &yy, &dx, &dy);
        cairo_matrix_init (&ctm, xx, yx, xy, yy, dx, dy);
        if (xx * yy == xy * yx)
          break;

        inverse = gsk_transform_invert (gsk_transform_ref (transform));
        gsk_transform_transform_bounds (inverse, clip, &new_clip);
        gsk_transform_unref (inverse);
        cairo_save (cr);
        cairo_transform (cr, &ctm);
        render_heatmap_node (cr,
                             gsk_transform_node_get_child (node),
                             &GRAPHENE_SIZE_INIT (scale->width * xx, scale->height * yy),
                             &new_clip,
                             rendering,
                             max_value);
        cairo_restore (cr);
      }
      break;

    case GSK_DEBUG_NODE:
      {
        const GskDebugProfile *profile = gsk_debug_node_get_profile (node);
        graphene_rect_t bounds;
        double val;

        gsk_render_node_get_bounds (node, &bounds);
        if (!gsk_rect_intersection (&bounds, clip, &bounds))
          break;
        if (profile && profile->self.gpu_ns)
          {
            gdk_cairo_rect (cr, &bounds);
            val = get_heatmap_value (profile, &bounds, scale, rendering, max_value);
            cairo_set_source_rgb (cr, val, val, val);
            cairo_fill (cr);
          }
        render_heatmap_node (cr, gsk_debug_node_get_child (node), scale, &bounds, rendering, max_value);
      }
      break;

    case GSK_CLIP_NODE:
      {
        graphene_rect_t new_clip;

        cairo_save (cr);
        gdk_cairo_rect (cr, gsk_clip_node_get_clip (node));
        if (!gsk_rect_intersection (clip, gsk_clip_node_get_clip (node), &new_clip))
          break;
        cairo_clip (cr);
        render_heatmap_node (cr, gsk_clip_node_get_child (node), scale, &new_clip, rendering, max_value);
        cairo_restore (cr);
      }
      break;

    case GSK_ROUNDED_CLIP_NODE:
      {
        graphene_rect_t new_clip;

        cairo_save (cr);
        gdk_cairo_rect (cr, &gsk_rounded_clip_node_get_clip (node)->bounds);
        if (!gsk_rect_intersection (clip, &gsk_rounded_clip_node_get_clip (node)->bounds, &new_clip))
          break;
        cairo_clip (cr);
        render_heatmap_node (cr, gsk_rounded_clip_node_get_child (node), scale, &new_clip, rendering, max_value);
        cairo_restore (cr);
      }
      break;

    case GSK_CONTAINER_NODE:
    case GSK_CAIRO_NODE:
    case GSK_COLOR_NODE:
    case GSK_LINEAR_GRADIENT_NODE:
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
    case GSK_RADIAL_GRADIENT_NODE:
    case GSK_REPEATING_RADIAL_GRADIENT_NODE:
    case GSK_CONIC_GRADIENT_NODE:
    case GSK_BORDER_NODE:
    case GSK_TEXTURE_NODE:
    case GSK_INSET_SHADOW_NODE:
    case GSK_OUTSET_SHADOW_NODE:
    case GSK_OPACITY_NODE:
    case GSK_COLOR_MATRIX_NODE:
    case GSK_REPEAT_NODE:
    case GSK_SHADOW_NODE:
    case GSK_BLEND_NODE:
    case GSK_CROSS_FADE_NODE:
    case GSK_TEXT_NODE:
    case GSK_BLUR_NODE:
    case GSK_GL_SHADER_NODE:
    case GSK_TEXTURE_SCALE_NODE:
    case GSK_MASK_NODE:
    case GSK_FILL_NODE:
    case GSK_STROKE_NODE:
    case GSK_SUBSURFACE_NODE:
    case GSK_COMPONENT_TRANSFER_NODE:
    case GSK_COPY_NODE:
    case GSK_PASTE_NODE:
    case GSK_COMPOSITE_NODE:
    case GSK_ISOLATION_NODE:
    case GSK_DISPLACEMENT_NODE:
    case GSK_ARITHMETIC_NODE:
      {
        GskRenderNode **children;
        gsize i, n_children;
        graphene_rect_t bounds;

        cairo_save (cr);
        gsk_render_node_get_bounds (node, &bounds);
        gdk_cairo_rect (cr, &bounds);
        cairo_clip (cr);
        if (!gsk_rect_intersection (&bounds, clip, &bounds))
          break;
        children = gsk_render_node_get_children (node, &n_children);
        for (i = 0; i < n_children; i++)
          render_heatmap_node (cr, children[i], scale, &bounds, rendering, max_value);
        cairo_restore (cr);
      }
      break;

    case GSK_NOT_A_RENDER_NODE:
    default:
      g_return_if_reached ();
  }
}

static gboolean
should_scale_surface (NodeWrapperRendering rendering)
{
  switch (rendering)
  {
    case NODE_WRAPPER_RENDER_DEFAULT:
    case NODE_WRAPPER_RENDER_OFFSCREENS:
    case NODE_WRAPPER_RENDER_UPLOADS:
      return FALSE;

    case NODE_WRAPPER_RENDER_GPU_TIME:
      return TRUE;

    default:
      g_assert_not_reached ();
      return FALSE;
  }
}

static void
scale_surface (cairo_surface_t *surface)
{
  float max = 0;
  guint8 *data;
  gsize width, height, stride;
  gsize x, y;

  cairo_surface_flush (surface);

  data = cairo_image_surface_get_data (surface);
  width = cairo_image_surface_get_width (surface);
  height = cairo_image_surface_get_height (surface);
  stride = cairo_image_surface_get_stride (surface);

  for (y = 0; y < height; y++)
    {
      float *row = (float *) (data + y * stride);

      for (x = 0; x < width; x++)
        max = MAX (max, row[3 * x]);
    }

  if (max >= 1.0 || max <= 0.0)
    return;

  for (y = 0; y < height; y++)
    {
      float *row = (float *) (data + y * stride);

      for (x = 0; x < width; x++)
        {
          float val = row[3 * x] / max;
          row[3 * x + 0] = val;
          row[3 * x + 1] = val;
          row[3 * x + 2] = val;
        }
    }

  cairo_surface_mark_dirty (surface);
}

static GdkTexture *
render_heatmap_mask (GskRenderNode        *node,
                     NodeWrapperRendering  rendering)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  graphene_rect_t bounds;
  GdkTexture *texture;
  gsize max_value, n_pixels;

  gsk_render_node_get_bounds (node, &bounds);

  n_pixels = ceil (bounds.size.width) * ceil (bounds.size.height);
  max_value = 100 * 1024 * n_pixels;
  if (gsk_render_node_get_node_type (node) == GSK_DEBUG_NODE)
    {
      const GskDebugProfile *profile = gsk_debug_node_get_profile (node);

      if (profile != NULL)
        max_value = 100 * profile->total.gpu_ns / n_pixels;
    }

  surface = cairo_image_surface_create (CAIRO_FORMAT_RGB96F,
                                        ceil (bounds.size.width),
                                        ceil (bounds.size.height));

  cr = cairo_create (surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_ADD);
  cairo_translate (cr, - bounds.origin.x, - bounds.origin.y);

  render_heatmap_node (cr, node, &GRAPHENE_SIZE_INIT (1.0, 1.0), &bounds, rendering, max_value);

  if (should_scale_surface (rendering))
    scale_surface (surface);

  cairo_destroy (cr);

  texture = gdk_texture_new_for_surface (surface);
  cairo_surface_destroy (surface);

  return texture;
}

static GskGradient *
get_heatmap_gradient (NodeWrapperRendering rendering)
{
  GskGradient *gradient;

  gradient = gsk_gradient_new ();

  switch (rendering)
  {
    case NODE_WRAPPER_RENDER_GPU_TIME:
      gsk_gradient_add_color_stops (gradient,
                                    (GskColorStop[4]) {
                                        { 0.0, { 0.3, 0.7, 0, 0.0 } },
                                        { 0.1, { 0.3, 0.7, 0, 0.2 } },
                                        { 0.5, { 1, 1, 0, 0.8 } },
                                        { 1.0, { 1, 0, 0, 0.8 } },
                                    },
                                    4);
      break;

    case NODE_WRAPPER_RENDER_OFFSCREENS:
    case NODE_WRAPPER_RENDER_UPLOADS:
      gsk_gradient_add_color_stops (gradient,
                                    (GskColorStop[8]) {
                                        { 0.125, { 0.0, 0.0, 0.0, 0.0 } },
                                        { 0.125, { 0.8, 0.8, 0.0, 0.8 } },
                                        { 0.375, { 0.8, 0.8, 0.0, 0.8 } },
                                        { 0.375, { 0.8, 0.6, 0.0, 0.8 } },
                                        { 0.625, { 0.8, 0.6, 0.0, 0.8 } },
                                        { 0.625, { 0.9, 0.4, 0.0, 0.8 } },
                                        { 0.875, { 0.9, 0.4, 0.0, 0.8 } },
                                        { 0.875, { 0.9, 0.0, 0.0, 0.8 } },
                                    },
                                    8);
      break;

    default:
    case NODE_WRAPPER_RENDER_DEFAULT:
      g_assert_not_reached ();
      break;
  }

  return gradient;
}

static GskRenderNode *
heatmap_from_mask (GskRenderNode        *mask,
                   NodeWrapperRendering  rendering)
{
  GskRenderNode *mask_gradient, *container, *gradient_node, *displacement;
  GskGradient *gradient;
  graphene_rect_t bounds;

  gsk_render_node_get_bounds (mask, &bounds);
  
  mask_gradient = gsk_linear_gradient_node_new (&bounds,
                                                &GRAPHENE_POINT_INIT (bounds.origin.x,
                                                                      bounds.origin.y),
                                                &GRAPHENE_POINT_INIT (bounds.origin.x + bounds.size.width,
                                                                      bounds.origin.y),
                                                (GskColorStop[2]) {
                                                  { 0, { 1, 1, 1, 0.5 } },
                                                  { 1, { 0, 0, 0, 0.5 } },
                                                },
                                                2);
  container = gsk_container_node_new ((GskRenderNode *[2]) { mask, mask_gradient }, 2);
  gsk_render_node_unref (mask_gradient);

  gradient = get_heatmap_gradient (rendering);
  gradient_node = gsk_linear_gradient_node_new2 (&GRAPHENE_RECT_INIT (bounds.origin.x - 10,
                                                                      bounds.origin.y,
                                                                      bounds.size.width + 20,
                                                                      bounds.size.height),
                                                 &GRAPHENE_POINT_INIT (bounds.origin.x,
                                                                       bounds.origin.y),
                                                 &GRAPHENE_POINT_INIT (bounds.origin.x + bounds.size.width,
                                                                       bounds.origin.y),
                                                 gradient);
  gsk_gradient_free (gradient);

  displacement = gsk_displacement_node_new (&bounds,
                                            gradient_node,
                                            container,
                                            (GdkColorChannel[2]) { GDK_COLOR_CHANNEL_RED, GDK_COLOR_CHANNEL_GREEN },
                                            &GRAPHENE_SIZE_INIT (bounds.size.width * 2, 0.1),
                                            &GRAPHENE_SIZE_INIT (bounds.size.width * 2, 0),
                                            &GRAPHENE_POINT_INIT (0.5, 0));
  gsk_render_node_unref (container);
  gsk_render_node_unref (gradient_node);
  
  return displacement;
}

GskRenderNode *
gtk_inspector_node_wrapper_render (GtkInspectorNodeWrapper *self,
                                   NodeWrapperRendering     rendering)
{
  GdkTexture *texture;
  GskRenderNode *result, *heatmap, *texture_node;
  graphene_rect_t bounds;

  if (rendering == NODE_WRAPPER_RENDER_DEFAULT)
    return gsk_render_node_ref (self->draw_node);

  if (self->profile_node == NULL)
    return gsk_render_node_ref (self->draw_node);

  gsk_render_node_get_bounds (self->profile_node, &bounds);

  texture = render_heatmap_mask (self->profile_node, rendering);
  texture_node = gsk_texture_node_new (texture, &bounds);
  g_object_unref (texture);

  heatmap = heatmap_from_mask (texture_node, rendering);
  gsk_render_node_unref (texture_node);

  result = gsk_container_node_new ((GskRenderNode *[2]) { self->draw_node, heatmap }, 2);
  gsk_render_node_unref (heatmap);

  return result;
}

