/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * GtkStatusbar Copyright (C) 1998 Shawn T. Amundson
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

#include "gtkframe.h"
#include "gtklabel.h"
#include "gtksignal.h"
#include "gtkstatusbar.h"


enum
{
  SIGNAL_TEXT_PUSHED,
  SIGNAL_TEXT_POPPED,
  SIGNAL_LAST
};

static void gtk_statusbar_class_init               (GtkStatusbarClass *class);
static void gtk_statusbar_init                     (GtkStatusbar      *statusbar);
static void gtk_statusbar_destroy                  (GtkObject         *object);
static void gtk_statusbar_finalize                 (GtkObject         *object);
static void gtk_statusbar_update		   (GtkStatusbar      *statusbar,
						    guint	       context_id,
						    const gchar       *text);
     
static GtkContainerClass *parent_class;
static guint              statusbar_signals[SIGNAL_LAST] = { 0 };

guint      
gtk_statusbar_get_type (void)
{
  static guint statusbar_type = 0;

  if (!statusbar_type)
    {
      static const GtkTypeInfo statusbar_info =
      {
        "GtkStatusbar",
        sizeof (GtkStatusbar),
        sizeof (GtkStatusbarClass),
        (GtkClassInitFunc) gtk_statusbar_class_init,
        (GtkObjectInitFunc) gtk_statusbar_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      statusbar_type = gtk_type_unique (gtk_hbox_get_type (), &statusbar_info);
    }

  return statusbar_type;
}

static void
gtk_statusbar_class_init (GtkStatusbarClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass *) class;
  widget_class = (GtkWidgetClass *) class;
  container_class = (GtkContainerClass *) class;

  parent_class = gtk_type_class (gtk_hbox_get_type ());

  statusbar_signals[SIGNAL_TEXT_PUSHED] =
    gtk_signal_new ("text_pushed",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkStatusbarClass, text_pushed),
		    gtk_marshal_NONE__UINT_STRING,
		    GTK_TYPE_NONE, 2,
		    GTK_TYPE_UINT,
		    GTK_TYPE_STRING);
  statusbar_signals[SIGNAL_TEXT_POPPED] =
    gtk_signal_new ("text_popped",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkStatusbarClass, text_popped),
		    gtk_marshal_NONE__UINT_STRING,
		    GTK_TYPE_NONE, 2,
		    GTK_TYPE_UINT,
		    GTK_TYPE_STRING);
  gtk_object_class_add_signals (object_class, statusbar_signals, SIGNAL_LAST);
  
  object_class->destroy = gtk_statusbar_destroy;
  object_class->finalize = gtk_statusbar_finalize;

  class->messages_mem_chunk = g_mem_chunk_new ("GtkStatusBar messages mem chunk",
					       sizeof (GtkStatusbarMsg),
					       sizeof (GtkStatusbarMsg) * 64,
					       G_ALLOC_AND_FREE);

  class->text_pushed = gtk_statusbar_update;
  class->text_popped = gtk_statusbar_update;
}

static void
gtk_statusbar_init (GtkStatusbar *statusbar)
{
  GtkBox *box;

  box = GTK_BOX (statusbar);

  box->spacing = 2;
  box->homogeneous = FALSE;

  statusbar->frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (statusbar->frame), GTK_SHADOW_IN);
  gtk_box_pack_start (box, statusbar->frame, TRUE, TRUE, 0);
  gtk_widget_show (statusbar->frame);

  statusbar->label = gtk_label_new ("");
  gtk_misc_set_alignment (GTK_MISC (statusbar->label), 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (statusbar->frame), statusbar->label);
  gtk_widget_show (statusbar->label);

  statusbar->seq_context_id = 1;
  statusbar->seq_message_id = 1;
  statusbar->messages = NULL;
  statusbar->keys = NULL;
}

GtkWidget* 
gtk_statusbar_new (void)
{
  return gtk_type_new (gtk_statusbar_get_type ());
}

static void
gtk_statusbar_update (GtkStatusbar *statusbar,
		      guint	    context_id,
		      const gchar  *text)
{
  g_return_if_fail (statusbar != NULL);
  g_return_if_fail (GTK_IS_STATUSBAR (statusbar));

  if (!text)
    text = "";

  gtk_label_set_text (GTK_LABEL (statusbar->label), text);
}

guint
gtk_statusbar_get_context_id (GtkStatusbar *statusbar,
			      const gchar  *context_description)
{
  gchar *string;
  guint *id;
  
  g_return_val_if_fail (statusbar != NULL, 0);
  g_return_val_if_fail (GTK_IS_STATUSBAR (statusbar), 0);
  g_return_val_if_fail (context_description != NULL, 0);

  /* we need to preserve namespaces on object datas */
  string = g_strconcat ("gtk-status-bar-context:", context_description, NULL);

  id = gtk_object_get_data (GTK_OBJECT (statusbar), string);
  if (!id)
    {
      id = g_new (guint, 1);
      *id = statusbar->seq_context_id++;
      gtk_object_set_data_full (GTK_OBJECT (statusbar), string, id, (GtkDestroyNotify) g_free);
      statusbar->keys = g_slist_prepend (statusbar->keys, string);
    }
  else
    g_free (string);

  return *id;
}

