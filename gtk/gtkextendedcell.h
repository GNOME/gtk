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

#ifndef __GTK_EXTENDED_CELL_H__
#define __GTK_EXTENDED_CELL_H__

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_EXTENDED_CELL            (gtk_extended_cell_get_type ())
#define GTK_EXTENDED_CELL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_EXTENDED_CELL, GtkExtendedCell))
#define GTK_EXTENDED_CELL_CLASS(klass)    ((GtkExtendedCellIface*)g_type_interface_peek ((klass), GTK_TYPE_EXTENDED_CELL))
#define GTK_IS_EXTENDED_CELL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_EXTENDED_CELL))
#define GTK_EXTENDED_CELL_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GTK_TYPE_EXTENDED_CELL, GtkExtendedCellIface))

typedef struct _GtkExtendedCell           GtkExtendedCell;
typedef struct _GtkExtendedCellIface      GtkExtendedCellIface;

struct _GtkExtendedCellIface
{
  GTypeInterface g_iface;

  /* virtual table */

  void (*get_desired_size)     (GtkExtendedCell  *cell,
				GtkWidget        *widget,
                                GtkRequisition   *minimum_size,
                                GtkRequisition   *natural_size);
};

GType gtk_extended_cell_get_type             (void) G_GNUC_CONST;

void  gtk_extended_cell_get_desired_size     (GtkExtendedCell *cell,
					      GtkWidget       *widget,
					      GtkRequisition  *minimum_size,
					      GtkRequisition  *natural_size);



G_END_DECLS

#endif /* __GTK_EXTENDED_CELL_H__ */
