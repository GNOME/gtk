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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "gtklabel.h"
#include "gtkmain.h"
#include "gtkwidget.h"
#include "gtkwindow.h"
#include "gtksignal.h"
#include "gtkstyle.h"
#include "gtktooltips.h"


#define DEFAULT_DELAY 500           /* Default delay in ms */

static void gtk_tooltips_class_init        (GtkTooltipsClass *klass);
static void gtk_tooltips_init              (GtkTooltips      *tooltips);
static void gtk_tooltips_destroy           (GtkObject        *object);

static gint gtk_tooltips_event_handler     (GtkWidget   *widget,
                                            GdkEvent    *event);
static void gtk_tooltips_widget_unmap      (GtkWidget   *widget,
                                            gpointer     data);
static void gtk_tooltips_widget_remove     (GtkWidget   *widget,
                                            gpointer     data);
static void gtk_tooltips_set_active_widget (GtkTooltips *tooltips,
                                            GtkWidget   *widget);
static gint gtk_tooltips_timeout           (gpointer     data);

static gint gtk_tooltips_paint_window      (GtkTooltips *tooltips);
static void gtk_tooltips_draw_tips         (GtkTooltips *tooltips);

static GtkDataClass *parent_class;
static const gchar  *tooltips_data_key = "_GtkTooltipsData";

GtkType
gtk_tooltips_get_type (void)
{
  static GtkType tooltips_type = 0;

  if (!tooltips_type)
    {
      static const GtkTypeInfo tooltips_info =
      {
	"GtkTooltips",
	sizeof (GtkTooltips),
	sizeof (GtkTooltipsClass),
	(GtkClassInitFunc) gtk_tooltips_class_init,
	(GtkObjectInitFunc) gtk_tooltips_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      tooltips_type = gtk_type_unique (GTK_TYPE_DATA, &tooltips_info);
    }

  return tooltips_type;
}

static void
gtk_tooltips_class_init (GtkTooltipsClass *class)
{
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass*) class;
  parent_class = gtk_type_class (GTK_TYPE_DATA);

  object_class->destroy = gtk_tooltips_destroy;
}

static void
gtk_tooltips_init (GtkTooltips *tooltips)
{
  tooltips->tip_window = NULL;
  tooltips->active_tips_data = NULL;
  tooltips->tips_data_list = NULL;
  
  tooltips->delay = DEFAULT_DELAY;
  tooltips->enabled = TRUE;
  tooltips->timer_tag = 0;
}

GtkTooltips *
gtk_tooltips_new (void)
{
  return gtk_type_new (GTK_TYPE_TOOLTIPS);
}

static void
gtk_tooltips_destroy_data (GtkTooltipsData *tooltipsdata)
{
  g_free (tooltipsdata->tip_text);
  g_free (tooltipsdata->tip_private);
  gtk_signal_disconnect_by_data (GTK_OBJECT (tooltipsdata->widget),
 				 (gpointer) tooltipsdata);
  gtk_object_remove_data (GTK_OBJECT (tooltipsdata->widget), tooltips_data_key);
  gtk_widget_unref (tooltipsdata->widget);
  g_free (tooltipsdata);
}

static void
gtk_tooltips_destroy (GtkObject *object)
{
  GtkTooltips *tooltips = GTK_TOOLTIPS (object);
  GList *current;
  GtkTooltipsData *tooltipsdata;

  g_return_if_fail (tooltips != NULL);

  if (tooltips->timer_tag)
    {
      gtk_timeout_remove (tooltips->timer_tag);
      tooltips->timer_tag = 0;
    }

  if (tooltips->tips_data_list != NULL)
    {
      current = g_list_first (tooltips->tips_data_list);
      while (current != NULL)
	{
	  tooltipsdata = (GtkTooltipsData*) current->data;
	  current = current->next;
	  gtk_tooltips_widget_remove (tooltipsdata->widget, tooltipsdata);
	}
    }

  if (tooltips->tip_window)
    gtk_widget_destroy (tooltips->tip_window);
}

