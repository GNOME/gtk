/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * GtkStatusbar Copyright (C) 1998 Shawn T. Amundson
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

#include "gtkframe.h"
#include "gtklabel.h"
#include "gtksignal.h"
#include "gtkstatusbar.h"
#include "gtkwindow.h"
#include "gtkintl.h"

typedef struct _GtkStatusbarMsg GtkStatusbarMsg;

struct _GtkStatusbarMsg
{
  gchar *text;
  guint context_id;
  guint message_id;
};

enum
{
  SIGNAL_TEXT_PUSHED,
  SIGNAL_TEXT_POPPED,
  SIGNAL_LAST
};

static void     gtk_statusbar_class_init     (GtkStatusbarClass *class);
static void     gtk_statusbar_init           (GtkStatusbar      *statusbar);
static void     gtk_statusbar_destroy        (GtkObject         *object);
static void     gtk_statusbar_update         (GtkStatusbar      *statusbar,
					      guint              context_id,
					      const gchar       *text);
static void     gtk_statusbar_size_allocate  (GtkWidget         *widget,
					      GtkAllocation     *allocation);
static void     gtk_statusbar_realize        (GtkWidget         *widget);
static void     gtk_statusbar_unrealize      (GtkWidget         *widget);
static void     gtk_statusbar_map            (GtkWidget         *widget);
static void     gtk_statusbar_unmap          (GtkWidget         *widget);
static gboolean gtk_statusbar_button_press   (GtkWidget         *widget,
					      GdkEventButton    *event);
static gboolean gtk_statusbar_expose_event   (GtkWidget         *widget,
					      GdkEventExpose    *event);
static void     gtk_statusbar_size_request   (GtkWidget         *widget,
                                              GtkRequisition    *requisition);
static void     gtk_statusbar_size_allocate  (GtkWidget         *widget,
                                              GtkAllocation     *allocation);
static void     gtk_statusbar_create_window  (GtkStatusbar      *statusbar);
static void     gtk_statusbar_destroy_window (GtkStatusbar      *statusbar);

static GtkContainerClass *parent_class;
static guint              statusbar_signals[SIGNAL_LAST] = { 0 };

GtkType      
gtk_statusbar_get_type (void)
{
  static GtkType statusbar_type = 0;

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

      statusbar_type = gtk_type_unique (GTK_TYPE_HBOX, &statusbar_info);
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

  parent_class = gtk_type_class (GTK_TYPE_HBOX);
  
  object_class->destroy = gtk_statusbar_destroy;

  widget_class->realize = gtk_statusbar_realize;
  widget_class->unrealize = gtk_statusbar_unrealize;
  widget_class->map = gtk_statusbar_map;
  widget_class->unmap = gtk_statusbar_unmap;
  
  widget_class->button_press_event = gtk_statusbar_button_press;
  widget_class->expose_event = gtk_statusbar_expose_event;

  widget_class->size_request = gtk_statusbar_size_request;
  widget_class->size_allocate = gtk_statusbar_size_allocate;
  
  class->messages_mem_chunk = g_mem_chunk_new ("GtkStatusBar messages mem chunk",
					       sizeof (GtkStatusbarMsg),
					       sizeof (GtkStatusbarMsg) * 64,
					       G_ALLOC_AND_FREE);

  class->text_pushed = gtk_statusbar_update;
  class->text_popped = gtk_statusbar_update;
  
  statusbar_signals[SIGNAL_TEXT_PUSHED] =
    gtk_signal_new ("text_pushed",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkStatusbarClass, text_pushed),
		    gtk_marshal_VOID__UINT_STRING,
		    GTK_TYPE_NONE, 2,
		    GTK_TYPE_UINT,
		    GTK_TYPE_STRING);
  statusbar_signals[SIGNAL_TEXT_POPPED] =
    gtk_signal_new ("text_popped",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkStatusbarClass, text_popped),
		    gtk_marshal_VOID__UINT_STRING,
		    GTK_TYPE_NONE, 2,
		    GTK_TYPE_UINT,
		    GTK_TYPE_STRING);

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_enum ("shadow_type",
                                                              _("Shadow type"),
                                                              _("Style of bevel around the statusbar text"),
                                                              GTK_TYPE_SHADOW_TYPE,
                                                              GTK_SHADOW_IN,
                                                              G_PARAM_READABLE));
}

static void
gtk_statusbar_init (GtkStatusbar *statusbar)
{
  GtkBox *box;
  GtkShadowType shadow_type;
  
  box = GTK_BOX (statusbar);

  box->spacing = 2;
  box->homogeneous = FALSE;

  statusbar->has_resize_grip = TRUE;

  gtk_widget_style_get (GTK_WIDGET (statusbar), "shadow_type", &shadow_type, NULL);
  
  statusbar->frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (statusbar->frame), shadow_type);
  gtk_box_pack_start (box, statusbar->frame, TRUE, TRUE, 0);
  gtk_widget_show (statusbar->frame);

  statusbar->label = gtk_label_new ("");
  gtk_misc_set_alignment (GTK_MISC (statusbar->label), 0.0, 0.0);
  /* don't expand the size request for the label; if we
   * do that then toplevels weirdly resize
   */
  gtk_widget_set_usize (statusbar->label, 1, -1);
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
  return gtk_type_new (GTK_TYPE_STATUSBAR);
}

