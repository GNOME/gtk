/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <ctype.h>
#include <string.h>
#ifdef USE_XIM
#include "gdk/gdkx.h"
#endif
#include "gdk/gdkkeysyms.h"
#include "gdk/gdki18n.h"
#include "gtkeditable.h"
#include "gtkmain.h"
#include "gtkselection.h"
#include "gtksignal.h"

#define MIN_EDITABLE_WIDTH  150
#define DRAW_TIMEOUT     20
#define INNER_BORDER     2

enum {
  ACTIVATE,
  CHANGED,
  LAST_SIGNAL
};

static void gtk_editable_class_init          (GtkEditableClass     *klass);
static void gtk_editable_init                (GtkEditable          *editable);
static void gtk_editable_finalize            (GtkObject         *object);
static gint gtk_editable_selection_clear     (GtkWidget         *widget,
					   GdkEventSelection *event);
static void gtk_editable_selection_handler   (GtkWidget    *widget,
					   GtkSelectionData  *selection_data,
					   gpointer      data);
static void gtk_editable_selection_received  (GtkWidget         *widget,
					   GtkSelectionData  *selection_data);

static void gtk_editable_set_selection    (GtkEditable          *editable,
					   gint               start,
					   gint               end);

static GtkWidgetClass *parent_class = NULL;
static guint editable_signals[LAST_SIGNAL] = { 0 };
static GdkAtom ctext_atom = GDK_NONE;
static GdkAtom text_atom = GDK_NONE;
static GdkAtom clipboard_atom = GDK_NONE;

guint
gtk_editable_get_type ()
{
  static guint editable_type = 0;

  if (!editable_type)
    {
      GtkTypeInfo editable_info =
      {
	"GtkEditable",
	sizeof (GtkEditable),
	sizeof (GtkEditableClass),
	(GtkClassInitFunc) gtk_editable_class_init,
	(GtkObjectInitFunc) gtk_editable_init,
	(GtkArgSetFunc) NULL,
	(GtkArgGetFunc) NULL,
      };

      editable_type = gtk_type_unique (gtk_widget_get_type (), &editable_info);
    }

  return editable_type;
}

static void
gtk_editable_class_init (GtkEditableClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;

  parent_class = gtk_type_class (gtk_widget_get_type ());

  editable_signals[ACTIVATE] =
    gtk_signal_new ("activate",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkEditableClass, activate),
		    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);

  editable_signals[CHANGED] =
    gtk_signal_new ("changed",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkEditableClass, changed),
		    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);

  gtk_object_class_add_signals (object_class, editable_signals, LAST_SIGNAL);

  object_class->finalize = gtk_editable_finalize;

  widget_class->selection_clear_event = gtk_editable_selection_clear;
  widget_class->selection_received = gtk_editable_selection_received;

  class->insert_text = NULL;
  class->delete_text = NULL;
  class->update_text = NULL;
  class->get_chars = NULL;
  class->set_selection = NULL;
  class->changed = NULL;
}

static void
gtk_editable_init (GtkEditable *editable)
{
  GTK_WIDGET_SET_FLAGS (editable, GTK_CAN_FOCUS);

  editable->selection_start_pos = 0;
  editable->selection_end_pos = 0;
  editable->has_selection = FALSE;
  editable->editable = 1;
  editable->clipboard_text = NULL;

#ifdef USE_XIM
  editable->ic = NULL;
#endif

  if (!clipboard_atom)
    clipboard_atom = gdk_atom_intern ("CLIPBOARD", FALSE);

  gtk_selection_add_handler (GTK_WIDGET(editable), GDK_SELECTION_PRIMARY,
			     GDK_TARGET_STRING, gtk_editable_selection_handler,
			     NULL);
  gtk_selection_add_handler (GTK_WIDGET(editable), clipboard_atom,
			     GDK_TARGET_STRING, gtk_editable_selection_handler,
			     NULL);

  if (!text_atom)
    text_atom = gdk_atom_intern ("TEXT", FALSE);

  gtk_selection_add_handler (GTK_WIDGET(editable), GDK_SELECTION_PRIMARY,
			     text_atom,
			     gtk_editable_selection_handler,
			     NULL);
  gtk_selection_add_handler (GTK_WIDGET(editable), clipboard_atom,
			     text_atom,
			     gtk_editable_selection_handler,
			     NULL);

  if (!ctext_atom)
    ctext_atom = gdk_atom_intern ("COMPOUND_TEXT", FALSE);

  gtk_selection_add_handler (GTK_WIDGET(editable), GDK_SELECTION_PRIMARY,
			     ctext_atom,
			     gtk_editable_selection_handler,
			     NULL);
  gtk_selection_add_handler (GTK_WIDGET(editable), clipboard_atom,
			     ctext_atom,
			     gtk_editable_selection_handler,
			     NULL);
}

