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

#include <ctype.h>
#include <string.h>
#include "gdk/gdkkeysyms.h"
#include "gdk/gdki18n.h"
#include "gtkclipboard.h"
#include "gtkoldeditable.h"
#include "gtkmain.h"
#include "gtkselection.h"
#include "gtksignal.h"

#define MIN_EDITABLE_WIDTH  150
#define DRAW_TIMEOUT     20
#define INNER_BORDER     2

enum {
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

static void  gtk_old_editable_class_init           (GtkOldEditableClass *klass);
static void  gtk_old_editable_editable_init        (GtkEditableClass    *iface);
static void  gtk_old_editable_init                 (GtkOldEditable      *editable);
static void  gtk_old_editable_set_arg              (GtkObject           *object,
						    GtkArg              *arg,
						    guint                arg_id);
static void  gtk_old_editable_get_arg              (GtkObject           *object,
						    GtkArg              *arg,
						    guint                arg_id);
static void *gtk_old_editable_get_public_chars     (GtkOldEditable      *old_editable,
						    gint                 start,
						    gint                 end);

static gint gtk_old_editable_selection_clear    (GtkWidget         *widget,
						 GdkEventSelection *event);
static void gtk_old_editable_selection_get      (GtkWidget         *widget,
						 GtkSelectionData  *selection_data,
						 guint              info,
						 guint              time);
static void gtk_old_editable_selection_received (GtkWidget         *widget,
						 GtkSelectionData  *selection_data,
						 guint              time);

static void  gtk_old_editable_set_selection        (GtkOldEditable      *old_editable,
						    gint                 start,
						    gint                 end);

static void gtk_old_editable_real_set_editable    (GtkOldEditable *old_editable,
						   gboolean        is_editable);
static void gtk_old_editable_real_cut_clipboard   (GtkOldEditable *old_editable);
static void gtk_old_editable_real_copy_clipboard  (GtkOldEditable *old_editable);
static void gtk_old_editable_real_paste_clipboard (GtkOldEditable *old_editable);

static void     gtk_old_editable_insert_text         (GtkEditable *editable,
						      const gchar *new_text,
						      gint         new_text_length,
						      gint        *position);
static void     gtk_old_editable_delete_text         (GtkEditable *editable,
						      gint         start_pos,
						      gint         end_pos);
static gchar *  gtk_old_editable_get_chars           (GtkEditable *editable,
						      gint         start,
						      gint         end);
static void     gtk_old_editable_set_selection_bounds (GtkEditable *editable,
						       gint         start,
						       gint         end);
static gboolean gtk_old_editable_get_selection_bounds (GtkEditable *editable,
						       gint        *start,
						       gint        *end);
static void     gtk_old_editable_set_position        (GtkEditable *editable,
						      gint         position);
static gint     gtk_old_editable_get_position        (GtkEditable *editable);

static GtkWidgetClass *parent_class = NULL;
static guint editable_signals[LAST_SIGNAL] = { 0 };

GtkType
gtk_old_editable_get_type (void)
{
  static GtkType old_editable_type = 0;

  if (!old_editable_type)
    {
      static const GtkTypeInfo old_editable_info =
      {
	"GtkOldEditable",
	sizeof (GtkOldEditable),
	sizeof (GtkOldEditableClass),
	(GtkClassInitFunc) gtk_old_editable_class_init,
	(GtkObjectInitFunc) gtk_old_editable_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      static const GInterfaceInfo editable_info =
      {
	(GInterfaceInitFunc) gtk_old_editable_editable_init,	 /* interface_init */
	NULL,			                                 /* interface_finalize */
	NULL			                                 /* interface_data */
      };

      old_editable_type = gtk_type_unique (GTK_TYPE_WIDGET, &old_editable_info);
      g_type_add_interface_static (old_editable_type,
				   GTK_TYPE_EDITABLE,
				   &editable_info);
    }

  return old_editable_type;
}

static void
gtk_old_editable_class_init (GtkOldEditableClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;

  parent_class = gtk_type_class (GTK_TYPE_WIDGET);
    
  object_class->set_arg = gtk_old_editable_set_arg;
  object_class->get_arg = gtk_old_editable_get_arg;

  widget_class->selection_clear_event = gtk_old_editable_selection_clear;
  widget_class->selection_received = gtk_old_editable_selection_received;
  widget_class->selection_get = gtk_old_editable_selection_get;

  class->activate = NULL;
  class->set_editable = gtk_old_editable_real_set_editable;

  class->move_cursor = NULL;
  class->move_word = NULL;
  class->move_page = NULL;
  class->move_to_row = NULL;
  class->move_to_column = NULL;

  class->kill_char = NULL;
  class->kill_word = NULL;
  class->kill_line = NULL;

  class->cut_clipboard = gtk_old_editable_real_cut_clipboard;
  class->copy_clipboard = gtk_old_editable_real_copy_clipboard;
  class->paste_clipboard = gtk_old_editable_real_paste_clipboard;

  class->update_text = NULL;
  class->get_chars = NULL;
  class->set_selection = NULL;
  class->set_position = NULL;

  editable_signals[ACTIVATE] =
    gtk_signal_new ("activate",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkOldEditableClass, activate),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  widget_class->activate_signal = editable_signals[ACTIVATE];

  editable_signals[SET_EDITABLE] =
    gtk_signal_new ("set-editable",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkOldEditableClass, set_editable),
		    gtk_marshal_NONE__BOOL,
		    GTK_TYPE_NONE, 1,
		    GTK_TYPE_BOOL);

  editable_signals[MOVE_CURSOR] =
    gtk_signal_new ("move_cursor",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkOldEditableClass, move_cursor),
		    gtk_marshal_NONE__INT_INT,
		    GTK_TYPE_NONE, 2, 
		    GTK_TYPE_INT, 
		    GTK_TYPE_INT);

