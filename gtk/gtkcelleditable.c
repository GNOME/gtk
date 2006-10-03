/* gtkcelleditable.c
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
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


#include <config.h>
#include "gtkcelleditable.h"
#include "gtkmarshalers.h"
#include "gtkintl.h"
#include "gtkalias.h"

static void gtk_cell_editable_base_init (gpointer g_class);

GType
gtk_cell_editable_get_type (void)
{
  static GType cell_editable_type = 0;

  if (! cell_editable_type)
    {
      const GTypeInfo cell_editable_info =
      {
	sizeof (GtkCellEditableIface), /* class_size */
	gtk_cell_editable_base_init,   /* base_init */
	NULL,		/* base_finalize */
	NULL,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	0,
	0,
	NULL
      };

      cell_editable_type =
	g_type_register_static (G_TYPE_INTERFACE, I_("GtkCellEditable"),
				&cell_editable_info, 0);

      g_type_interface_add_prerequisite (cell_editable_type, GTK_TYPE_WIDGET);
    }

  return cell_editable_type;
}

static void
gtk_cell_editable_base_init (gpointer g_class)
{
  static gboolean initialized = FALSE;

  if (! initialized)
    {
      g_signal_new (I_("editing_done"),
                    GTK_TYPE_CELL_EDITABLE,
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GtkCellEditableIface, editing_done),
                    NULL, NULL,
                    _gtk_marshal_VOID__VOID,
                    G_TYPE_NONE, 0);
      g_signal_new (I_("remove_widget"),
                    GTK_TYPE_CELL_EDITABLE,
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GtkCellEditableIface, remove_widget),
                    NULL, NULL,
                    _gtk_marshal_VOID__VOID,
                    G_TYPE_NONE, 0);
      initialized = TRUE;
    }
}

/**
 * gtk_cell_editable_start_editing:
 * @cell_editable: A #GtkCellEditable
 * @event: A #GdkEvent, or %NULL
 * 
 * Begins editing on a @cell_editable.  @event is the #GdkEvent that began the
 * editing process.  It may be %NULL, in the instance that editing was initiated
 * through programatic means.
 **/
void
gtk_cell_editable_start_editing (GtkCellEditable *cell_editable,
				 GdkEvent        *event)
{
  g_return_if_fail (GTK_IS_CELL_EDITABLE (cell_editable));

  (* GTK_CELL_EDITABLE_GET_IFACE (cell_editable)->start_editing) (cell_editable, event);
}

/**
 * gtk_cell_editable_editing_done:
 * @cell_editable: A #GtkTreeEditable
 * 
 * Emits the "editing_done" signal.  This signal is a sign for the cell renderer
 * to update its value from the cell.
 **/
void
gtk_cell_editable_editing_done (GtkCellEditable *cell_editable)
{
  g_return_if_fail (GTK_IS_CELL_EDITABLE (cell_editable));

  g_signal_emit_by_name (cell_editable, "editing_done");
}

/**
 * gtk_cell_editable_remove_widget:
 * @cell_editable: A #GtkTreeEditable
 * 
 * Emits the "remove_widget" signal.  This signal is meant to indicate that the
 * cell is finished editing, and the widget may now be destroyed.
 **/
void
gtk_cell_editable_remove_widget (GtkCellEditable *cell_editable)
{
  g_return_if_fail (GTK_IS_CELL_EDITABLE (cell_editable));

  g_signal_emit_by_name (cell_editable, "remove_widget");
}

#define __GTK_CELL_EDITABLE_C__
#include "gtkaliasdef.c"
