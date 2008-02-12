/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <config.h>
#include <string.h>

#include "gtkeditable.h"
#include "gtkmarshalers.h"
#include "gtkintl.h"
#include "gtkalias.h"


static void gtk_editable_base_init (gpointer g_class);


GType
gtk_editable_get_type (void)
{
  static GType editable_type = 0;

  if (!editable_type)
    {
      const GTypeInfo editable_info =
      {
	sizeof (GtkEditableClass),  /* class_size */
	gtk_editable_base_init,	    /* base_init */
	NULL,			    /* base_finalize */
      };

      editable_type = g_type_register_static (G_TYPE_INTERFACE, I_("GtkEditable"),
					      &editable_info, 0);
    }

  return editable_type;
}

static void
gtk_editable_base_init (gpointer g_class)
{
  static gboolean initialized = FALSE;

  if (! initialized)
    {
      g_signal_new (I_("insert_text"),
		    GTK_TYPE_EDITABLE,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkEditableClass, insert_text),
		    NULL, NULL,
		    _gtk_marshal_VOID__STRING_INT_POINTER,
		    G_TYPE_NONE, 3,
		    G_TYPE_STRING,
		    G_TYPE_INT,
		    G_TYPE_POINTER);
      g_signal_new (I_("delete_text"),
		    GTK_TYPE_EDITABLE,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkEditableClass, delete_text),
		    NULL, NULL,
		    _gtk_marshal_VOID__INT_INT,
		    G_TYPE_NONE, 2,
		    G_TYPE_INT,
		    G_TYPE_INT);
      g_signal_new (I_("changed"),
		    GTK_TYPE_EDITABLE,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkEditableClass, changed),
		    NULL, NULL,
		    _gtk_marshal_VOID__VOID,
		    G_TYPE_NONE, 0);

      initialized = TRUE;
    }
}

void
gtk_editable_insert_text (GtkEditable *editable,
			  const gchar *new_text,
			  gint         new_text_length,
			  gint        *position)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));
  g_return_if_fail (position != NULL);

  if (new_text_length < 0)
    new_text_length = strlen (new_text);
  
  GTK_EDITABLE_GET_CLASS (editable)->do_insert_text (editable, new_text, new_text_length, position);
}

void
gtk_editable_delete_text (GtkEditable *editable,
			  gint         start_pos,
			  gint         end_pos)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));

  GTK_EDITABLE_GET_CLASS (editable)->do_delete_text (editable, start_pos, end_pos);
}

gchar *    
gtk_editable_get_chars (GtkEditable *editable,
			gint         start,
			gint         end)
{
  g_return_val_if_fail (GTK_IS_EDITABLE (editable), NULL);

  return GTK_EDITABLE_GET_CLASS (editable)->get_chars (editable, start, end);
}

void
gtk_editable_set_position (GtkEditable      *editable,
			   gint              position)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));

  GTK_EDITABLE_GET_CLASS (editable)->set_position (editable, position);
}

gint
gtk_editable_get_position (GtkEditable      *editable)
{
  g_return_val_if_fail (GTK_IS_EDITABLE (editable), 0);

  return GTK_EDITABLE_GET_CLASS (editable)->get_position (editable);
}

gboolean
gtk_editable_get_selection_bounds (GtkEditable *editable,
				   gint        *start_pos,
				   gint        *end_pos)
{
  gint tmp_start, tmp_end;
  gboolean result;
  
  g_return_val_if_fail (GTK_IS_EDITABLE (editable), FALSE);

  result = GTK_EDITABLE_GET_CLASS (editable)->get_selection_bounds (editable, &tmp_start, &tmp_end);

  if (start_pos)
    *start_pos = MIN (tmp_start, tmp_end);
  if (end_pos)
    *end_pos = MAX (tmp_start, tmp_end);

  return result;
}

void
gtk_editable_delete_selection (GtkEditable *editable)
{
  gint start, end;

  g_return_if_fail (GTK_IS_EDITABLE (editable));

  if (gtk_editable_get_selection_bounds (editable, &start, &end))
    gtk_editable_delete_text (editable, start, end);
}

void
gtk_editable_select_region (GtkEditable *editable,
			    gint         start,
			    gint         end)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));
  
  GTK_EDITABLE_GET_CLASS (editable)->set_selection_bounds (editable,  start, end);
}

void
gtk_editable_cut_clipboard (GtkEditable *editable)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));
  
  g_signal_emit_by_name (editable, "cut_clipboard");
}

void
gtk_editable_copy_clipboard (GtkEditable *editable)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));
  
  g_signal_emit_by_name (editable, "copy_clipboard");
}

void
gtk_editable_paste_clipboard (GtkEditable *editable)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));
  
  g_signal_emit_by_name (editable, "paste_clipboard");
}

void
gtk_editable_set_editable (GtkEditable    *editable,
			   gboolean        is_editable)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));

  g_object_set (editable,
		"editable", is_editable != FALSE,
		NULL);
}

/**
 * gtk_editable_get_editable:
 * @editable: a #GtkEditable
 *
 * Retrieves whether @editable is editable. See
 * gtk_editable_set_editable().
 *
 * Return value: %TRUE if @editable is editable.
 **/
gboolean
gtk_editable_get_editable (GtkEditable *editable)
{
  gboolean value;

  g_return_val_if_fail (GTK_IS_EDITABLE (editable), FALSE);

  g_object_get (editable, "editable", &value, NULL);

  return value;
}

#define __GTK_EDITABLE_C__
#include "gtkaliasdef.c"