static void
gtk_statusbar_update (GtkStatusbar *statusbar,
		      guint	    context_id,
		      const gchar  *text)
{
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

  g_return_val_if_fail (GTK_IS_STATUSBAR (statusbar), 0);
  g_return_val_if_fail (text != NULL, 0);

  class = GTK_STATUSBAR_GET_CLASS (statusbar);
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

  g_return_if_fail (GTK_IS_STATUSBAR (statusbar));

  if (statusbar->messages)
    {
      GSList *list;

      for (list = statusbar->messages; list; list = list->next)
	{
	  msg = list->data;

	  if (msg->context_id == context_id)
	    {
	      GtkStatusbarClass *class;

	      class = GTK_STATUSBAR_GET_CLASS (statusbar);

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

  g_return_if_fail (GTK_IS_STATUSBAR (statusbar));
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
	      
	      class = GTK_STATUSBAR_GET_CLASS (statusbar);
	      statusbar->messages = g_slist_remove_link (statusbar->messages, list);
	      g_free (msg->text);
	      g_mem_chunk_free (class->messages_mem_chunk, msg);
	      g_slist_free_1 (list);
	      
	      break;
	    }
	}
    }
}

void
gtk_statusbar_set_has_resize_grip (GtkStatusbar *statusbar,
				   gboolean      setting)
{
  g_return_if_fail (GTK_IS_STATUSBAR (statusbar));

  setting = setting != FALSE;

  if (setting != statusbar->has_resize_grip)
    {
      statusbar->has_resize_grip = setting;
      gtk_widget_queue_draw (GTK_WIDGET (statusbar));

      if (GTK_WIDGET_REALIZED (statusbar))
        {
          if (statusbar->has_resize_grip && statusbar->grip_window == NULL)
            gtk_statusbar_create_window (statusbar);
          else if (!statusbar->has_resize_grip && statusbar->grip_window != NULL)
            gtk_statusbar_destroy_window (statusbar);
        }
    }
}

gboolean
gtk_statusbar_get_has_resize_grip (GtkStatusbar *statusbar)
{
  g_return_val_if_fail (GTK_IS_STATUSBAR (statusbar), FALSE);

  return statusbar->has_resize_grip;
}

