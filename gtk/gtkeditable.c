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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
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
  CHANGED,
  INSERT_TEXT,
  DELETE_TEXT,
  /* Binding actions */
  ACTIVATE,
  SET_EDITABLE,
  MOVE_CURSOR,
  MOVE_WORD,
  MOVE_PAGE,
  MOVE_TO_ROW,
  MOVE_TO_COLUMN,
  KILL_CHAR,
  KILL_WORD,
  KILL_LINE,
  CUT_CLIPBOARD,
  COPY_CLIPBOARD,
  PASTE_CLIPBOARD,
  LAST_SIGNAL
};

enum {
  ARG_0,
  ARG_TEXT_POSITION,
  ARG_EDITABLE
};

/* values for selection info */

enum {
  TARGET_STRING,
  TARGET_TEXT,
  TARGET_COMPOUND_TEXT
};

static void gtk_editable_class_init          (GtkEditableClass *klass);
static void gtk_editable_init                (GtkEditable      *editable);
static void gtk_editable_set_arg	     (GtkObject        *object,
					      GtkArg           *arg,
					      guint             arg_id);
static void gtk_editable_get_arg	     (GtkObject        *object,
					      GtkArg           *arg,
					      guint             arg_id);
static void *gtk_editable_get_public_chars   (GtkEditable      *editable,
					      gint              start,
					      gint              end);
static gint gtk_editable_selection_clear     (GtkWidget        *widget,
					     GdkEventSelection *event);
static void gtk_editable_selection_get      (GtkWidget         *widget,
					     GtkSelectionData  *selection_data,
					     guint              info,
					     guint              time);
static void gtk_editable_selection_received  (GtkWidget        *widget,
					      GtkSelectionData *selection_data,
					      guint             time);

static void gtk_editable_set_selection    (GtkEditable      *editable,
					   gint              start,
					   gint              end);
static guint32 gtk_editable_get_event_time (GtkEditable     *editable);

static void gtk_editable_real_cut_clipboard   (GtkEditable     *editable);
static void gtk_editable_real_copy_clipboard  (GtkEditable     *editable);
static void gtk_editable_real_paste_clipboard (GtkEditable     *editable);
static void gtk_editable_real_set_editable    (GtkEditable     *editable,
					       gboolean         is_editable);
     
static GtkWidgetClass *parent_class = NULL;
static guint editable_signals[LAST_SIGNAL] = { 0 };

static GdkAtom clipboard_atom = GDK_NONE;