guint
gtk_statusbar_push (GtkStatusbar *statusbar,
		    guint	  context_id,
		    const gchar  *text)
{
  GtkStatusbarMsg *msg;
  GtkStatusbarClass *class;

  g_return_val_if_fail (statusbar != NULL, 0);
  g_return_val_if_fail (GTK_IS_STATUSBAR (statusbar), 0);
  g_return_val_if_fail (text != NULL, 0);
  g_return_val_if_fail (context_id > 0, 0);

  class = GTK_STATUSBAR_CLASS (GTK_OBJECT (statusbar)->klass);
  msg = g_chunk_new (GtkStatusbarMsg, class->messages_mem_chunk);
  msg->text = g_strdup (text);
  msg->context_id = context_id;
  msg->message_id = statusbar->seq_message_id++;

  statusbar->messages = g_slist_prepend (statusbar->messages, msg);

  gtk_signal_emit (GTK_OBJECT (statusbar),
		   statusbar_signals[SIGNAL_TEXT_PUSHED],
		   msg->context_id,
		   msg->text);

  return msg->message_id;
}

void
gtk_statusbar_pop (GtkStatusbar *statusbar,
		   guint	 context_id)
{
  GtkStatusbarMsg *msg;

  g_return_if_fail (statusbar != NULL);
  g_return_if_fail (GTK_IS_STATUSBAR (statusbar));
  g_return_if_fail (context_id > 0);

  if (statusbar->messages)
    {
      GSList *list;

      for (list = statusbar->messages; list; list = list->next)
	{
	  msg = list->data;

	  if (msg->context_id == context_id)
	    {
	      GtkStatusbarClass *class;

	      class = GTK_STATUSBAR_CLASS (GTK_OBJECT (statusbar)->klass);

	      statusbar->messages = g_slist_remove_link (statusbar->messages,
							 list);
	      g_free (msg->text);
	      g_mem_chunk_free (class->messages_mem_chunk, msg);
	      g_slist_free_1 (list);
	      break;
	    }
	}
    }

  msg = statusbar->messages ? statusbar->messages->data : NULL;

  gtk_signal_emit (GTK_OBJECT (statusbar),
		   statusbar_signals[SIGNAL_TEXT_POPPED],
		   (guint) (msg ? msg->context_id : 0),
		   msg ? msg->text : NULL);
}

void
gtk_statusbar_remove (GtkStatusbar *statusbar,
		      guint	   context_id,
		      guint        message_id)
{
  GtkStatusbarMsg *msg;

  g_return_if_fail (statusbar != NULL);
  g_return_if_fail (GTK_IS_STATUSBAR (statusbar));
  g_return_if_fail (context_id > 0);
  g_return_if_fail (message_id > 0);

  msg = statusbar->messages ? statusbar->messages->data : NULL;
  if (msg)
    {
      GSList *list;

      /* care about signal emission if the topmost item is removed */
      if (msg->context_id == context_id &&
	  msg->message_id == message_id)
	{
	  gtk_statusbar_pop (statusbar, context_id);
	  return;
	}
      
      for (list = statusbar->messages; list; list = list->next)
	{
	  msg = list->data;
	  
	  if (msg->context_id == context_id &&
	      msg->message_id == message_id)
	    {
	      GtkStatusbarClass *class;
	      
	      class = GTK_STATUSBAR_CLASS (GTK_OBJECT (statusbar)->klass);
	      statusbar->messages = g_slist_remove_link (statusbar->messages, list);
	      g_free (msg->text);
	      g_mem_chunk_free (class->messages_mem_chunk, msg);
	      g_slist_free_1 (list);
	      
	      break;
	    }
	}
    }
}

static void
gtk_statusbar_destroy (GtkObject *object)
{
  GtkStatusbar *statusbar;
  GtkStatusbarClass *class;
  GSList *list;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_STATUSBAR (object));

  statusbar = GTK_STATUSBAR (object);
  class = GTK_STATUSBAR_CLASS (GTK_OBJECT (statusbar)->klass);

  for (list = statusbar->messages; list; list = list->next)
    {
      GtkStatusbarMsg *msg;

      msg = list->data;
      g_free (msg->text);
      g_mem_chunk_free (class->messages_mem_chunk, msg);
    }
  g_slist_free (statusbar->messages);
  statusbar->messages = NULL;

  for (list = statusbar->keys; list; list = list->next)
    g_free (list->data);
  g_slist_free (statusbar->keys);
  statusbar->keys = NULL;

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
gtk_statusbar_finalize (GtkObject *object)
{
  GtkStatusbar *statusbar;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_STATUSBAR (object));

  statusbar = GTK_STATUSBAR (object);

  GTK_OBJECT_CLASS (parent_class)->finalize (object);
}