  editable_signals[MOVE_WORD] =
    gtk_signal_new ("move_word",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkOldEditableClass, move_word),
		    gtk_marshal_NONE__INT,
		    GTK_TYPE_NONE, 1, 
		    GTK_TYPE_INT);

  editable_signals[MOVE_PAGE] =
    gtk_signal_new ("move_page",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkOldEditableClass, move_page),
		    gtk_marshal_NONE__INT_INT,
		    GTK_TYPE_NONE, 2, 
		    GTK_TYPE_INT, 
		    GTK_TYPE_INT);

  editable_signals[MOVE_TO_ROW] =
    gtk_signal_new ("move_to_row",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkOldEditableClass, move_to_row),
		    gtk_marshal_NONE__INT,
		    GTK_TYPE_NONE, 1, 
		    GTK_TYPE_INT);

  editable_signals[MOVE_TO_COLUMN] =
    gtk_signal_new ("move_to_column",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkOldEditableClass, move_to_column),
		    gtk_marshal_NONE__INT,
		    GTK_TYPE_NONE, 1, 
		    GTK_TYPE_INT);

  editable_signals[KILL_CHAR] =
    gtk_signal_new ("kill_char",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkOldEditableClass, kill_char),
		    gtk_marshal_NONE__INT,
		    GTK_TYPE_NONE, 1, 
		    GTK_TYPE_INT);

  editable_signals[KILL_WORD] =
    gtk_signal_new ("kill_word",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkOldEditableClass, kill_word),
		    gtk_marshal_NONE__INT,
		    GTK_TYPE_NONE, 1, 
		    GTK_TYPE_INT);

  editable_signals[KILL_LINE] =
    gtk_signal_new ("kill_line",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkOldEditableClass, kill_line),
		    gtk_marshal_NONE__INT,
		    GTK_TYPE_NONE, 1, 
		    GTK_TYPE_INT);

  editable_signals[CUT_CLIPBOARD] =
    gtk_signal_new ("cut_clipboard",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkOldEditableClass, cut_clipboard),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);

  editable_signals[COPY_CLIPBOARD] =
    gtk_signal_new ("copy_clipboard",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkOldEditableClass, copy_clipboard),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);

  editable_signals[PASTE_CLIPBOARD] =
    gtk_signal_new ("paste_clipboard",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkOldEditableClass, paste_clipboard),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);

  gtk_object_add_arg_type ("GtkOldEditable::text_position", GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_TEXT_POSITION);
  gtk_object_add_arg_type ("GtkOldEditable::editable", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_EDITABLE);
}

static void
gtk_old_editable_editable_init (GtkEditableClass *iface)
{
  iface->do_insert_text = gtk_old_editable_insert_text;
  iface->do_delete_text = gtk_old_editable_delete_text;
  iface->get_chars = gtk_old_editable_get_chars;
  iface->set_selection_bounds = gtk_old_editable_set_selection_bounds;
  iface->get_selection_bounds = gtk_old_editable_get_selection_bounds;
  iface->set_position = gtk_old_editable_set_position;
  iface->get_position = gtk_old_editable_get_position;
}

