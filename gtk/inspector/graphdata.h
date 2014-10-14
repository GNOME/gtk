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

#ifndef __GTK_GRAPH_DATA_H__
#define __GTK_GRAPH_DATA_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_TYPE_GRAPH_DATA            (gtk_graph_data_get_type ())
#define GTK_GRAPH_DATA(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_GRAPH_DATA, GtkGraphData))
#define GTK_GRAPH_DATA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_GRAPH_DATA, GtkGraphDataClass))
#define GTK_IS_GRAPH_DATA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_GRAPH_DATA))
#define GTK_IS_GRAPH_DATA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_GRAPH_DATA))
#define GTK_GRAPH_DATA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_GRAPH_DATA, GtkGraphDataClass))

typedef struct _GtkGraphData        GtkGraphData;
typedef struct _GtkGraphDataClass   GtkGraphDataClass;
typedef struct _GtkGraphDataPrivate GtkGraphDataPrivate;

struct _GtkGraphData
{
  GObject              object;

  /*< private >*/
  GtkGraphDataPrivate *priv;
};

struct _GtkGraphDataClass
{
  GObjectClass parent_class;

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};


GType            gtk_graph_data_get_type        (void) G_GNUC_CONST;

GtkGraphData    *gtk_graph_data_new             (guint           n_values);

guint            gtk_graph_data_get_n_values    (GtkGraphData   *data);
double           gtk_graph_data_get_value       (GtkGraphData   *data,
                                                 guint           i);
double           gtk_graph_data_get_minimum     (GtkGraphData   *data);
double           gtk_graph_data_get_maximum     (GtkGraphData   *data);

void             gtk_graph_data_prepend_value   (GtkGraphData   *data,
                                                 double          value);

G_END_DECLS

#endif /* __GTK_GRAPH_DATA_H__ */