static void
gtk_statusbar_destroy (GtkObject *object)
{
  GtkStatusbar *statusbar;
  GtkStatusbarClass *class;
  GSList *list;

  g_return_if_fail (GTK_IS_STATUSBAR (object));

  statusbar = GTK_STATUSBAR (object);
  class = GTK_STATUSBAR_GET_CLASS (statusbar);

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
get_grip_rect (GtkStatusbar *statusbar,
               GdkRectangle *rect)
{
  GtkWidget *widget;
  gint w, h;
  
  widget = GTK_WIDGET (statusbar);

  /* These are in effect the max/default size of the grip. */
  w = 18;
  h = 18;

  if (w > (widget->allocation.width))
    w = widget->allocation.width;

  if (h > (widget->allocation.height - widget->style->ythickness))
    h = widget->allocation.height - widget->style->ythickness;
  
  rect->x = widget->allocation.x + widget->allocation.width - w;
  rect->y = widget->allocation.y + widget->allocation.height - h;
  rect->width = w;
  rect->height = h;
}

static void
gtk_statusbar_create_window (GtkStatusbar *statusbar)
{
  GtkWidget *widget;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GdkRectangle rect;
  
  g_return_if_fail (GTK_WIDGET_REALIZED (statusbar));
  g_return_if_fail (statusbar->has_resize_grip);
  
  widget = GTK_WIDGET (statusbar);

  get_grip_rect (statusbar, &rect);

  attributes.x = rect.x;
  attributes.y = rect.y;
  attributes.width = rect.width;
  attributes.height = rect.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.event_mask = gtk_widget_get_events (widget) |
    GDK_BUTTON_PRESS_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y;

  statusbar->grip_window = gdk_window_new (widget->window,
                                           &attributes, attributes_mask);
  gdk_window_set_user_data (statusbar->grip_window, widget);
}

static void
gtk_statusbar_destroy_window (GtkStatusbar *statusbar)
{
  gdk_window_set_user_data (statusbar->grip_window, NULL);
  gdk_window_destroy (statusbar->grip_window);
  statusbar->grip_window = NULL;
}

static void
gtk_statusbar_realize (GtkWidget *widget)
{
  GtkStatusbar *statusbar;

  statusbar = GTK_STATUSBAR (widget);
  
  (* GTK_WIDGET_CLASS (parent_class)->realize) (widget);

  if (statusbar->has_resize_grip)
    gtk_statusbar_create_window (statusbar);
}

static void
gtk_statusbar_unrealize (GtkWidget *widget)
{
  GtkStatusbar *statusbar;

  statusbar = GTK_STATUSBAR (widget);

  if (statusbar->grip_window)
    gtk_statusbar_destroy_window (statusbar);
  
  (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
gtk_statusbar_map (GtkWidget *widget)
{
  GtkStatusbar *statusbar;

  statusbar = GTK_STATUSBAR (widget);
  
  (* GTK_WIDGET_CLASS (parent_class)->map) (widget);
  
  if (statusbar->grip_window)
    gdk_window_show (statusbar->grip_window);
}

static void
gtk_statusbar_unmap (GtkWidget *widget)
{
  GtkStatusbar *statusbar;

  statusbar = GTK_STATUSBAR (widget);

  if (statusbar->grip_window)
    gdk_window_hide (statusbar->grip_window);
  
  (* GTK_WIDGET_CLASS (parent_class)->unmap) (widget);
}

static gboolean
gtk_statusbar_button_press (GtkWidget      *widget,
                            GdkEventButton *event)
{
  GtkStatusbar *statusbar;
  GtkWidget *ancestor;
  
  statusbar = GTK_STATUSBAR (widget);
  
  if (!statusbar->has_resize_grip)
    return FALSE;
  
  ancestor = gtk_widget_get_toplevel (widget);

  if (!GTK_IS_WINDOW (ancestor))
    return FALSE;

  if (event->button == 1)
    gtk_window_begin_resize_drag (GTK_WINDOW (ancestor),
                                  GDK_WINDOW_EDGE_SOUTH_EAST,
                                  event->button,
                                  event->x_root, event->y_root,
                                  event->time);
  else if (event->button == 2)
    gtk_window_begin_move_drag (GTK_WINDOW (ancestor),
                                event->button,
                                event->x_root, event->y_root,
                                event->time);
  else
    return FALSE;
  
  return TRUE;
}

static gboolean
gtk_statusbar_expose_event (GtkWidget      *widget,
                            GdkEventExpose *event)
{
  GtkStatusbar *statusbar;
  GdkRectangle rect;
  
  statusbar = GTK_STATUSBAR (widget);

  GTK_WIDGET_CLASS (parent_class)->expose_event (widget, event);

  if (statusbar->has_resize_grip)
    {
      get_grip_rect (statusbar, &rect);

      gtk_paint_resize_grip (widget->style,
                             widget->window,
                             GTK_WIDGET_STATE (widget),
                             NULL,
                             widget,
                             "statusbar",
                             GDK_WINDOW_EDGE_SOUTH_EAST,
                             rect.x, rect.y,
                             /* don't draw grip over the frame, though you
                              * can click on the frame.
                              */
                             rect.width - widget->style->xthickness,
                             rect.height - widget->style->ythickness);
    }

  return FALSE;
}

static void
gtk_statusbar_size_request   (GtkWidget      *widget,
                              GtkRequisition *requisition)
{
  GtkStatusbar *statusbar;
  GtkShadowType shadow_type;
  
  statusbar = GTK_STATUSBAR (widget);

  gtk_widget_style_get (GTK_WIDGET (statusbar), "shadow_type", &shadow_type, NULL);  
  gtk_frame_set_shadow_type (GTK_FRAME (statusbar->frame), shadow_type);
  
  GTK_WIDGET_CLASS (parent_class)->size_request (widget, requisition);

  if (statusbar->has_resize_grip)
    {
      GdkRectangle rect;

      /* x, y in the grip rect depend on size allocation, but
       * w, h do not so this is OK
       */
      get_grip_rect (statusbar, &rect);
      
      requisition->width += rect.width;
      requisition->height = MAX (requisition->height, rect.height);
    }
}

static void
gtk_statusbar_size_allocate  (GtkWidget     *widget,
                              GtkAllocation *allocation)
{
  GtkStatusbar *statusbar;
  
  statusbar = GTK_STATUSBAR (widget);

  if (statusbar->has_resize_grip)
    {
      GdkRectangle rect;
      GtkRequisition saved_req;
      
      widget->allocation = *allocation; /* get_grip_rect needs this info */
      get_grip_rect (statusbar, &rect);
  
      if (statusbar->grip_window)
        gdk_window_move_resize (statusbar->grip_window,
                                rect.x, rect.y,
                                rect.width, rect.height);
      
      /* enter the bad hack zone */      
      saved_req = widget->requisition;
      widget->requisition.width -= rect.width; /* HBox::size_allocate needs this */
      if (widget->requisition.width < 0)
        widget->requisition.width = 0;
      GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);
      widget->requisition = saved_req;
    }
  else
    {
      /* chain up normally */
      GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);
    }
}