static void
gtk_old_editable_set_arg (GtkObject *object,
			  GtkArg    *arg,
			  guint      arg_id)
{
  GtkEditable *editable = GTK_EDITABLE (object);

  switch (arg_id)
    {
    case ARG_TEXT_POSITION:
      gtk_editable_set_position (editable, GTK_VALUE_INT (*arg));
      break;
    case ARG_EDITABLE:
      gtk_signal_emit (object, editable_signals[SET_EDITABLE],
		       GTK_VALUE_BOOL (*arg) != FALSE);
      break;
    default:
      break;
    }
}

static void
gtk_old_editable_get_arg (GtkObject *object,
			  GtkArg    *arg,
			  guint      arg_id)
{
  GtkOldEditable *old_editable;

  old_editable = GTK_OLD_EDITABLE (object);

  switch (arg_id)
    {
    case ARG_TEXT_POSITION:
      GTK_VALUE_INT (*arg) = old_editable->current_pos;
      break;
    case ARG_EDITABLE:
      GTK_VALUE_BOOL (*arg) = old_editable->editable;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

static void
gtk_old_editable_init (GtkOldEditable *old_editable)
{
  static const GtkTargetEntry targets[] = {
    { "UTF8_STRING", 0, 0 },
    { "STRING", 0, 0 },
    { "TEXT",   0, 0 }, 
    { "COMPOUND_TEXT", 0, 0 }
  };

  GTK_WIDGET_SET_FLAGS (old_editable, GTK_CAN_FOCUS);

  old_editable->selection_start_pos = 0;
  old_editable->selection_end_pos = 0;
  old_editable->has_selection = FALSE;
  old_editable->editable = 1;
  old_editable->visible = 1;
  old_editable->clipboard_text = NULL;

  gtk_selection_add_targets (GTK_WIDGET (old_editable), GDK_SELECTION_PRIMARY,
			     targets, G_N_ELEMENTS (targets));
}

static void
gtk_old_editable_insert_text (GtkEditable *editable,
			      const gchar *new_text,
			      gint         new_text_length,
			      gint        *position)
{
  gchar buf[64];
  gchar *text;

  gtk_widget_ref (GTK_WIDGET (editable));

  if (new_text_length <= 63)
    text = buf;
  else
    text = g_new (gchar, new_text_length + 1);

  text[new_text_length] = '\0';
  strncpy (text, new_text, new_text_length);
  
  g_signal_emit_by_name (editable, "insert_text", text, new_text_length,
			 position);
  g_signal_emit_by_name (editable, "changed");
  
  if (new_text_length > 63)
    g_free (text);

  gtk_widget_unref (GTK_WIDGET (editable));
}

static void
gtk_old_editable_delete_text (GtkEditable *editable,
			      gint         start_pos,
			      gint         end_pos)
{
  GtkOldEditable *old_editable = GTK_OLD_EDITABLE (editable);

  gtk_widget_ref (GTK_WIDGET (old_editable));

  g_signal_emit_by_name (editable, "delete_text", start_pos, end_pos);
  g_signal_emit_by_name (editable, "changed");

  if (old_editable->selection_start_pos == old_editable->selection_end_pos &&
      old_editable->has_selection)
    gtk_old_editable_claim_selection (old_editable, FALSE, GDK_CURRENT_TIME);
  
  gtk_widget_unref (GTK_WIDGET (old_editable));
}

static void
gtk_old_editable_update_text (GtkOldEditable *old_editable,
			      gint            start_pos,
			      gint            end_pos)
{
  GtkOldEditableClass *klass = GTK_OLD_EDITABLE_GET_CLASS (old_editable);
  klass->update_text (GTK_OLD_EDITABLE (old_editable), start_pos, end_pos);
}

static gchar *    
gtk_old_editable_get_chars  (GtkEditable      *editable,
			     gint              start,
			     gint              end)
{
  GtkOldEditableClass *klass = GTK_OLD_EDITABLE_GET_CLASS (editable);
  return klass->get_chars (GTK_OLD_EDITABLE (editable), start, end);
}

/*
 * Like gtk_editable_get_chars, but if the editable is not
 * visible, return asterisks; also convert result to UTF-8.
 */
static void *    
gtk_old_editable_get_public_chars (GtkOldEditable   *old_editable,
				   gint              start,
				   gint              end)
{
  gchar *str = NULL;
  const gchar *charset;
  gboolean need_conversion = !g_get_charset (&charset);

  if (old_editable->visible)
    {
      GError *error = NULL;
      gchar *tmp = gtk_editable_get_chars (GTK_EDITABLE (old_editable), start, end);

      if (need_conversion)
	{
	  str = g_convert (tmp, -1,
			   "UTF-8", charset,
			   NULL, NULL, &error);
	  
	  if (!str)
	    {
	      g_warning ("Cannot convert text from charset to UTF-8 %s: %s", charset, error->message);
	      g_error_free (error);
	    }

	  g_free (tmp);
	}
      else
	str = tmp;
    }
  else
    {
      gint i;
      gint nchars = end - start;
       
      if (nchars < 0)
	nchars = -nchars;

      str = g_new (gchar, nchars + 1);
      for (i = 0; i<nchars; i++)
	str[i] = '*';
      str[i] = '\0';
    }

  return str;
}

static void
gtk_old_editable_set_selection (GtkOldEditable *old_editable,
				gint            start_pos,
				gint            end_pos)
{
  GtkOldEditableClass *klass = GTK_OLD_EDITABLE_GET_CLASS (old_editable);
  klass->set_selection (old_editable, start_pos, end_pos);
}

static void
gtk_old_editable_set_position (GtkEditable *editable,
			       gint            position)
{
  GtkOldEditableClass *klass = GTK_OLD_EDITABLE_GET_CLASS (editable);

  klass->set_position (GTK_OLD_EDITABLE (editable), position);
}

static gint
gtk_old_editable_get_position (GtkEditable *editable)
{
  return GTK_OLD_EDITABLE (editable)->current_pos;
}

static gint
gtk_old_editable_selection_clear (GtkWidget         *widget,
				  GdkEventSelection *event)
{
  GtkOldEditable *old_editable = GTK_OLD_EDITABLE (widget);
  
  /* Let the selection handling code know that the selection
   * has been changed, since we've overriden the default handler */
  if (!gtk_selection_clear (widget, event))
    return FALSE;
  
  if (old_editable->has_selection)
    {
      old_editable->has_selection = FALSE;
      gtk_old_editable_update_text (old_editable, old_editable->selection_start_pos,
				    old_editable->selection_end_pos);
    }
  
  return TRUE;
}

static void
gtk_old_editable_selection_get (GtkWidget        *widget,
				GtkSelectionData *selection_data,
				guint             info,
				guint             time)
{
  GtkOldEditable *old_editable = GTK_OLD_EDITABLE (widget);
  gint selection_start_pos;
  gint selection_end_pos;

  gchar *str;

  selection_start_pos = MIN (old_editable->selection_start_pos, old_editable->selection_end_pos);
  selection_end_pos = MAX (old_editable->selection_start_pos, old_editable->selection_end_pos);

  str = gtk_old_editable_get_public_chars (old_editable, 
					   selection_start_pos, 
					   selection_end_pos);

  if (str)
    {
      gtk_selection_data_set_text (selection_data, str);
      g_free (str);
    }
}

static void
gtk_old_editable_paste_received (GtkOldEditable *old_editable,
				 const gchar    *text,
				 gboolean        is_clipboard)
{
  const gchar *str = NULL;
  const gchar *charset;
  gboolean need_conversion = FALSE;

  if (text)
    {
      GError *error = NULL;
      
      need_conversion = !g_get_charset (&charset);

      if (need_conversion)
	{
	  str = g_convert_with_fallback (text, -1,
					 charset, "UTF-8", NULL,
					 NULL, NULL, &error);
	  if (!str)
	    {
	      g_warning ("Cannot convert text from UTF-8 to %s: %s",
			 charset, error->message);
	      g_error_free (error);
	      return;
	    }
	}
      else
	str = text;
    }

  if (str)
    {
      gboolean reselect;
      gint old_pos;
      gint tmp_pos;
  
      reselect = FALSE;

      if ((old_editable->selection_start_pos != old_editable->selection_end_pos) && 
	  (!old_editable->has_selection || is_clipboard))
	{
	  reselect = TRUE;
	  
	  /* Don't want to call gtk_editable_delete_selection here if we are going
	   * to reclaim the selection to avoid extra server traffic */
	  if (old_editable->has_selection)
	    {
	      gtk_editable_delete_text (GTK_EDITABLE (old_editable),
					MIN (old_editable->selection_start_pos, old_editable->selection_end_pos),
					MAX (old_editable->selection_start_pos, old_editable->selection_end_pos));
	    }
	  else
	    gtk_editable_delete_selection (GTK_EDITABLE (old_editable));
	}
      
      tmp_pos = old_pos = old_editable->current_pos;
      
      gtk_editable_insert_text (GTK_EDITABLE (old_editable), str, -1, &tmp_pos);

      if (reselect)
	gtk_old_editable_set_selection (old_editable, old_pos, old_editable->current_pos);

      if (str && str != text)
	g_free ((gchar *) str);
    }
}

static void
gtk_old_editable_selection_received  (GtkWidget         *widget,
				      GtkSelectionData  *selection_data,
				      guint              time)
{
  GtkOldEditable *old_editable = GTK_OLD_EDITABLE (widget);

  gchar *text = gtk_selection_data_get_text (selection_data);

  if (!text)
    {
      /* If we asked for UTF8 and didn't get it, try text; if we asked
       * for text and didn't get it, try string.  If we asked for
       * anything else and didn't get it, give up.
       */
      if (selection_data->target == gdk_atom_intern ("UTF8_STRING", FALSE))
	{
	  gtk_selection_convert (widget, GDK_SELECTION_PRIMARY,
				 gdk_atom_intern ("TEXT", FALSE),
				 time);
	  return;
	}
      else if (selection_data->target == gdk_atom_intern ("TEXT", FALSE))
	{
	  gtk_selection_convert (widget, GDK_SELECTION_PRIMARY,
				 GDK_TARGET_STRING,
				 time);
	  return;
	}
    }

  if (text)
    {
      gtk_old_editable_paste_received (old_editable, text, FALSE);
      g_free (text);
    }
}

static void
old_editable_text_received_cb (GtkClipboard *clipboard,
			       const gchar  *text,
			       gpointer      data)
{
  GtkOldEditable *old_editable = GTK_OLD_EDITABLE (data);

  gtk_old_editable_paste_received (old_editable, text, TRUE);
  g_object_unref (G_OBJECT (old_editable));
}

void
gtk_old_editable_claim_selection (GtkOldEditable *old_editable, 
				  gboolean        claim, 
				  guint32         time)
{
  g_return_if_fail (GTK_IS_OLD_EDITABLE (old_editable));
  g_return_if_fail (GTK_WIDGET_REALIZED (old_editable));

  old_editable->has_selection = FALSE;
  
  if (claim)
    {
      if (gtk_selection_owner_set (GTK_WIDGET (old_editable), GDK_SELECTION_PRIMARY, time))
	old_editable->has_selection = TRUE;
    }
  else
    {
      if (gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == GTK_WIDGET (old_editable)->window)
	gtk_selection_owner_set (NULL, GDK_SELECTION_PRIMARY, time);
    }
}

static void
gtk_old_editable_set_selection_bounds (GtkEditable *editable,
				       gint         start,
				       gint         end)
{
  GtkOldEditable *old_editable = GTK_OLD_EDITABLE (editable);
  
  if (GTK_WIDGET_REALIZED (editable))
    gtk_old_editable_claim_selection (old_editable, start != end, GDK_CURRENT_TIME);
  
  gtk_old_editable_set_selection (old_editable, start, end);
}

static gboolean
gtk_old_editable_get_selection_bounds (GtkEditable *editable,
				       gint        *start,
				       gint        *end)
{
  GtkOldEditable *old_editable = GTK_OLD_EDITABLE (editable);

  *start = old_editable->selection_start_pos;
  *end = old_editable->selection_end_pos;

  return (old_editable->selection_start_pos != old_editable->selection_end_pos);
}

static void
gtk_old_editable_real_set_editable (GtkOldEditable *old_editable,
				    gboolean        is_editable)
{
  is_editable = is_editable != FALSE;

  if (old_editable->editable != is_editable)
    {
      old_editable->editable = is_editable;
      gtk_widget_queue_draw (GTK_WIDGET (old_editable));
    }
}

static void
gtk_old_editable_real_cut_clipboard (GtkOldEditable *old_editable)
{
  gtk_old_editable_real_copy_clipboard (old_editable);
  gtk_editable_delete_selection (GTK_EDITABLE (old_editable));
}

static void
gtk_old_editable_real_copy_clipboard (GtkOldEditable *old_editable)
{
  gint selection_start_pos; 
  gint selection_end_pos;

  selection_start_pos = MIN (old_editable->selection_start_pos, old_editable->selection_end_pos);
  selection_end_pos = MAX (old_editable->selection_start_pos, old_editable->selection_end_pos);

  if (selection_start_pos != selection_end_pos)
    {
      gchar *text = gtk_old_editable_get_public_chars (old_editable,
						       selection_start_pos,
						       selection_end_pos);

      if (text)
	{
	  gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), text, -1);
	  g_free (text);
	}
    }
}

static void
gtk_old_editable_real_paste_clipboard (GtkOldEditable *old_editable)
{
  g_object_ref (G_OBJECT (old_editable));
  gtk_clipboard_request_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
			      old_editable_text_received_cb, old_editable);
}

void
gtk_old_editable_changed (GtkOldEditable *old_editable)
{
  g_return_if_fail (GTK_IS_OLD_EDITABLE (old_editable));
  
  g_signal_emit_by_name (old_editable, "changed");
}
