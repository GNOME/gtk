/*
 * Copyright (c) 2016 Red Hat, Inc.
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

#include "rendernodeview.h"

#include <cairo-gobject.h>
#include <glib/gi18n-lib.h>
#include <math.h>

#include <gtk/gtksnapshot.h>

#include "gsk/gskrendernodeprivate.h"

#include "fallback-c89.c"

typedef struct _GtkRenderNodeViewPrivate GtkRenderNodeViewPrivate;
struct _GtkRenderNodeViewPrivate
{
  GdkRectangle viewport;
  GskRenderNode *render_node;
  cairo_region_t *render_region;
  cairo_region_t *clip_region;
};

enum
{
  PROP_0,
  PROP_VIEWPORT,
  PROP_RENDER_NODE,
  PROP_RENDER_REGION,
  PROP_CLIP_REGION,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (GtkRenderNodeView, gtk_render_node_view, GTK_TYPE_WIDGET)

static gboolean
gtk_render_node_view_has_viewport (GtkRenderNodeView *view)
{
  GtkRenderNodeViewPrivate *priv = gtk_render_node_view_get_instance_private (view);

  return priv->viewport.width > 0
      && priv->viewport.height > 0;
}

static void
gtk_render_node_view_get_effective_viewport (GtkRenderNodeView *view,
                                             GdkRectangle      *viewport)
{
  GtkRenderNodeViewPrivate *priv = gtk_render_node_view_get_instance_private (view);

  if (gtk_render_node_view_has_viewport (view))
    {
      *viewport = priv->viewport;
    }
  else if (priv->render_node != NULL)
    {
      graphene_rect_t bounds;

      gsk_render_node_get_bounds (priv->render_node, &bounds);

      viewport->x = bounds.origin.x;
      viewport->y = bounds.origin.y;
      viewport->width = bounds.size.width;
      viewport->height = bounds.size.height;
    }
  else
    {
      *viewport = (GdkRectangle) { 0, 0, 0, 0 };
    }
}

static void
gtk_render_node_view_get_property (GObject    *object,
                                   guint       param_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GtkRenderNodeView *view = GTK_RENDER_NODE_VIEW (object);
  GtkRenderNodeViewPrivate *priv = gtk_render_node_view_get_instance_private (view);

  switch (param_id)
    {
    case PROP_VIEWPORT:
      if (gtk_render_node_view_has_viewport (view))
        g_value_set_boxed (value, &priv->viewport);
      else
        g_value_set_boxed (value, NULL);
      break;

    case PROP_RENDER_NODE:
      g_value_set_pointer (value, priv->render_node);
      break;

    case PROP_RENDER_REGION:
      g_value_set_boxed (value, priv->render_region);
      break;

    case PROP_CLIP_REGION:
      g_value_set_boxed (value, priv->clip_region);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gtk_render_node_view_set_property (GObject      *object,
                                   guint         param_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GtkRenderNodeView *view = GTK_RENDER_NODE_VIEW (object);

  switch (param_id)
    {
    case PROP_VIEWPORT:
      gtk_render_node_view_set_viewport (view, g_value_get_boxed (value));
      break;

    case PROP_RENDER_NODE:
      gtk_render_node_view_set_render_node (view, g_value_get_pointer (value));
      break;

    case PROP_RENDER_REGION:
      gtk_render_node_view_set_render_region (view, g_value_get_boxed (value));
      break;

    case PROP_CLIP_REGION:
      gtk_render_node_view_set_clip_region (view, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gtk_render_node_view_dispose (GObject *object)
{
  GtkRenderNodeView *view = GTK_RENDER_NODE_VIEW (object);
  GtkRenderNodeViewPrivate *priv = gtk_render_node_view_get_instance_private (view);

  g_clear_pointer (&priv->render_node, gsk_render_node_unref);
  g_clear_pointer (&priv->render_region, cairo_region_destroy);
  g_clear_pointer (&priv->clip_region, cairo_region_destroy);

  G_OBJECT_CLASS (gtk_render_node_view_parent_class)->dispose (object);
}

static void
gtk_render_node_view_measure (GtkWidget      *widget,
                              GtkOrientation  orientation,
                              int             for_size,
                              int            *minimum,
                              int            *natural,
                              int            *minimum_baseline,
                              int            *natural_baseline)
{
  GtkRenderNodeView *view = GTK_RENDER_NODE_VIEW (widget);
  GtkRenderNodeViewPrivate *priv = gtk_render_node_view_get_instance_private (view);
  GdkRectangle viewport;

  *minimum = 1;

  if (priv->render_node == NULL)
    {
      *natural = *minimum;
      return;
    }

  gtk_render_node_view_get_effective_viewport (view, &viewport);
  if (for_size < 0)
    {
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        *natural = viewport.width;
      else
        *natural = viewport.height;
    }
  else
    {
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          if (for_size > viewport.height)
            *natural = viewport.width;
          else
            *natural = ceil ((double) for_size * viewport.width / viewport.height);
        }
      else
        {
          if (for_size > viewport.width)
            *natural = viewport.height;
          else
            *natural = ceil ((double) for_size * viewport.height / viewport.width);
        }
    }
}

static void
gtk_render_node_view_snapshot (GtkWidget   *widget,
                               GtkSnapshot *snapshot)
{
  GtkRenderNodeView *view = GTK_RENDER_NODE_VIEW (widget);
  GtkRenderNodeViewPrivate *priv = gtk_render_node_view_get_instance_private (view);
  GdkRectangle viewport;
  graphene_rect_t rect;
  int width, height;
  cairo_t *cr;

  if (priv->render_node == NULL)
    return;

  gtk_render_node_view_get_effective_viewport (view, &viewport);
  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  graphene_rect_init (&rect, 0, 0, width, height);
  cr = gtk_snapshot_append_cairo (snapshot,
                                  &rect,
                                  "RenderNodeView");

  cairo_translate (cr, width / 2.0, height / 2.0);
  if (width < viewport.width || height < viewport.height)
    {
      double scale = MIN ((double) width / viewport.width, (double) height / viewport.height);
      cairo_scale (cr, scale, scale);
    }
  cairo_translate (cr, - viewport.x - viewport.width / 2.0, - viewport.y - viewport.height / 2.0);

  gsk_render_node_draw (priv->render_node, cr);

  if (priv->render_region)
    {
      cairo_region_t *draw;
      cairo_pattern_t *linear;

      linear = cairo_pattern_create_linear (0, 0, 10, 10);
      cairo_pattern_set_extend (linear, CAIRO_EXTEND_REPEAT);
      cairo_pattern_add_color_stop_rgba (linear, 0.4, 0, 0, 0, 0);
      cairo_pattern_add_color_stop_rgba (linear, 0.45, 0, 0, 0, 0.5);
      cairo_pattern_add_color_stop_rgba (linear, 0.55, 0, 0, 0, 0.5);
      cairo_pattern_add_color_stop_rgba (linear, 0.6, 0, 0, 0, 0);
      
      draw = cairo_region_create_rectangle (&viewport);
      cairo_region_subtract (draw, priv->render_region);

      cairo_set_source (cr, linear);
      gdk_cairo_region (cr, draw);
      cairo_fill (cr);

      cairo_region_destroy (draw);
      cairo_pattern_destroy (linear);
    }

  if (priv->clip_region)
    {
      cairo_region_t *draw;
      
      draw = cairo_region_create_rectangle (&viewport);
      cairo_region_subtract (draw, priv->clip_region);

      cairo_set_source_rgba (cr, 0, 0, 0, 0.5);
      gdk_cairo_region (cr, draw);
      cairo_fill (cr);

      cairo_region_destroy (draw);
    }

  cairo_destroy (cr);
}

static void
gtk_render_node_view_class_init (GtkRenderNodeViewClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gtk_render_node_view_get_property;
  object_class->set_property = gtk_render_node_view_set_property;
  object_class->dispose = gtk_render_node_view_dispose;

  widget_class->measure = gtk_render_node_view_measure;
  widget_class->snapshot = gtk_render_node_view_snapshot;

  props[PROP_VIEWPORT] =
    g_param_spec_boxed ("viewport",
                        "Viewport",
                        "Viewport",
                        CAIRO_GOBJECT_TYPE_RECTANGLE_INT,
                        G_PARAM_READWRITE);

  props[PROP_RENDER_NODE] =
    g_param_spec_pointer ("render-node",
                          "Render node",
                          "Render node",
                          /* GSK_TYPE_RENDER_NODE, */
                          G_PARAM_READWRITE);

  props[PROP_RENDER_REGION] =
    g_param_spec_boxed ("render-region",
                        "Render region",
                        "Actually rendered region",
                        CAIRO_GOBJECT_TYPE_REGION,
                        G_PARAM_READWRITE);

  props[PROP_CLIP_REGION] =
    g_param_spec_boxed ("clip-region",
                        "Clip region",
                        "Clip region",
                        CAIRO_GOBJECT_TYPE_REGION,
                        G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
gtk_render_node_view_init (GtkRenderNodeView *view)
{
  gtk_widget_set_has_window (GTK_WIDGET (view), FALSE);
}

GtkWidget *
gtk_render_node_view_new (void)
{
  return g_object_new (GTK_TYPE_RENDER_NODE_VIEW, NULL);
}

void
gtk_render_node_view_set_render_node (GtkRenderNodeView *view,
                                      GskRenderNode     *node)
{
  GtkRenderNodeViewPrivate *priv = gtk_render_node_view_get_instance_private (view);

  if (priv->render_node == node)
    return;

  g_clear_pointer (&priv->render_node, gsk_render_node_unref);
  if (node)
    priv->render_node = gsk_render_node_ref (node);

  if (gtk_render_node_view_has_viewport (view))
    gtk_widget_queue_draw (GTK_WIDGET (view));
  else
    gtk_widget_queue_resize (GTK_WIDGET (view));

  g_object_notify_by_pspec (G_OBJECT (view), props[PROP_RENDER_NODE]);
}

GskRenderNode *
gtk_render_node_view_get_render_node (GtkRenderNodeView *view)
{
  GtkRenderNodeViewPrivate *priv = gtk_render_node_view_get_instance_private (view);

  return priv->render_node;
}

void
gtk_render_node_view_set_viewport (GtkRenderNodeView  *view,
                                   const GdkRectangle *viewport)
{
  GtkRenderNodeViewPrivate *priv = gtk_render_node_view_get_instance_private (view);

  if (viewport)
    {
      priv->viewport = *viewport;
    }
  else
    {
      priv->viewport = (GdkRectangle) { 0, 0, 0, 0 };
    }

  gtk_widget_queue_resize (GTK_WIDGET (view));

  g_object_notify_by_pspec (G_OBJECT (view), props[PROP_VIEWPORT]);
}

void
gtk_render_node_view_get_viewport (GtkRenderNodeView *view,
                                   GdkRectangle      *viewport)
{
  GtkRenderNodeViewPrivate *priv = gtk_render_node_view_get_instance_private (view);

  *viewport = priv->viewport;
}

void
gtk_render_node_view_set_clip_region (GtkRenderNodeView    *view,
                                      const cairo_region_t *clip)
{
  GtkRenderNodeViewPrivate *priv = gtk_render_node_view_get_instance_private (view);

  if (priv->clip_region)
    cairo_region_destroy (priv->clip_region);
  priv->clip_region = cairo_region_copy (clip);

  gtk_widget_queue_draw (GTK_WIDGET (view));

  g_object_notify_by_pspec (G_OBJECT (view), props[PROP_CLIP_REGION]);
}

const cairo_region_t*
gtk_render_node_view_get_clip_region (GtkRenderNodeView *view)
{
  GtkRenderNodeViewPrivate *priv = gtk_render_node_view_get_instance_private (view);

  return priv->clip_region;
}

void
gtk_render_node_view_set_render_region (GtkRenderNodeView    *view,
                                        const cairo_region_t *region)
{
  GtkRenderNodeViewPrivate *priv = gtk_render_node_view_get_instance_private (view);

  if (priv->render_region)
    cairo_region_destroy (priv->render_region);
  priv->render_region = cairo_region_copy (region);

  gtk_widget_queue_draw (GTK_WIDGET (view));

  g_object_notify_by_pspec (G_OBJECT (view), props[PROP_RENDER_REGION]);
}

const cairo_region_t*
gtk_render_node_view_get_render_region (GtkRenderNodeView *view)
{
  GtkRenderNodeViewPrivate *priv = gtk_render_node_view_get_instance_private (view);

  return priv->render_region;
}


// vim: set et sw=2 ts=2:
