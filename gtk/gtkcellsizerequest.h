/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Openismus GmbH
 *
 * Author:
 *      Tristan Van Berkom <tristan.van.berkom@gmail.com>
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

#ifndef __GTK_CELL_SIZE_REQUEST_H__
#define __GTK_CELL_SIZE_REQUEST_H__

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_CELL_SIZE_REQUEST            (gtk_cell_size_request_get_type ())
#define GTK_CELL_SIZE_REQUEST(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_CELL_SIZE_REQUEST, GtkCellSizeRequest))
#define GTK_CELL_SIZE_REQUEST_CLASS(klass)    ((GtkCellSizeRequestIface*)g_type_interface_peek ((klass), GTK_TYPE_CELL_SIZE_REQUEST))
#define GTK_IS_CELL_SIZE_REQUEST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_CELL_SIZE_REQUEST))
#define GTK_CELL_SIZE_REQUEST_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GTK_TYPE_CELL_SIZE_REQUEST, GtkCellSizeRequestIface))

typedef struct _GtkCellSizeRequest           GtkCellSizeRequest;
typedef struct _GtkCellSizeRequestIface      GtkCellSizeRequestIface;

struct _GtkCellSizeRequestIface
{
  GTypeInterface g_iface;

  /* virtual table */
  GtkSizeRequestMode (* get_request_mode)     (GtkCellSizeRequest *cell);
  void               (* get_width)            (GtkCellSizeRequest *cell,
					       GtkWidget          *widget,
					       gint               *minimum_size,
					       gint               *natural_size);
  void               (* get_height_for_width) (GtkCellSizeRequest *cell,
					       GtkWidget          *widget,
					       gint                width,
					       gint               *minimum_height,
					       gint               *natural_height);
  void               (* get_height)           (GtkCellSizeRequest *cell,
					       GtkWidget          *widget,
					       gint               *minimum_size,
					       gint               *natural_size);
  void               (* get_width_for_height) (GtkCellSizeRequest *cell,
					       GtkWidget          *widget,
					       gint                height,
					       gint               *minimum_width,
					       gint               *natural_width);
};

GType              gtk_cell_size_request_get_type             (void) G_GNUC_CONST;
GtkSizeRequestMode gtk_cell_size_request_get_request_mode     (GtkCellSizeRequest *cell);
void               gtk_cell_size_request_get_width            (GtkCellSizeRequest *cell,
							       GtkWidget          *widget,
							       gint               *minimum_size,
							       gint               *natural_size);
void               gtk_cell_size_request_get_height_for_width (GtkCellSizeRequest *cell,
							       GtkWidget          *widget,
							       gint                width,
							       gint               *minimum_height,
							       gint               *natural_height);
void               gtk_cell_size_request_get_height           (GtkCellSizeRequest *cell,
							       GtkWidget          *widget,
							       gint               *minimum_size,
							       gint               *natural_size);
void               gtk_cell_size_request_get_width_for_height (GtkCellSizeRequest *cell,
							       GtkWidget          *widget,
							       gint                height,
							       gint               *minimum_width,
							       gint               *natural_width);
void               gtk_cell_size_request_get_size             (GtkCellSizeRequest *cell,
							       GtkWidget          *widget,
							       GtkRequisition     *minimum_size,
							       GtkRequisition     *natural_size);

G_END_DECLS

#endif /* __GTK_CELL_SIZE_REQUEST_H__ */