GtkType
gtk_editable_get_type (void)
{
  static GtkType editable_type = 0;

  if (!editable_type)
    {
      static const GtkTypeInfo editable_info =
      {
	"GtkEditable",
	sizeof (GtkEditable),
	sizeof (GtkEditableClass),
	(GtkClassInitFunc) gtk_editable_class_init,
	(GtkObjectInitFunc) gtk_editable_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      editable_type = gtk_type_unique (GTK_TYPE_WIDGET, &editable_info);
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

  parent_class = gtk_type_class (GTK_TYPE_WIDGET);

  editable_signals[CHANGED] =
    gtk_signal_new ("changed",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkEditableClass, changed),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);

  editable_signals[INSERT_TEXT] =
    gtk_signal_new ("insert_text",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkEditableClass, insert_text),
		    gtk_marshal_NONE__POINTER_INT_POINTER,
		    GTK_TYPE_NONE,
		    3,
		    GTK_TYPE_STRING,
		    GTK_TYPE_INT,
		    GTK_TYPE_POINTER);

  editable_signals[DELETE_TEXT] =
    gtk_signal_new ("delete_text",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkEditableClass, delete_text),
		    gtk_marshal_NONE__INT_INT,
		    GTK_TYPE_NONE,
		    2,
		    GTK_TYPE_INT,
		    GTK_TYPE_INT);		    

  editable_signals[ACTIVATE] =
    gtk_signal_new ("activate",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkEditableClass, activate),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  widget_class->activate_signal = editable_signals[ACTIVATE];

  editable_signals[SET_EDITABLE] =
    gtk_signal_new ("set-editable",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkEditableClass, set_editable),
		    gtk_marshal_NONE__BOOL,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_BOOL);

  editable_signals[MOVE_CURSOR] =
    gtk_signal_new ("move_cursor",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkEditableClass, move_cursor),
		    gtk_marshal_NONE__INT_INT,
		    GTK_TYPE_NONE, 2, 
		    GTK_TYPE_INT, 
		    GTK_TYPE_INT);

  editable_signals[MOVE_WORD] =
    gtk_signal_new ("move_word",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkEditableClass, move_word),
		    gtk_marshal_NONE__INT,
		    GTK_TYPE_NONE, 1, 
		    GTK_TYPE_INT);

  editable_signals[MOVE_PAGE] =
    gtk_signal_new ("move_page",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkEditableClass, move_page),
		    gtk_marshal_NONE__INT_INT,
		    GTK_TYPE_NONE, 2, 
		    GTK_TYPE_INT, 
		    GTK_TYPE_INT);

  editable_signals[MOVE_TO_ROW] =
    gtk_signal_new ("move_to_row",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkEditableClass, move_to_row),
		    gtk_marshal_NONE__INT,
		    GTK_TYPE_NONE, 1, 
		    GTK_TYPE_INT);

  editable_signals[MOVE_TO_COLUMN] =
    gtk_signal_new ("move_to_column",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkEditableClass, move_to_column),
		    gtk_marshal_NONE__INT,
		    GTK_TYPE_NONE, 1, 
		    GTK_TYPE_INT);

  editable_signals[KILL_CHAR] =
    gtk_signal_new ("kill_char",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkEditableClass, kill_char),
		    gtk_marshal_NONE__INT,
		    GTK_TYPE_NONE, 1, 
		    GTK_TYPE_INT);

  editable_signals[KILL_WORD] =
    gtk_signal_new ("kill_word",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkEditableClass, kill_word),
		    gtk_marshal_NONE__INT,
		    GTK_TYPE_NONE, 1, 
		    GTK_TYPE_INT);

  editable_signals[KILL_LINE] =
    gtk_signal_new ("kill_line",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkEditableClass, kill_line),
		    gtk_marshal_NONE__INT,
		    GTK_TYPE_NONE, 1, 
		    GTK_TYPE_INT);

  editable_signals[CUT_CLIPBOARD] =
    gtk_signal_new ("cut_clipboard",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkEditableClass, cut_clipboard),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);

  editable_signals[COPY_CLIPBOARD] =
    gtk_signal_new ("copy_clipboard",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkEditableClass, copy_clipboard),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);

  editable_signals[PASTE_CLIPBOARD] =
    gtk_signal_new ("paste_clipboard",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkEditableClass, paste_clipboard),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);

  gtk_object_class_add_signals (object_class, editable_signals, LAST_SIGNAL);

  gtk_object_add_arg_type ("GtkEditable::text_position", GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_TEXT_POSITION);
  gtk_object_add_arg_type ("GtkEditable::editable", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_EDITABLE);
    
  object_class->set_arg = gtk_editable_set_arg;
  object_class->get_arg = gtk_editable_get_arg;

  widget_class->selection_clear_event = gtk_editable_selection_clear;
  widget_class->selection_received = gtk_editable_selection_received;
  widget_class->selection_get = gtk_editable_selection_get;

  class->insert_text = NULL;
  class->delete_text = NULL;

  class->activate = NULL;
  class->set_editable = gtk_editable_real_set_editable;

  class->move_cursor = NULL;
  class->move_word = NULL;
  class->move_page = NULL;
  class->move_to_row = NULL;
  class->move_to_column = NULL;

  class->kill_char = NULL;
  class->kill_word = NULL;
  class->kill_line = NULL;

  class->cut_clipboard = gtk_editable_real_cut_clipboard;
  class->copy_clipboard = gtk_editable_real_copy_clipboard;
  class->paste_clipboard = gtk_editable_real_paste_clipboard;

  class->update_text = NULL;
  class->get_chars = NULL;
  class->set_selection = NULL;
  class->set_position = NULL;
}