static void
gtk_editable_finalize (GtkObject *object)
{
  GtkEditable *editable;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_EDITABLE (object));

  editable = GTK_EDITABLE (object);

#ifdef USE_XIM
  if (editable->ic)
    {
      gdk_ic_destroy (editable->ic);
      editable->ic = NULL;
    }
#endif

  (* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}

void
gtk_editable_insert_text (GtkEditable *editable,
			  const gchar *new_text,
			  gint         new_text_length,
			  gint        *position)
{
  GtkEditableClass *klass;

  gchar buf[64];
  gchar *text;

  g_return_if_fail (editable != NULL);
  g_return_if_fail (GTK_IS_EDITABLE (editable));

  klass = GTK_EDITABLE_CLASS (GTK_OBJECT (editable)->klass);

  if (new_text_length <= 64)
    text = buf;
  else
    text = g_new (gchar, new_text_length);

  strncpy (text, new_text, new_text_length);

  klass->insert_text (editable, text, new_text_length, position);
  gtk_signal_emit (GTK_OBJECT (editable), editable_signals[CHANGED]);

  if (new_text_length > 64)
    g_free (text);
}

void
gtk_editable_delete_text (GtkEditable *editable,
			  gint         start_pos,
			  gint         end_pos)
{
  GtkEditableClass *klass;

  g_return_if_fail (editable != NULL);
  g_return_if_fail (GTK_IS_EDITABLE (editable));

  klass = GTK_EDITABLE_CLASS (GTK_OBJECT (editable)->klass);

  klass->delete_text (editable, start_pos, end_pos);
  gtk_signal_emit (GTK_OBJECT (editable), editable_signals[CHANGED]);
}

static void
gtk_editable_update_text (GtkEditable *editable,
		       gint      start_pos,
		       gint      end_pos)
{
  GtkEditableClass *klass;

  g_return_if_fail (editable != NULL);
  g_return_if_fail (GTK_IS_EDITABLE (editable));

  klass = GTK_EDITABLE_CLASS (GTK_OBJECT (editable)->klass);

  klass->update_text (editable, start_pos, end_pos);
}

gchar *    
gtk_editable_get_chars      (GtkEditable      *editable,
			     gint              start,
			     gint              end)
{
  GtkEditableClass *klass;

  g_return_val_if_fail (editable != NULL, NULL);
  g_return_val_if_fail (GTK_IS_EDITABLE (editable), NULL);

  klass = GTK_EDITABLE_CLASS (GTK_OBJECT (editable)->klass);

  return klass->get_chars (editable, start, end);
}

static void
gtk_editable_set_selection (GtkEditable *editable,
			    gint      start_pos,
			    gint      end_pos)
{
  GtkEditableClass *klass;

  g_return_if_fail (editable != NULL);
  g_return_if_fail (GTK_IS_EDITABLE (editable));

  klass = GTK_EDITABLE_CLASS (GTK_OBJECT (editable)->klass);

  klass->set_selection (editable, start_pos, end_pos);
}

