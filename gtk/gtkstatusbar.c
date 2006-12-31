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

#include <config.h>
#include "gtkframe.h"
#include "gtklabel.h"
#include "gtkmarshalers.h"
#include "gtkstatusbar.h"
#include "gtkwindow.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkalias.h"

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

enum 
{
  PROP_ZERO,
  PROP_HAS_RESIZE_GRIP
};

static void     gtk_statusbar_destroy           (GtkObject         *object);
static void     gtk_statusbar_update            (GtkStatusbar      *statusbar,
						 guint              context_id,
						 const gchar       *text);
static void     gtk_statusbar_size_allocate     (GtkWidget         *widget,
						 GtkAllocation     *allocation);
static void     gtk_statusbar_realize           (GtkWidget         *widget);
static void     gtk_statusbar_unrealize         (GtkWidget         *widget);
static void     gtk_statusbar_map               (GtkWidget         *widget);
static void     gtk_statusbar_unmap             (GtkWidget         *widget);
static gboolean gtk_statusbar_button_press      (GtkWidget         *widget,
						 GdkEventButton    *event);
static gboolean gtk_statusbar_expose_event      (GtkWidget         *widget,
						 GdkEventExpose    *event);
static void     gtk_statusbar_size_request      (GtkWidget         *widget,
						 GtkRequisition    *requisition);
static void     gtk_statusbar_size_allocate     (GtkWidget         *widget,
						 GtkAllocation     *allocation);
static void     gtk_statusbar_direction_changed (GtkWidget         *widget,
						 GtkTextDirection   prev_dir);
static void     gtk_statusbar_state_changed     (GtkWidget        *widget,
                                                 GtkStateType      previous_state);
static void     gtk_statusbar_create_window     (GtkStatusbar      *statusbar);
static void     gtk_statusbar_destroy_window    (GtkStatusbar      *statusbar);
static void     gtk_statusbar_get_property      (GObject           *object,
						 guint              prop_id,
						 GValue            *value,
						 GParamSpec        *pspec);
static void     gtk_statusbar_set_property      (GObject           *object,
						 guint              prop_id,
						 const GValue      *value,
						 GParamSpec        *pspec);
static void     label_selectable_changed        (GtkWidget         *label,
                                                 GParamSpec        *pspec,
						 gpointer           data);


static guint              statusbar_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (GtkStatusbar, gtk_statusbar, GTK_TYPE_HBOX)

