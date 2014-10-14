/*
 * Copyright (c) 2014 Benjamin Otte <ottte@gnome.org>
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

#include "cellrenderergraph.h"

#include "graphdata.h"

enum {
  PROP_0,
  PROP_DATA,
  PROP_MINIMUM,
  PROP_MAXIMUM
};

struct _GtkCellRendererGraphPrivate
{
  GtkGraphData *data;
  double minimum;
  double maximum;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkCellRendererGraph, gtk_cell_renderer_graph, GTK_TYPE_CELL_RENDERER)

static void 
gtk_cell_renderer_graph_dispose (GObject *object)
{
  GtkCellRendererGraph *graph = GTK_CELL_RENDERER_GRAPH (object);
  GtkCellRendererGraphPrivate *priv = graph->priv;

  g_clear_object (&priv->data);

  G_OBJECT_CLASS (gtk_cell_renderer_graph_parent_class)->dispose (object);
}

static void
gtk_cell_renderer_graph_get_property (GObject    *object,
                                      guint       param_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GtkCellRendererGraph *cell = GTK_CELL_RENDERER_GRAPH (object);
  GtkCellRendererGraphPrivate *priv = cell->priv;

  switch (param_id)
    {
      case PROP_DATA:
        g_value_set_object (value, priv->data);
        break;
      case PROP_MINIMUM:
        g_value_set_double (value, priv->minimum);
        break;
      case PROP_MAXIMUM:
        g_value_set_double (value, priv->maximum);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
gtk_cell_renderer_graph_set_property (GObject      *object,
                                      guint         param_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GtkCellRendererGraph *cell = GTK_CELL_RENDERER_GRAPH (object);
  GtkCellRendererGraphPrivate *priv = cell->priv;

  switch (param_id)
    {
      case PROP_DATA:
        if (priv->data != g_value_get_object (value))
          {
            if (priv->data)
              g_object_unref (priv->data);
            priv->data = g_value_dup_object (value);
            g_object_notify_by_pspec (object, pspec);
          }
        break;
      case PROP_MINIMUM:
        if (priv->minimum != g_value_get_double (value))
          {
            priv->minimum = g_value_get_double (value);
            g_object_notify_by_pspec (object, pspec);
          }
        break;
      case PROP_MAXIMUM:
        if (priv->maximum != g_value_get_double (value))
          {
            priv->maximum = g_value_get_double (value);
            g_object_notify_by_pspec (object, pspec);
          }
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
gtk_cell_renderer_graph_get_size (GtkCellRenderer    *cell,
                                  GtkWidget          *widget,
                                  const GdkRectangle *cell_area,
                                  gint               *x_offset,
                                  gint               *y_offset,
                                  gint               *width,
                                  gint               *height)
{
  int xpad, ypad;

#define MIN_HEIGHT 24
#define MIN_WIDTH 3 * MIN_HEIGHT

  g_object_get (cell,
                "xpad", &xpad,
                "ypad", &ypad,
                NULL);

  if (cell_area)
    {
      if (width)
        *width = cell_area->width - 2 * xpad;
      if (height)
        *height = cell_area->height - 2 * ypad;
    }
  else
    {
      if (width)
        *width = MIN_WIDTH + 2 * xpad;
      if (height)
        *height = MIN_HEIGHT + 2 * ypad;
    }

  if (x_offset)
    *x_offset = xpad;
  if (y_offset)
    *y_offset = ypad;
}

static void
gtk_cell_renderer_graph_render (GtkCellRenderer      *cell,
                                cairo_t              *cr,
                                GtkWidget            *widget,
                                const GdkRectangle   *background_area,
                                const GdkRectangle   *cell_area,
                                GtkCellRendererState  flags)
{
  GtkCellRendererGraph *graph = GTK_CELL_RENDERER_GRAPH (cell);
  GtkCellRendererGraphPrivate *priv = graph->priv;
  GtkStyleContext *context;
  double minimum, maximum, diff;
  double x, y, width, height;
  int xpad, ypad;
  GdkRGBA color;
  guint i, n;

#define LINE_WIDTH 1.0

  if (priv->data == NULL)
    return;

  g_object_get (cell,
                "xpad", &xpad,
                "ypad", &ypad,
                NULL);

  if (priv->minimum == -G_MAXDOUBLE)
    minimum = gtk_graph_data_get_minimum (priv->data);
  else
    minimum = priv->minimum;

  if (priv->maximum == G_MAXDOUBLE)
    maximum = gtk_graph_data_get_maximum (priv->data);
  else
    maximum = priv->maximum;

  diff = maximum - minimum;

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_get_color (context, gtk_style_context_get_state (context), &color);

  cairo_set_line_width (cr, 1.0);

  x = background_area->x + xpad + LINE_WIDTH / 2.0;
  y = background_area->y + ypad + LINE_WIDTH / 2.0;
  width = background_area->width - 2 * xpad - LINE_WIDTH;
  height = background_area->height - 2 * ypad - LINE_WIDTH;

  cairo_move_to (cr, x, y + height);

  if (diff > 0)
    {
      n = gtk_graph_data_get_n_values (priv->data);
      for (i = 0; i < n; i++)
        {
          double val = gtk_graph_data_get_value (priv->data, i);

          val = (val - minimum) / diff;
          val = y + height - val * height;

          cairo_line_to (cr, x + width * i / (n - 1), val);
        }
    }

  cairo_line_to (cr, x + width, y + height);
  cairo_close_path (cr);

  gdk_cairo_set_source_rgba (cr, &color);
  cairo_stroke_preserve (cr);

  color.alpha *= 0.2;
  gdk_cairo_set_source_rgba (cr, &color);
  cairo_fill (cr);
}

static void
gtk_cell_renderer_graph_class_init (GtkCellRendererGraphClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (klass);

  object_class->dispose = gtk_cell_renderer_graph_dispose;
  object_class->get_property = gtk_cell_renderer_graph_get_property;
  object_class->set_property = gtk_cell_renderer_graph_set_property;

  cell_class->get_size = gtk_cell_renderer_graph_get_size;
  cell_class->render = gtk_cell_renderer_graph_render;

  g_object_class_install_property (object_class,
                                   PROP_DATA,
                                   g_param_spec_object ("data",
                                                        "Data",
                                                        "The data to display",
                                                         GTK_TYPE_GRAPH_DATA,
                                                         G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (object_class,
                                   PROP_MINIMUM,
                                   g_param_spec_double ("minimum",
                                                        "Minimum",
                                                        "Minimum value to use (or -G_MAXDOUBLE for graph's value",
                                                        -G_MAXDOUBLE, G_MAXDOUBLE, -G_MAXDOUBLE,
                                                        G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_property (object_class,
                                   PROP_MINIMUM,
                                   g_param_spec_double ("maximum",
                                                        "Maximum",
                                                        "Maximum value to use (or G_MAXDOUBLE for graph's value",
                                                        -G_MAXDOUBLE, G_MAXDOUBLE, G_MAXDOUBLE,
                                                        G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
}

static void
gtk_cell_renderer_graph_init (GtkCellRendererGraph *cell)
{
  cell->priv = gtk_cell_renderer_graph_get_instance_private (cell);

  cell->priv->minimum = -G_MAXDOUBLE;
  cell->priv->maximum = G_MAXDOUBLE;
}

GtkCellRenderer *
gtk_cell_renderer_graph_new (void)
{
  return g_object_new (GTK_TYPE_CELL_RENDERER_GRAPH, NULL);
}