static void
gtk_editable_set_arg (GtkObject      *object,
		      GtkArg         *arg,
		      guint           arg_id)
{
  GtkEditable *editable;

  editable = GTK_EDITABLE (object);

  switch (arg_id)
    {
    case ARG_TEXT_POSITION:
      gtk_editable_set_position (editable, GTK_VALUE_INT (*arg));
      break;
    case ARG_EDITABLE:
      gtk_editable_set_editable (editable, GTK_VALUE_BOOL (*arg));
      break;
    default:
      break;
    }
}

static void
gtk_editable_get_arg (GtkObject      *object,
		      GtkArg         *arg,
		      guint           arg_id)
{
  GtkEditable *editable;

  editable = GTK_EDITABLE (object);

  switch (arg_id)
    {
    case ARG_TEXT_POSITION:
      GTK_VALUE_INT (*arg) = editable->current_pos;
      break;
    case ARG_EDITABLE:
      GTK_VALUE_BOOL (*arg) = editable->editable;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

static void
gtk_editable_init (GtkEditable *editable)
{
  static const GtkTargetEntry targets[] = {
    { "STRING", 0, TARGET_STRING },
    { "TEXT",   0, TARGET_TEXT }, 
    { "COMPOUND_TEXT", 0, TARGET_COMPOUND_TEXT }
  };
  static const gint n_targets = sizeof(targets) / sizeof(targets[0]);
  
  GTK_WIDGET_SET_FLAGS (editable, GTK_CAN_FOCUS);

  editable->selection_start_pos = 0;
  editable->selection_end_pos = 0;
  editable->has_selection = FALSE;
  editable->editable = 1;
  editable->visible = 1;
  editable->clipboard_text = NULL;

#ifdef USE_XIM
  editable->ic = NULL;
#endif

  if (!clipboard_atom)
    clipboard_atom = gdk_atom_intern ("CLIPBOARD", FALSE);

  gtk_selection_add_targets (GTK_WIDGET (editable), GDK_SELECTION_PRIMARY,
			     targets, n_targets);
  gtk_selection_add_targets (GTK_WIDGET (editable), clipboard_atom,
			     targets, n_targets);
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

  gtk_widget_ref (GTK_WIDGET (editable));

  klass = GTK_EDITABLE_CLASS (GTK_OBJECT (editable)->klass);

  if (new_text_length <= 64)
    text = buf;
  else
    text = g_new (gchar, new_text_length);

  strncpy (text, new_text, new_text_length);

  gtk_signal_emit (GTK_OBJECT (editable), editable_signals[INSERT_TEXT], text, new_text_length, position);
  gtk_signal_emit (GTK_OBJECT (editable), editable_signals[CHANGED]);

  if (new_text_length > 64)
    g_free (text);

  gtk_widget_unref (GTK_WIDGET (editable));
}

void
gtk_editable_delete_text (GtkEditable *editable,
			  gint         start_pos,
			  gint         end_pos)
{
  GtkEditableClass *klass;

  g_return_if_fail (editable != NULL);
  g_return_if_fail (GTK_IS_EDITABLE (editable));

  gtk_widget_ref (GTK_WIDGET (editable));

  klass = GTK_EDITABLE_CLASS (GTK_OBJECT (editable)->klass);

  gtk_signal_emit (GTK_OBJECT (editable), editable_signals[DELETE_TEXT], start_pos, end_pos);
  gtk_signal_emit (GTK_OBJECT (editable), editable_signals[CHANGED]);

  gtk_widget_unref (GTK_WIDGET (editable));
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

/*
 * Like gtk_editable_get_chars, but if the editable is not
 * visible, return asterisks
 */
static void *    
gtk_editable_get_public_chars (GtkEditable      *editable,
			       gint              start,
			       gint              end)
{
  if (editable->visible)
    return gtk_editable_get_chars (editable, start, end);
  else
    {
      gint i;
      gint nchars = end - start;
      gchar *str;
       
      if (nchars < 0)
	nchars = -nchars;

      str = g_new (gchar, nchars + 1);
      for (i = 0; i<nchars; i++)
	str[i] = '*';
      str[i] = '\0';

      return str;
    }
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

void
gtk_editable_set_position (GtkEditable      *editable,
			   gint              position)
{
  GtkEditableClass *klass;

  g_return_if_fail (editable != NULL);
  g_return_if_fail (GTK_IS_EDITABLE (editable));

  klass = GTK_EDITABLE_CLASS (GTK_OBJECT (editable)->klass);

  klass->set_position (editable, position);
}

gint
gtk_editable_get_position (GtkEditable      *editable)
{
  g_return_val_if_fail (editable != NULL, -1);
  g_return_val_if_fail (GTK_IS_EDITABLE (editable), -1);

  return editable->current_pos;
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
gtk_editable_selection_get (GtkWidget        *widget,
			    GtkSelectionData *selection_data,
			    guint             info,
			    guint             time)
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
      str = gtk_editable_get_public_chars(editable, 
					  selection_start_pos, 
					  selection_end_pos);
      if (!str)
	 return;		/* Refuse */
      length = strlen (str);
    }
  else				/* CLIPBOARD */
    {
      if (!editable->clipboard_text)
	return;			/* Refuse */

      str = editable->clipboard_text;
      length = strlen (editable->clipboard_text);
    }
  
  if (info == TARGET_STRING)
    {
      gtk_selection_data_set (selection_data,
                              GDK_SELECTION_TYPE_STRING,
                              8*sizeof(gchar), (guchar *)str, length);
    }
  else if ((info == TARGET_TEXT) || (info == TARGET_COMPOUND_TEXT))
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
				  GtkSelectionData  *selection_data,
				  guint              time)
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
  else if ((selection_data->type == gdk_atom_intern ("COMPOUND_TEXT", FALSE)) ||
	   (selection_data->type == gdk_atom_intern ("TEXT", FALSE)))
    type = CTEXT;
  else
    type = INVALID;

  if (type == INVALID || selection_data->length < 0)
    {
    /* avoid infinite loop */
    if (selection_data->target != GDK_TARGET_STRING)
      gtk_selection_convert (widget, selection_data->selection,
			     GDK_TARGET_STRING, time);
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
      gtk_editable_insert_text (editable, (gchar *)selection_data->data,
				strlen ((gchar *)selection_data->data), 
				&tmp_pos);
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

  g_return_if_fail (editable != NULL);
  g_return_if_fail (GTK_IS_EDITABLE (editable));

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
  g_return_if_fail (editable != NULL);
  g_return_if_fail (GTK_IS_EDITABLE (editable));
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
  g_return_if_fail (editable != NULL);
  g_return_if_fail (GTK_IS_EDITABLE (editable));
  
  if (GTK_WIDGET_REALIZED (editable))
    gtk_editable_claim_selection (editable, start != end, GDK_CURRENT_TIME);
  
  gtk_editable_set_selection (editable, start, end);
}

/* Get the timestamp of the current event. Actually, the only thing
 * we really care about below is the key event
 */
static guint32
gtk_editable_get_event_time (GtkEditable *editable)
{
  GdkEvent *event;
  guint32 tm = GDK_CURRENT_TIME;
  
  event = gtk_get_current_event();
  
  if (event)
    {
      switch (event->type)
	{
	case GDK_MOTION_NOTIFY:
	  tm = event->motion.time; break;
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
	  tm = event->button.time; break;
	case GDK_KEY_PRESS:
	case GDK_KEY_RELEASE:
	  tm = event->key.time; break;
	case GDK_ENTER_NOTIFY:
	case GDK_LEAVE_NOTIFY:
	  tm = event->crossing.time; break;
	case GDK_PROPERTY_NOTIFY:
	  tm = event->property.time; break;
	case GDK_SELECTION_CLEAR:
	case GDK_SELECTION_REQUEST:
	case GDK_SELECTION_NOTIFY:
	  tm = event->selection.time; break;
	case GDK_PROXIMITY_IN:
	case GDK_PROXIMITY_OUT:
	  tm = event->proximity.time; break;
	default:			/* use current time */
	  break;
	}
      
      gdk_event_free(event);
    }
  
  return tm;
}

void
gtk_editable_cut_clipboard (GtkEditable *editable)
{
  g_return_if_fail (editable != NULL);
  g_return_if_fail (GTK_IS_EDITABLE (editable));
  
  gtk_signal_emit (GTK_OBJECT (editable), editable_signals[CUT_CLIPBOARD]);
}

void
gtk_editable_copy_clipboard (GtkEditable *editable)
{
  g_return_if_fail (editable != NULL);
  g_return_if_fail (GTK_IS_EDITABLE (editable));
  
  gtk_signal_emit (GTK_OBJECT (editable), editable_signals[COPY_CLIPBOARD]);
}

void
gtk_editable_paste_clipboard (GtkEditable *editable)
{
  g_return_if_fail (editable != NULL);
  g_return_if_fail (GTK_IS_EDITABLE (editable));
  
  gtk_signal_emit (GTK_OBJECT (editable), editable_signals[PASTE_CLIPBOARD]);
}

void
gtk_editable_set_editable (GtkEditable    *editable,
			   gboolean        is_editable)
{
  g_return_if_fail (editable != NULL);
  g_return_if_fail (GTK_IS_EDITABLE (editable));
  
  gtk_signal_emit (GTK_OBJECT (editable), editable_signals[SET_EDITABLE], is_editable != FALSE);
}

static void
gtk_editable_real_set_editable (GtkEditable    *editable,
				gboolean        is_editable)
{
  g_return_if_fail (editable != NULL);
  g_return_if_fail (GTK_IS_EDITABLE (editable));

  editable->editable = is_editable != FALSE;
  gtk_widget_queue_draw (GTK_WIDGET (editable));
}

static void
gtk_editable_real_cut_clipboard (GtkEditable *editable)
{
  g_return_if_fail (editable != NULL);
  g_return_if_fail (GTK_IS_EDITABLE (editable));
  
  gtk_editable_real_copy_clipboard (editable);
  gtk_editable_delete_selection (editable);
}

static void
gtk_editable_real_copy_clipboard (GtkEditable *editable)
{
  guint32 time;
  gint selection_start_pos; 
  gint selection_end_pos;

  g_return_if_fail (editable != NULL);
  g_return_if_fail (GTK_IS_EDITABLE (editable));
  
  time = gtk_editable_get_event_time (editable);
  selection_start_pos = MIN (editable->selection_start_pos, editable->selection_end_pos);
  selection_end_pos = MAX (editable->selection_start_pos, editable->selection_end_pos);
 
  if (selection_start_pos != selection_end_pos)
    {
      if (gtk_selection_owner_set (GTK_WIDGET (editable),
				   clipboard_atom,
				   time))
	editable->clipboard_text = gtk_editable_get_public_chars (editable,
								  selection_start_pos,
								  selection_end_pos);
    }
}

static void
gtk_editable_real_paste_clipboard (GtkEditable *editable)
{
  guint32 time;

  g_return_if_fail (editable != NULL);
  g_return_if_fail (GTK_IS_EDITABLE (editable));
  
  time = gtk_editable_get_event_time (editable);
  if (editable->editable)
    gtk_selection_convert (GTK_WIDGET(editable), 
			   clipboard_atom, 
			   gdk_atom_intern ("COMPOUND_TEXT", FALSE), time);
}

void
gtk_editable_changed (GtkEditable *editable)
{
  g_return_if_fail (editable != NULL);
  g_return_if_fail (GTK_IS_EDITABLE (editable));
  
  gtk_signal_emit (GTK_OBJECT (editable), editable_signals[CHANGED]);
}

#if 0
static void
gtk_editable_parent_set (GtkWidget *widget, 
			 GtkWidget *old_parent, 
			 GtkWidget *editable)
{
  GtkWidget *parent;

  parent = old_parent;
  while (parent)
    {
      gtk_signal_disconnect_by_func (GTK_OBJECT (parent),
				     GTK_SIGNAL_FUNC (gtk_editable_parent_set),
				     editable);
      parent = parent->parent;
    }

  parent = widget->parent;
  while (parent)
    {
      gtk_signal_connect (GTK_OBJECT (parent), "parent_set",
			  GTK_SIGNAL_FUNC (gtk_editable_parent_set), 
			  editable);
      
      parent = parent->parent;
    }
}
#endif
