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
#define STICKY_DELAY 0              /* Delay before popping up next tip
                                     * if we're sticky
                                     */
#define STICKY_REVERT_DELAY 1000    /* Delay before sticky tooltips revert
				     * to normal
                                     */

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

static GtkObjectClass *parent_class;
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

      tooltips_type = gtk_type_unique (GTK_TYPE_OBJECT, &tooltips_info);
    }

  return tooltips_type;
}

static void
gtk_tooltips_class_init (GtkTooltipsClass *class)
{
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass*) class;
  parent_class = gtk_type_class (GTK_TYPE_OBJECT);

  object_class->destroy = gtk_tooltips_destroy;
}

static void
gtk_tooltips_init (GtkTooltips *tooltips)
{
  static gboolean have_rc = FALSE;
  if (have_rc)
    {
      have_rc = TRUE;
      
      gtk_rc_parse_string ("style \"gtk-default-tooltips-style\" {\n"
			   "  bg[NORMAL] = \"#ffffc0\"\n"
			   "  fg[NORMAL] = \"#000000\"\n"
			   "}\n"
			   "\n"
			   "widget \"gtk-tooltips*\" style : gtk \"gtk-default-tooltips-style\"\n");
    }
  
  tooltips->tip_window = NULL;
  tooltips->active_tips_data = NULL;
  tooltips->tips_data_list = NULL;
  
  tooltips->delay = DEFAULT_DELAY;
  tooltips->enabled = TRUE;
  tooltips->timer_tag = 0;
  tooltips->use_sticky_delay = FALSE;
  tooltips->last_popdown.tv_sec = -1;
  tooltips->last_popdown.tv_usec = -1;
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

      tooltips->tip_label = gtk_label_new (NULL);
      gtk_label_set_line_wrap (GTK_LABEL (tooltips->tip_label), TRUE);
      gtk_misc_set_alignment (GTK_MISC (tooltips->tip_label), 0.5, 0.5);
      gtk_widget_show (tooltips->tip_label);
      
      gtk_container_add (GTK_CONTAINER (tooltips->tip_window), tooltips->tip_label);

      gtk_signal_connect (GTK_OBJECT (tooltips->tip_window),
			  "destroy",
			  GTK_SIGNAL_FUNC (gtk_widget_destroyed),
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

  g_return_if_fail (GTK_IS_TOOLTIPS (tooltips));
  g_return_if_fail (widget != NULL);

  tooltipsdata = gtk_tooltips_data_get (widget);

  if (!tip_text)
    {
      if (tooltipsdata)
	gtk_tooltips_widget_remove (tooltipsdata->widget, tooltipsdata);
      return;
    }
  
  if (tooltips->active_tips_data 
      && tooltips->active_tips_data->widget == widget
      && GTK_WIDGET_DRAWABLE (tooltips->active_tips_data->widget))
    {
      g_free (tooltipsdata->tip_text);
      g_free (tooltipsdata->tip_private);

      tooltipsdata->tip_text = g_strdup (tip_text);
      tooltipsdata->tip_private = g_strdup (tip_private);
      
      gtk_tooltips_draw_tips (tooltips);
    }
  else 
    {
      gtk_widget_ref (widget);
      
      if (tooltipsdata)
        gtk_tooltips_widget_remove (tooltipsdata->widget, tooltipsdata);
      
      tooltipsdata = g_new0 (GtkTooltipsData, 1);
      
      tooltipsdata->tooltips = tooltips;
      tooltipsdata->widget = widget;

      tooltipsdata->tip_text = g_strdup (tip_text);
      tooltipsdata->tip_private = g_strdup (tip_private);

      tooltips->tips_data_list = g_list_append (tooltips->tips_data_list,
                                                tooltipsdata);
      gtk_signal_connect_after (GTK_OBJECT (widget), "event",
                               (GtkSignalFunc) gtk_tooltips_event_handler,
                               tooltipsdata);

      gtk_object_set_data (GTK_OBJECT (widget), tooltips_data_key,
                           tooltipsdata);

      gtk_signal_connect (GTK_OBJECT (widget), "unmap",
                          (GtkSignalFunc) gtk_tooltips_widget_unmap,
                          tooltipsdata);

      gtk_signal_connect (GTK_OBJECT (widget), "unrealize",
                          (GtkSignalFunc) gtk_tooltips_widget_unmap,
                          tooltipsdata);

      gtk_signal_connect (GTK_OBJECT (widget), "destroy",
                          (GtkSignalFunc) gtk_tooltips_widget_remove,
                          tooltipsdata);
    }
}

static gint
gtk_tooltips_paint_window (GtkTooltips *tooltips)
{
  gtk_paint_flat_box (tooltips->tip_window->style, tooltips->tip_window->window,
		      GTK_STATE_NORMAL, GTK_SHADOW_OUT, 
		      NULL, GTK_WIDGET(tooltips->tip_window), "tooltip",
		      0, 0, -1, -1);

  return FALSE;
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
    g_get_current_time (&tooltips->last_popdown);

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

  gtk_window_move (GTK_WINDOW (tooltips->tip_window), x, y);
  gtk_widget_show (tooltips->tip_window);
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
    {
      if (GTK_WIDGET_VISIBLE (tooltips->tip_window))
	g_get_current_time (&tooltips->last_popdown);
      gtk_widget_hide (tooltips->tip_window);
    }
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
  else
    {
      tooltips->use_sticky_delay = FALSE;
    }
}

static gboolean
gtk_tooltips_recently_shown (GtkTooltips *tooltips)
{
  GTimeVal now;
  glong msec;
  
  g_get_current_time (&now);
  msec = (now.tv_sec  - tooltips->last_popdown.tv_sec) * 1000 +
	  (now.tv_usec - tooltips->last_popdown.tv_usec) / 1000;
  return (msec < STICKY_REVERT_DELAY);
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
	  guint delay;
	  
	  gtk_tooltips_set_active_widget (tooltips, widget);
	  
	  if (tooltips->use_sticky_delay  &&
	      gtk_tooltips_recently_shown (tooltips))
	    delay = STICKY_DELAY;
	  else
	    delay = tooltips->delay;
	  tooltips->timer_tag = gtk_timeout_add (delay,
						 gtk_tooltips_timeout,
						 (gpointer) tooltips);
	}
      break;

    case GDK_LEAVE_NOTIFY:
      {
	gboolean use_sticky_delay;

	use_sticky_delay = tooltips->tip_window &&
		GTK_WIDGET_VISIBLE (tooltips->tip_window);
	gtk_tooltips_set_active_widget (tooltips, NULL);
	tooltips->use_sticky_delay = use_sticky_delay;
      }
      break;
      
    default:
      gtk_tooltips_set_active_widget (tooltips, NULL);
      return FALSE;
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

gboolean
_gtk_tooltips_show_tip (GtkWidget *widget)
{
  /* Showing the tip from the keyboard */

  /* FIXME this function is completely broken right now,
   * popdown doesn't occur when it should.
   */
  
  GtkTooltipsData *tooltipsdata;

  tooltipsdata = gtk_tooltips_data_get (widget);

  if (tooltipsdata == NULL)
    return FALSE;

  gtk_tooltips_set_active_widget (tooltipsdata->tooltips,
                                  widget);

  gtk_tooltips_timeout (tooltipsdata->tooltips);
  return TRUE;
}