static gint
gtk_editable_selection_clear (GtkWidget         *widget,
			      GdkEventSelection *event)
{
  GtkEditable *editable;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_EDITABLE (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  /* Let the selection handling code know that the selection
   * has been changed, since we've overriden the default handler */
  if (!gtk_selection_clear (widget, event))
    return FALSE;
  
  editable = GTK_EDITABLE (widget);
  
  if (event->selection == GDK_SELECTION_PRIMARY)
    {
      if (editable->has_selection)
	{
	  editable->has_selection = FALSE;
	  gtk_editable_update_text (editable, editable->selection_start_pos,
				    editable->selection_end_pos);
	}
    }
  else if (event->selection == clipboard_atom)
    {
      g_free (editable->clipboard_text);
      editable->clipboard_text = NULL;
    }
  
  return TRUE;
}

static void
gtk_editable_selection_handler (GtkWidget        *widget,
				GtkSelectionData *selection_data,
				gpointer          data)
{
  GtkEditable *editable;
  gint selection_start_pos;
  gint selection_end_pos;

  gchar *str;
  gint length;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_EDITABLE (widget));

  editable = GTK_EDITABLE (widget);

  if (selection_data->selection == GDK_SELECTION_PRIMARY)
    {
      selection_start_pos = MIN (editable->selection_start_pos, editable->selection_end_pos);
      selection_end_pos = MAX (editable->selection_start_pos, editable->selection_end_pos);
      str = gtk_editable_get_chars(editable, 
				   selection_start_pos, 
				   selection_end_pos);
      length = selection_end_pos - selection_start_pos;
    }
  else				/* CLIPBOARD */
    {
      if (!editable->clipboard_text)
	return;			/* Refuse */

      str = editable->clipboard_text;
      length = strlen (editable->clipboard_text);
    }
  
  if (selection_data->target == GDK_SELECTION_TYPE_STRING)
    {
      gtk_selection_data_set (selection_data,
                              GDK_SELECTION_TYPE_STRING,
                              8*sizeof(gchar), str, length);
    }
  else if (selection_data->target == text_atom ||
           selection_data->target == ctext_atom)
    {
      guchar *text;
      gchar c;
      GdkAtom encoding;
      gint format;
      gint new_length;

      c = str[length];
      str[length] = '\0';
      gdk_string_to_compound_text (str, &encoding, &format, &text, &new_length);
      gtk_selection_data_set (selection_data, encoding, format, text, new_length);
      gdk_free_compound_text (text);
      str[length] = c;
    }

  if (str != editable->clipboard_text)
    g_free (str);
}

static void
gtk_editable_selection_received  (GtkWidget         *widget,
				  GtkSelectionData  *selection_data)
{
  GtkEditable *editable;
  gint reselect;
  gint old_pos;
  gint tmp_pos;
  enum {INVALID, STRING, CTEXT} type;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_EDITABLE (widget));

  editable = GTK_EDITABLE (widget);

  if (selection_data->type == GDK_TARGET_STRING)
    type = STRING;
  else if (selection_data->type == ctext_atom)
    type = CTEXT;
  else
    type = INVALID;

  if (type == INVALID || selection_data->length < 0)
    {
    /* avoid infinite loop */
    if (selection_data->target != GDK_TARGET_STRING)
      gtk_selection_convert (widget, selection_data->selection,
			     GDK_TARGET_STRING, GDK_CURRENT_TIME);
    return;
  }

  reselect = FALSE;

  if ((editable->selection_start_pos != editable->selection_end_pos) && 
      (!editable->has_selection || 
       (selection_data->selection == clipboard_atom)))
    {
      reselect = TRUE;

      /* Don't want to call gtk_editable_delete_selection here if we are going
       * to reclaim the selection to avoid extra server traffic */
      if (editable->has_selection)
	{
	  gtk_editable_delete_text (editable,
				 MIN (editable->selection_start_pos, editable->selection_end_pos),
				 MAX (editable->selection_start_pos, editable->selection_end_pos));
	}
      else
	gtk_editable_delete_selection (editable);
    }

  tmp_pos = old_pos = editable->current_pos;

  switch (type)
    {
    case STRING:
      selection_data->data[selection_data->length] = 0;
      gtk_editable_insert_text (editable, selection_data->data,
				strlen (selection_data->data), &tmp_pos);
      editable->current_pos = tmp_pos;
      break;
    case CTEXT:
      {
	gchar **list;
	gint count;
	gint i;

	count = gdk_text_property_to_text_list (selection_data->type,
						selection_data->format, 
	      					selection_data->data,
						selection_data->length,
						&list);
	for (i=0; i<count; i++) 
	  {
	    gtk_editable_insert_text (editable, list[i], strlen (list[i]), &tmp_pos);
	    editable->current_pos = tmp_pos;
	  }
	if (count > 0)
	  gdk_free_text_list (list);
      }
      break;
    case INVALID:		/* quiet compiler */
      break;
    }

  if (reselect)
    gtk_editable_set_selection (editable, old_pos, editable->current_pos);
}

