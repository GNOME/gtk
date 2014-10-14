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

#include "graphdata.h"

enum {
  PROP_0,
  PROP_N_VALUES
};

struct _GtkGraphDataPrivate
{
  guint n_values;
  guint offset;
  double *values;
};


G_DEFINE_TYPE_WITH_PRIVATE (GtkGraphData, gtk_graph_data, G_TYPE_OBJECT)

static void
gtk_graph_data_get_property (GObject    *object,
                             guint       param_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkGraphData *graph = GTK_GRAPH_DATA (object);
  GtkGraphDataPrivate *priv = graph->priv;

  switch (param_id)
    {
      case PROP_N_VALUES:
        g_value_set_boolean (value, priv->n_values);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
gtk_graph_data_finalize (GObject *object)
{
  GtkGraphData *graph = GTK_GRAPH_DATA (object);
  GtkGraphDataPrivate *priv = graph->priv;

  g_free (priv->values);

  G_OBJECT_CLASS (gtk_graph_data_parent_class)->finalize (object);
}

static void
gtk_graph_data_set_property (GObject      *object,
                             guint         param_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkGraphData *graph = GTK_GRAPH_DATA (object);
  GtkGraphDataPrivate *priv = graph->priv;

  switch (param_id)
    {
      case PROP_N_VALUES:
        priv->n_values = g_value_get_uint (value);
        priv->values = g_new0 (double, priv->n_values);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
gtk_graph_data_class_init (GtkGraphDataClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_graph_data_finalize;
  object_class->get_property = gtk_graph_data_get_property;
  object_class->set_property = gtk_graph_data_set_property;

  g_object_class_install_property (object_class,
                                   PROP_N_VALUES,
                                   g_param_spec_uint ("n-values",
                                                      "n values",
                                                      "Number of values in the graph",
                                                      1, G_MAXUINT, 1,
                                                      G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY));
}

static void
gtk_graph_data_init (GtkGraphData *graph)
{
  graph->priv = gtk_graph_data_get_instance_private (graph);
}

GtkGraphData *
gtk_graph_data_new (guint n_values)
{
  return g_object_new (GTK_TYPE_GRAPH_DATA,
                       "n-values", n_values,
                       NULL);
}

guint
gtk_graph_data_get_n_values (GtkGraphData *data)
{
  return data->priv->n_values;
}

double
gtk_graph_data_get_value (GtkGraphData   *data,
                          guint           i)
{
  GtkGraphDataPrivate *priv = data->priv;

  return priv->values[(priv->offset + i) % priv->n_values];
}

double
gtk_graph_data_get_minimum (GtkGraphData *data)
{
  GtkGraphDataPrivate *priv = data->priv;
  double minimum = G_MAXDOUBLE;
  guint i;

  for (i = 0; i < priv->n_values; i++)
    {
      minimum = MIN (minimum, priv->values[i]);
    }

  return minimum;
}

double
gtk_graph_data_get_maximum (GtkGraphData *data)
{
  GtkGraphDataPrivate *priv = data->priv;
  double maximum = -G_MAXDOUBLE;
  guint i;

  for (i = 0; i < priv->n_values; i++)
    {
      maximum = MAX (maximum, priv->values[i]);
    }

  return maximum;
}

void
gtk_graph_data_prepend_value (GtkGraphData *data,
                              double        value)
{
  GtkGraphDataPrivate *priv = data->priv;

  priv->offset = (priv->offset + priv->n_values - 1) % priv->n_values;
  priv->values[priv->offset] = value;
}