void
gtk_tooltips_force_window (GtkTooltips *tooltips)
{
  g_return_if_fail (tooltips != NULL);
  g_return_if_fail (GTK_IS_TOOLTIPS (tooltips));

  if (!tooltips->tip_window)
    {
      tooltips->tip_window = gtk_window_new (GTK_WINDOW_POPUP);
      gtk_widget_set_app_paintable (tooltips->tip_window, TRUE);
      gtk_window_set_policy (GTK_WINDOW (tooltips->tip_window), FALSE, FALSE, TRUE);
      gtk_widget_set_name (tooltips->tip_window, "gtk-tooltips");
      gtk_container_set_border_width (GTK_CONTAINER (tooltips->tip_window), 4);

      gtk_signal_connect_object (GTK_OBJECT (tooltips->tip_window), 
				 "expose_event",
				 GTK_SIGNAL_FUNC (gtk_tooltips_paint_window), 
				 GTK_OBJECT (tooltips));
      gtk_signal_connect_object (GTK_OBJECT (tooltips->tip_window), 
				 "draw",
				 GTK_SIGNAL_FUNC (gtk_tooltips_paint_window), 
				 GTK_OBJECT (tooltips));

      tooltips->tip_label = gtk_label_new (NULL);
      gtk_label_set_line_wrap (GTK_LABEL (tooltips->tip_label), TRUE);
      gtk_misc_set_alignment (GTK_MISC (tooltips->tip_label), 0.5, 0.5);
      gtk_widget_show (tooltips->tip_label);
      
      gtk_container_add (GTK_CONTAINER (tooltips->tip_window), tooltips->tip_label);

      gtk_signal_connect (GTK_OBJECT (tooltips->tip_window),
			  "destroy",
			  gtk_widget_destroyed,
			  &tooltips->tip_window);
    }
}

void
gtk_tooltips_enable (GtkTooltips *tooltips)
{
  g_return_if_fail (tooltips != NULL);

  tooltips->enabled = TRUE;
}

void
gtk_tooltips_disable (GtkTooltips *tooltips)
{
  g_return_if_fail (tooltips != NULL);

  gtk_tooltips_set_active_widget (tooltips, NULL);

  tooltips->enabled = FALSE;
}

void
gtk_tooltips_set_delay (GtkTooltips *tooltips,
                        guint         delay)
{
  g_return_if_fail (tooltips != NULL);

  tooltips->delay = delay;
}

GtkTooltipsData*
gtk_tooltips_data_get (GtkWidget       *widget)
{
  g_return_val_if_fail (widget != NULL, NULL);

  return gtk_object_get_data ((GtkObject*) widget, tooltips_data_key);
}

void
gtk_tooltips_set_tip (GtkTooltips *tooltips,
		      GtkWidget   *widget,
		      const gchar *tip_text,
		      const gchar *tip_private)
{
  GtkTooltipsData *tooltipsdata;

  g_return_if_fail (tooltips != NULL);
  g_return_if_fail (GTK_IS_TOOLTIPS (tooltips));
  g_return_if_fail (widget != NULL);

  tooltipsdata = gtk_tooltips_data_get (widget);
  if (tooltipsdata)
    gtk_tooltips_widget_remove (tooltipsdata->widget, tooltipsdata);

  if (!tip_text)
    return;

  tooltipsdata = g_new0 (GtkTooltipsData, 1);

  if (tooltipsdata != NULL)
    {
      tooltipsdata->tooltips = tooltips;
      tooltipsdata->widget = widget;
      gtk_widget_ref (widget);

      tooltipsdata->tip_text = g_strdup (tip_text);
      tooltipsdata->tip_private = g_strdup (tip_private);

      tooltips->tips_data_list = g_list_append (tooltips->tips_data_list,
                                             tooltipsdata);
      gtk_signal_connect_after(GTK_OBJECT (widget), "event",
                               (GtkSignalFunc) gtk_tooltips_event_handler,
 	                       (gpointer) tooltipsdata);

      gtk_object_set_data (GTK_OBJECT (widget), tooltips_data_key,
                           (gpointer) tooltipsdata);

      gtk_signal_connect (GTK_OBJECT (widget), "unmap",
                          (GtkSignalFunc) gtk_tooltips_widget_unmap,
                          (gpointer) tooltipsdata);

      gtk_signal_connect (GTK_OBJECT (widget), "unrealize",
                          (GtkSignalFunc) gtk_tooltips_widget_unmap,
                          (gpointer) tooltipsdata);

      gtk_signal_connect (GTK_OBJECT (widget), "destroy",
			  (GtkSignalFunc) gtk_tooltips_widget_remove,
			  (gpointer) tooltipsdata);
    }
}

void
gtk_tooltips_set_colors (GtkTooltips *tooltips,
			 GdkColor    *background,
			 GdkColor    *foreground)
{
  g_return_if_fail (tooltips != NULL);

  g_warning ("gtk_tooltips_set_colors is deprecated and does nothing.\n"
	     "The colors for tooltips are now taken from the style.");
}

static gint
gtk_tooltips_paint_window (GtkTooltips *tooltips)
{
  gtk_paint_flat_box(tooltips->tip_window->style, tooltips->tip_window->window,
		     GTK_STATE_NORMAL, GTK_SHADOW_OUT, 
		     NULL, GTK_WIDGET(tooltips->tip_window), "tooltip",
		     0, 0, -1, -1);

  return TRUE;
}

