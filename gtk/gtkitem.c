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

#include "gtkitem.h"
#include "gtksignal.h"


enum {
  SELECT,
  DESELECT,
  TOGGLE,
  LAST_SIGNAL
};


static void gtk_item_class_init (GtkItemClass     *klass);
static void gtk_item_init       (GtkItem          *item);
static void gtk_item_map        (GtkWidget        *widget);
static void gtk_item_unmap      (GtkWidget        *widget);
static void gtk_item_realize    (GtkWidget        *widget);
static gint gtk_item_enter      (GtkWidget        *widget,
				 GdkEventCrossing *event);
static gint gtk_item_leave      (GtkWidget        *widget,
				 GdkEventCrossing *event);


static guint item_signals[LAST_SIGNAL] = { 0 };


GtkType
gtk_item_get_type (void)
{
  static GtkType item_type = 0;

  if (!item_type)
    {
      static const GtkTypeInfo item_info =
      {
	"GtkItem",
	sizeof (GtkItem),
	sizeof (GtkItemClass),
	(GtkClassInitFunc) gtk_item_class_init,
	(GtkObjectInitFunc) gtk_item_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      item_type = gtk_type_unique (GTK_TYPE_BIN, &item_info);
    }

  return item_type;
}

static void
gtk_item_class_init (GtkItemClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;

  item_signals[SELECT] =
    gtk_signal_new ("select",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkItemClass, select),
                    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  item_signals[DESELECT] =
    gtk_signal_new ("deselect",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkItemClass, deselect),
                    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  item_signals[TOGGLE] =
    gtk_signal_new ("toggle",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkItemClass, toggle),
                    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);

  gtk_object_class_add_signals (object_class, item_signals, LAST_SIGNAL);

  widget_class->activate_signal = item_signals[TOGGLE];
  widget_class->map = gtk_item_map;
  widget_class->unmap = gtk_item_unmap;
  widget_class->realize = gtk_item_realize;
  widget_class->enter_notify_event = gtk_item_enter;
  widget_class->leave_notify_event = gtk_item_leave;

  class->select = NULL;
  class->deselect = NULL;
  class->toggle = NULL;
}

static void
gtk_item_init (GtkItem *item)
{
  GTK_WIDGET_UNSET_FLAGS (item, GTK_NO_WINDOW);
}

void
gtk_item_select (GtkItem *item)
{
  gtk_signal_emit (GTK_OBJECT (item), item_signals[SELECT]);
}

void
gtk_item_deselect (GtkItem *item)
{
  gtk_signal_emit (GTK_OBJECT (item), item_signals[DESELECT]);
}

void
gtk_item_toggle (GtkItem *item)
{
  gtk_signal_emit (GTK_OBJECT (item), item_signals[TOGGLE]);
}


static void
gtk_item_map (GtkWidget *widget)
{
  GtkBin *bin;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ITEM (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);

  bin = GTK_BIN (widget);

  if (bin->child &&
      GTK_WIDGET_VISIBLE (bin->child) &&
      !GTK_WIDGET_MAPPED (bin->child))
    gtk_widget_map (bin->child);

  gdk_window_show (widget->window);
}

static void
gtk_item_unmap (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ITEM (widget));

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);

  gdk_window_hide (widget->window);
}

static void
gtk_item_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ITEM (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = (gtk_widget_get_events (widget) |
			   GDK_EXPOSURE_MASK |
			   GDK_BUTTON_PRESS_MASK |
			   GDK_BUTTON_RELEASE_MASK |
			   GDK_ENTER_NOTIFY_MASK |
			   GDK_LEAVE_NOTIFY_MASK |
			   GDK_POINTER_MOTION_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
   gdk_window_set_back_pixmap (widget->window, NULL, TRUE);
}

static gint
gtk_item_enter (GtkWidget        *widget,
		GdkEventCrossing *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_ITEM (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  return gtk_widget_event (widget->parent, (GdkEvent*) event);
}

static gint
gtk_item_leave (GtkWidget        *widget,
		GdkEventCrossing *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_ITEM (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  return gtk_widget_event (widget->parent, (GdkEvent*) event);
}