void
gtk_editable_delete_selection (GtkEditable *editable)
{
  guint start;
  guint end;

  if (!editable->editable)
    return;

  start = editable->selection_start_pos;
  end = editable->selection_end_pos;

  editable->selection_start_pos = 0;
  editable->selection_end_pos = 0;

  if (start != end)
    gtk_editable_delete_text (editable, MIN (start, end), MAX (start,end));

  if (editable->has_selection)
    {
      editable->has_selection = FALSE;
      if (gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == GTK_WIDGET (editable)->window)
	gtk_selection_owner_set (NULL, GDK_SELECTION_PRIMARY, GDK_CURRENT_TIME);
    }
}

void
gtk_editable_claim_selection (GtkEditable *editable, 
			      gboolean  claim, 
			      guint32   time)
{
  g_return_if_fail (GTK_WIDGET_REALIZED (editable));

  editable->has_selection = FALSE;
  
  if (claim)
    {
      if (gtk_selection_owner_set (GTK_WIDGET(editable), GDK_SELECTION_PRIMARY, time))
	editable->has_selection = TRUE;
    }
  else
    {
      if (gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == 
	  GTK_WIDGET(editable)->window)
	gtk_selection_owner_set (NULL, GDK_SELECTION_PRIMARY, time);
    }
}

void
gtk_editable_select_region (GtkEditable *editable,
			    gint         start,
			    gint         end)
{
  if (GTK_WIDGET_REALIZED (editable))
    gtk_editable_claim_selection (editable, start != end, GDK_CURRENT_TIME);

  gtk_editable_set_selection (editable, start, end);
}

void
gtk_editable_cut_clipboard (GtkEditable *editable, guint32 time)
{
  gtk_editable_copy_clipboard (editable, time);
  gtk_editable_delete_selection (editable);
}

void
gtk_editable_copy_clipboard (GtkEditable *editable, guint32 time)
{
  gint selection_start_pos; 
  gint selection_end_pos;

  selection_start_pos = MIN (editable->selection_start_pos, editable->selection_end_pos);
  selection_end_pos = MAX (editable->selection_start_pos, editable->selection_end_pos);
 
  if (selection_start_pos != selection_end_pos)
    {
      if (gtk_selection_owner_set (GTK_WIDGET (editable),
				   clipboard_atom,
				   time))
	editable->clipboard_text = gtk_editable_get_chars (editable,
							   selection_start_pos,
							   selection_end_pos);
    }
}

void
gtk_editable_paste_clipboard (GtkEditable *editable, guint32 time)
{
  if (editable->editable)
    gtk_selection_convert (GTK_WIDGET(editable), 
			   clipboard_atom, ctext_atom, time);
}

void
gtk_editable_changed (GtkEditable *editable)
{
  gtk_signal_emit (GTK_OBJECT (editable), editable_signals[CHANGED]);
}