static void
gtk_tooltips_draw_tips (GtkTooltips * tooltips)
{
  GtkRequisition requisition;
  GtkWidget *widget;
  GtkStyle *style;
  gint x, y, w, h, scr_w, scr_h;
  GtkTooltipsData *data;

  if (!tooltips->tip_window)
    gtk_tooltips_force_window (tooltips);
  else if (GTK_WIDGET_VISIBLE (tooltips->tip_window))
    gtk_widget_hide (tooltips->tip_window);

  gtk_widget_ensure_style (tooltips->tip_window);
  style = tooltips->tip_window->style;
  
  widget = tooltips->active_tips_data->widget;

  scr_w = gdk_screen_width ();
  scr_h = gdk_screen_height ();

  data = tooltips->active_tips_data;

  gtk_label_set_text (GTK_LABEL (tooltips->tip_label), data->tip_text);

  gtk_widget_size_request (tooltips->tip_window, &requisition);
  w = requisition.width;
  h = requisition.height;

  gdk_window_get_pointer (NULL, &x, NULL, NULL);
  gdk_window_get_origin (widget->window, NULL, &y);
  if (GTK_WIDGET_NO_WINDOW (widget))
    y += widget->allocation.y;

  x -= (w / 2 + 4);

  if ((x + w) > scr_w)
    x -= (x + w) - scr_w;
  else if (x < 0)
    x = 0;

  if ((y + h + widget->allocation.height + 4) > scr_h)
    y = y - h - 4;
  else
    y = y + widget->allocation.height + 4;

  gtk_widget_popup (tooltips->tip_window, x, y);
}

static gint
gtk_tooltips_timeout (gpointer data)
{
  GtkTooltips *tooltips = (GtkTooltips *) data;

  GDK_THREADS_ENTER ();
  
  if (tooltips->active_tips_data != NULL &&
      GTK_WIDGET_DRAWABLE (tooltips->active_tips_data->widget))
    gtk_tooltips_draw_tips (tooltips);

  GDK_THREADS_LEAVE ();

  return FALSE;
}

static void
gtk_tooltips_set_active_widget (GtkTooltips *tooltips,
                                GtkWidget   *widget)
{
  if (tooltips->tip_window)
    gtk_widget_hide (tooltips->tip_window);
  if (tooltips->timer_tag)
    {
      gtk_timeout_remove (tooltips->timer_tag);
      tooltips->timer_tag = 0;
    }
  
  tooltips->active_tips_data = NULL;
  
  if (widget)
    {
      GList *list;
      
      for (list = tooltips->tips_data_list; list; list = list->next)
	{
	  GtkTooltipsData *tooltipsdata;
	  
	  tooltipsdata = list->data;
	  
	  if (tooltipsdata->widget == widget &&
	      GTK_WIDGET_DRAWABLE (widget))
	    {
	      tooltips->active_tips_data = tooltipsdata;
	      break;
	    }
	}
    }
}

static gint
gtk_tooltips_event_handler (GtkWidget *widget,
                            GdkEvent  *event)
{
  GtkTooltips *tooltips;
  GtkTooltipsData *old_tips_data;
  GtkWidget *event_widget;

  if ((event->type == GDK_LEAVE_NOTIFY || event->type == GDK_ENTER_NOTIFY) &&
      event->crossing.detail == GDK_NOTIFY_INFERIOR)
    return FALSE;

  event_widget = gtk_get_event_widget (event);
  if (event_widget != widget)
    return FALSE;
  
  old_tips_data = gtk_tooltips_data_get (widget);
  tooltips = old_tips_data->tooltips;

  switch (event->type)
    {
    case GDK_MOTION_NOTIFY:
    case GDK_EXPOSE:
      /* do nothing */
      break;
      
    case GDK_ENTER_NOTIFY:
      old_tips_data = tooltips->active_tips_data;
      if (tooltips->enabled &&
	  (!old_tips_data || old_tips_data->widget != widget))
	{
	  gtk_tooltips_set_active_widget (tooltips, widget);
	  
	  tooltips->timer_tag = gtk_timeout_add (tooltips->delay,
						 gtk_tooltips_timeout,
						 (gpointer) tooltips);
	}
      break;

    default:
      gtk_tooltips_set_active_widget (tooltips, NULL);
      break;
    }

  return FALSE;
}

static void
gtk_tooltips_widget_unmap (GtkWidget *widget,
			   gpointer   data)
{
  GtkTooltipsData *tooltipsdata = (GtkTooltipsData *)data;
  GtkTooltips *tooltips = tooltipsdata->tooltips;
  
  if (tooltips->active_tips_data &&
      (tooltips->active_tips_data->widget == widget))
    gtk_tooltips_set_active_widget (tooltips, NULL);
}

static void
gtk_tooltips_widget_remove (GtkWidget *widget,
			    gpointer   data)
{
  GtkTooltipsData *tooltipsdata = (GtkTooltipsData*) data;
  GtkTooltips *tooltips = tooltipsdata->tooltips;

  gtk_tooltips_widget_unmap (widget, data);
  tooltips->tips_data_list = g_list_remove (tooltips->tips_data_list,
					    tooltipsdata);
  gtk_tooltips_destroy_data (tooltipsdata);
}
