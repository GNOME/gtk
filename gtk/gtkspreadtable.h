/* 
 * Copyright (C) 2010 Openismus GmbH
 *
 * Authors:
 *      Tristan Van Berkom <tristanvb@openismus.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_SPREAD_TABLE_H__
#define __GTK_SPREAD_TABLE_H__

#include <gtk/gtkcontainer.h>

G_BEGIN_DECLS


#define GTK_TYPE_SPREAD_TABLE                  (gtk_spread_table_get_type ())
#define GTK_SPREAD_TABLE(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SPREAD_TABLE, GtkSpreadTable))
#define GTK_SPREAD_TABLE_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_SPREAD_TABLE, GtkSpreadTableClass))
#define GTK_IS_SPREAD_TABLE(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SPREAD_TABLE))
#define GTK_IS_SPREAD_TABLE_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SPREAD_TABLE))
#define GTK_SPREAD_TABLE_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_SPREAD_TABLE, GtkSpreadTableClass))

typedef struct _GtkSpreadTable            GtkSpreadTable;
typedef struct _GtkSpreadTablePrivate     GtkSpreadTablePrivate;
typedef struct _GtkSpreadTableClass       GtkSpreadTableClass;


struct _GtkSpreadTable
{
  GtkContainer parent_instance;

  /*< private >*/
  GtkSpreadTablePrivate *priv;
};

struct _GtkSpreadTableClass
{
  GtkContainerClass parent_class;
};


GType                 gtk_spread_table_get_type                  (void) G_GNUC_CONST;


GtkWidget            *gtk_spread_table_new                       (GtkOrientation  orientation,
								  guint           lines);

void                  gtk_spread_table_insert_child              (GtkSpreadTable *table,
								  GtkWidget      *child,
								  gint            index);

guint                 gtk_spread_table_get_child_line            (GtkSpreadTable *table,
								  GtkWidget      *child,
								  gint            size);

void                  gtk_spread_table_set_lines                 (GtkSpreadTable *table,
								  guint           lines);
guint                 gtk_spread_table_get_lines                 (GtkSpreadTable *table);

void                  gtk_spread_table_set_horizontal_spacing    (GtkSpreadTable *table,
								  guint           spacing);
guint                 gtk_spread_table_get_horizontal_spacing    (GtkSpreadTable *table);

void                  gtk_spread_table_set_vertical_spacing      (GtkSpreadTable *table,
								  guint           spacing);
guint                 gtk_spread_table_get_vertical_spacing      (GtkSpreadTable *table);




G_END_DECLS


#endif /* __GTK_SPREAD_TABLE_H__ */