static void
gtk_statusbar_class_init (GtkStatusbarClass *class)
{
  GObjectClass *gobject_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  gobject_class = (GObjectClass *) class;
  object_class = (GtkObjectClass *) class;
  widget_class = (GtkWidgetClass *) class;

  gobject_class->set_property = gtk_statusbar_set_property;
  gobject_class->get_property = gtk_statusbar_get_property;

  object_class->destroy = gtk_statusbar_destroy;

  widget_class->realize = gtk_statusbar_realize;
  widget_class->unrealize = gtk_statusbar_unrealize;
  widget_class->map = gtk_statusbar_map;
  widget_class->unmap = gtk_statusbar_unmap;
  widget_class->button_press_event = gtk_statusbar_button_press;
  widget_class->expose_event = gtk_statusbar_expose_event;
  widget_class->size_request = gtk_statusbar_size_request;
  widget_class->size_allocate = gtk_statusbar_size_allocate;
  widget_class->direction_changed = gtk_statusbar_direction_changed;
  widget_class->state_changed = gtk_statusbar_state_changed;
  
  class->text_pushed = gtk_statusbar_update;
  class->text_popped = gtk_statusbar_update;
  
  /**
   * GtkStatusbar:has-resize-grip:
   *
   * Whether the statusbar has a grip for resizing the toplevel window.
   *
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
				   PROP_HAS_RESIZE_GRIP,
				   g_param_spec_boolean ("has-resize-grip",
 							 P_("Has Resize Grip"),
 							 P_("Whether the statusbar has a grip for resizing the toplevel"),
 							 TRUE,
 							 GTK_PARAM_READWRITE));
  statusbar_signals[SIGNAL_TEXT_PUSHED] =
    g_signal_new (I_("text_pushed"),
		  G_OBJECT_CLASS_TYPE (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkStatusbarClass, text_pushed),
		  NULL, NULL,
		  _gtk_marshal_VOID__UINT_STRING,
		  G_TYPE_NONE, 2,
		  G_TYPE_UINT,
		  G_TYPE_STRING);
  statusbar_signals[SIGNAL_TEXT_POPPED] =
    g_signal_new (I_("text_popped"),
		  G_OBJECT_CLASS_TYPE (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkStatusbarClass, text_popped),
		  NULL, NULL,
		  _gtk_marshal_VOID__UINT_STRING,
		  G_TYPE_NONE, 2,
		  G_TYPE_UINT,
		  G_TYPE_STRING);

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_enum ("shadow-type",
                                                              P_("Shadow type"),
                                                              P_("Style of bevel around the statusbar text"),
                                                              GTK_TYPE_SHADOW_TYPE,
                                                              GTK_SHADOW_IN,
                                                              GTK_PARAM_READABLE));
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

  gtk_widget_style_get (GTK_WIDGET (statusbar), "shadow-type", &shadow_type, NULL);
  
  statusbar->frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (statusbar->frame), shadow_type);
  gtk_box_pack_start (box, statusbar->frame, TRUE, TRUE, 0);
  gtk_widget_show (statusbar->frame);

  statusbar->label = gtk_label_new ("");
  gtk_label_set_single_line_mode (GTK_LABEL (statusbar->label), TRUE);
  gtk_misc_set_alignment (GTK_MISC (statusbar->label), 0.0, 0.5);
  g_signal_connect (statusbar->label, "notify::selectable",
		    G_CALLBACK (label_selectable_changed), statusbar);
  gtk_label_set_ellipsize (GTK_LABEL (statusbar->label), PANGO_ELLIPSIZE_END);
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
  return g_object_new (GTK_TYPE_STATUSBAR, NULL);
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

  id = g_object_get_data (G_OBJECT (statusbar), string);
  if (!id)
    {
      id = g_new (guint, 1);
      *id = statusbar->seq_context_id++;
      g_object_set_data_full (G_OBJECT (statusbar), string, id, g_free);
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
  msg = g_slice_new (GtkStatusbarMsg);
  msg->text = g_strdup (text);
  msg->context_id = context_id;
  msg->message_id = statusbar->seq_message_id++;

  statusbar->messages = g_slist_prepend (statusbar->messages, msg);

  g_signal_emit (statusbar,
		 statusbar_signals[SIGNAL_TEXT_PUSHED],
		 0,
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
              g_slice_free (GtkStatusbarMsg, msg);
	      g_slist_free_1 (list);
	      break;
	    }
	}
    }

  msg = statusbar->messages ? statusbar->messages->data : NULL;

  g_signal_emit (statusbar,
		 statusbar_signals[SIGNAL_TEXT_POPPED],
		 0,
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
              g_slice_free (GtkStatusbarMsg, msg);
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
      gtk_widget_queue_resize (statusbar->label);
      gtk_widget_queue_draw (GTK_WIDGET (statusbar));

      if (GTK_WIDGET_REALIZED (statusbar))
        {
          if (statusbar->has_resize_grip && statusbar->grip_window == NULL)
	    {
	      gtk_statusbar_create_window (statusbar);
	      if (GTK_WIDGET_MAPPED (statusbar))
		gdk_window_show (statusbar->grip_window);
	    }
          else if (!statusbar->has_resize_grip && statusbar->grip_window != NULL)
            gtk_statusbar_destroy_window (statusbar);
        }
      
      g_object_notify (G_OBJECT (statusbar), "has-resize-grip");
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
      g_slice_free (GtkStatusbarMsg, msg);
    }
  g_slist_free (statusbar->messages);
  statusbar->messages = NULL;

  for (list = statusbar->keys; list; list = list->next)
    g_free (list->data);
  g_slist_free (statusbar->keys);
  statusbar->keys = NULL;

  GTK_OBJECT_CLASS (gtk_statusbar_parent_class)->destroy (object);
}

static void
gtk_statusbar_set_property (GObject      *object, 
			    guint         prop_id, 
			    const GValue *value, 
			    GParamSpec   *pspec)
{
  GtkStatusbar *statusbar = GTK_STATUSBAR (object);

  switch (prop_id) 
    {
    case PROP_HAS_RESIZE_GRIP:
      gtk_statusbar_set_has_resize_grip (statusbar, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_statusbar_get_property (GObject    *object, 
			    guint       prop_id, 
			    GValue     *value, 
			    GParamSpec *pspec)
{
  GtkStatusbar *statusbar = GTK_STATUSBAR (object);
	
  switch (prop_id) 
    {
    case PROP_HAS_RESIZE_GRIP:
      g_value_set_boolean (value, statusbar->has_resize_grip);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static GdkWindowEdge
get_grip_edge (GtkStatusbar *statusbar)
{
  GtkWidget *widget = GTK_WIDGET (statusbar);

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR) 
    return GDK_WINDOW_EDGE_SOUTH_EAST; 
  else
    return GDK_WINDOW_EDGE_SOUTH_WEST; 
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

  if (w > widget->allocation.width)
    w = widget->allocation.width;

  if (h > widget->allocation.height - widget->style->ythickness)
    h = widget->allocation.height - widget->style->ythickness;
  
  rect->width = w;
  rect->height = h;
  rect->y = widget->allocation.y + widget->allocation.height - h;

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR) 
    rect->x = widget->allocation.x + widget->allocation.width - w;
  else 
    rect->x = widget->allocation.x + widget->style->xthickness;
}

static void
set_grip_cursor (GtkStatusbar *statusbar)
{
  if (statusbar->has_resize_grip)
    {
      GtkWidget *widget = GTK_WIDGET (statusbar);
      GdkDisplay *display = gtk_widget_get_display (widget);
      GdkCursorType cursor_type;
      GdkCursor *cursor;
      
      if (GTK_WIDGET_IS_SENSITIVE (widget))
        {
          if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
	    cursor_type = GDK_BOTTOM_RIGHT_CORNER;
          else
	    cursor_type = GDK_BOTTOM_LEFT_CORNER;

          cursor = gdk_cursor_new_for_display (display, cursor_type);
          gdk_window_set_cursor (statusbar->grip_window, cursor);
          gdk_cursor_unref (cursor);
        }
      else
        gdk_window_set_cursor (statusbar->grip_window, NULL);
    }
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

  set_grip_cursor (statusbar);
}

static void
gtk_statusbar_direction_changed (GtkWidget        *widget,
				 GtkTextDirection  prev_dir)
{
  GtkStatusbar *statusbar = GTK_STATUSBAR (widget);

  set_grip_cursor (statusbar);
}

static void
gtk_statusbar_state_changed (GtkWidget    *widget,
	                     GtkStateType  previous_state)   
{
  GtkStatusbar *statusbar = GTK_STATUSBAR (widget);

  set_grip_cursor (statusbar);
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
  
  (* GTK_WIDGET_CLASS (gtk_statusbar_parent_class)->realize) (widget);

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
  
  (* GTK_WIDGET_CLASS (gtk_statusbar_parent_class)->unrealize) (widget);
}

static void
gtk_statusbar_map (GtkWidget *widget)
{
  GtkStatusbar *statusbar;

  statusbar = GTK_STATUSBAR (widget);
  
  (* GTK_WIDGET_CLASS (gtk_statusbar_parent_class)->map) (widget);
  
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
  
  (* GTK_WIDGET_CLASS (gtk_statusbar_parent_class)->unmap) (widget);
}

static gboolean
gtk_statusbar_button_press (GtkWidget      *widget,
                            GdkEventButton *event)
{
  GtkStatusbar *statusbar;
  GtkWidget *ancestor;
  GdkWindowEdge edge;
  
  statusbar = GTK_STATUSBAR (widget);
  
  if (!statusbar->has_resize_grip ||
      event->type != GDK_BUTTON_PRESS ||
      event->window != statusbar->grip_window)
    return FALSE;
  
  ancestor = gtk_widget_get_toplevel (widget);

  if (!GTK_IS_WINDOW (ancestor))
    return FALSE;

  edge = get_grip_edge (statusbar);

  if (event->button == 1)
    gtk_window_begin_resize_drag (GTK_WINDOW (ancestor),
                                  edge,
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

  GTK_WIDGET_CLASS (gtk_statusbar_parent_class)->expose_event (widget, event);

  if (statusbar->has_resize_grip)
    {
      GdkWindowEdge edge;
      
      edge = get_grip_edge (statusbar);

      get_grip_rect (statusbar, &rect);

      gtk_paint_resize_grip (widget->style,
                             widget->window,
                             GTK_WIDGET_STATE (widget),
                             NULL,
                             widget,
                             "statusbar",
                             edge,
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
gtk_statusbar_size_request (GtkWidget      *widget,
			    GtkRequisition *requisition)
{
  GtkStatusbar *statusbar;
  GtkShadowType shadow_type;
  
  statusbar = GTK_STATUSBAR (widget);

  gtk_widget_style_get (GTK_WIDGET (statusbar), "shadow-type", &shadow_type, NULL);  
  gtk_frame_set_shadow_type (GTK_FRAME (statusbar->frame), shadow_type);
  
  GTK_WIDGET_CLASS (gtk_statusbar_parent_class)->size_request (widget, requisition);
}

/* look for extra children between the frame containing
 * the label and where we want to draw the resize grip 
 */
static gboolean
has_extra_children (GtkStatusbar *statusbar)
{
  GList *l;
  GtkBoxChild *child, *frame;

  frame = NULL;
  for (l = GTK_BOX (statusbar)->children; l; l = l->next)
    {
      frame = l->data;

      if (frame->widget == statusbar->frame)
	break;
    }
  
  for (l = l->next; l; l = l->next)
    {
      child = l->data;

      if (!GTK_WIDGET_VISIBLE (child->widget))
	continue;

      if (frame->pack == GTK_PACK_START || child->pack == GTK_PACK_END)
	return TRUE;
    }

  return FALSE;
}

static void
gtk_statusbar_size_allocate  (GtkWidget     *widget,
                              GtkAllocation *allocation)
{
  GtkStatusbar *statusbar = GTK_STATUSBAR (widget);
  gboolean extra_children = FALSE;
  GdkRectangle rect;

  if (statusbar->has_resize_grip)
    {
      widget->allocation = *allocation;
      get_grip_rect (statusbar, &rect);    
      
      extra_children = has_extra_children (statusbar);
      
      /* If there are extra children, we don't want them to occupy
       * the space where we draw the resize grip, so we temporarily
       * shrink the allocation.
       * If there are no extra children, we want the frame to get
       * the full allocation, and we fix up the allocation of the
       * label afterwards to make room for the grip.
       */
      if (extra_children)
	{
	  allocation->width -= rect.width;
	  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) 
	    allocation->x += rect.width;
	}
    }

  /* chain up normally */
  GTK_WIDGET_CLASS (gtk_statusbar_parent_class)->size_allocate (widget, allocation);

  if (statusbar->has_resize_grip)
    {
      if (statusbar->grip_window)
	{
	  gdk_window_raise (statusbar->grip_window);
	  gdk_window_move_resize (statusbar->grip_window,
				  rect.x, rect.y,
				  rect.width, rect.height);
	}

      if (extra_children) 
	{
	  allocation->width += rect.width;
	  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) 
	    allocation->x -= rect.width;
	  
	  widget->allocation = *allocation;
	}
      else
	{
	  if (statusbar->label->allocation.width + rect.width > statusbar->frame->allocation.width)
	    {
	      /* shrink the label to make room for the grip */
	      *allocation = statusbar->label->allocation;
	      allocation->width = MAX (1, allocation->width - rect.width);
	      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) 
		allocation->x += statusbar->label->allocation.width - allocation->width;

	      gtk_widget_size_allocate (statusbar->label, allocation);
	    }
	}
    }
}

static void
label_selectable_changed (GtkWidget  *label,
			  GParamSpec *pspec,
			  gpointer    data)
{
  GtkStatusbar *statusbar = GTK_STATUSBAR (data);

  if (statusbar && 
      statusbar->has_resize_grip && statusbar->grip_window)
    gdk_window_raise (statusbar->grip_window);
}

#define __GTK_STATUSBAR_C__
#include "gtkaliasdef.c"
