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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
static void gtk_tooltips_create_window     (GtkTooltips *tooltips);
static void gtk_tooltips_draw_tips         (GtkTooltips *tooltips);

static GtkDataClass *parent_class;
static const gchar  *tooltips_data_key = "_GtkTooltipsData";

guint
gtk_tooltips_get_type ()
{
  static guint tooltips_type = 0;

  if (!tooltips_type)
    {
      GtkTypeInfo tooltips_info =
      {
	"GtkTooltips",
	sizeof (GtkTooltips),
	sizeof (GtkTooltipsClass),
	(GtkClassInitFunc) gtk_tooltips_class_init,
	(GtkObjectInitFunc) gtk_tooltips_init,
	(GtkArgSetFunc) NULL,
	(GtkArgGetFunc) NULL,
      };

      tooltips_type = gtk_type_unique (gtk_data_get_type (), &tooltips_info);
    }

  return tooltips_type;
}

static void
gtk_tooltips_class_init (GtkTooltipsClass *class)
{
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass*) class;
  parent_class = gtk_type_class (gtk_data_get_type ());

  object_class->destroy = gtk_tooltips_destroy;
}

static void
gtk_tooltips_init (GtkTooltips *tooltips)
{
  tooltips->tip_window = NULL;
  tooltips->active_tips_data = NULL;
  tooltips->tips_data_list = NULL;
  tooltips->gc = NULL;
  tooltips->foreground = NULL;
  tooltips->background = NULL;
  
  tooltips->delay = DEFAULT_DELAY;
  tooltips->enabled = TRUE;
  tooltips->timer_tag = 0;
}

GtkTooltips *
gtk_tooltips_new ()
{
  return gtk_type_new (gtk_tooltips_get_type ());
}

void
gtk_tooltips_free_string (gpointer data, gpointer user_data)
{
  if (data)
    g_free (data);
}

static void
gtk_tooltips_destroy_data (GtkTooltipsData *tooltipsdata)
{
  g_free (tooltipsdata->tip_text);
  g_free (tooltipsdata->tip_private);
  g_list_foreach (tooltipsdata->row, gtk_tooltips_free_string, 0);
  if (tooltipsdata->row)
    g_list_free (tooltipsdata->row);
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

  if (tooltips->tip_window != NULL)
    {
      gtk_widget_destroy (tooltips->tip_window);
      gtk_widget_unref (tooltips->tip_window);
      tooltips->tip_window = NULL;
    }

  if (tooltips->gc != NULL)
    {
      gdk_gc_destroy (tooltips->gc);
      tooltips->gc = NULL;
    }
}

static void
gtk_tooltips_create_window (GtkTooltips *tooltips)
{
  tooltips->tip_window = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_widget_ref (tooltips->tip_window);
  gtk_window_set_policy (GTK_WINDOW (tooltips->tip_window), FALSE, FALSE, TRUE);
  gtk_widget_realize (tooltips->tip_window);
}

static void
gtk_tooltips_layout_text (GtkTooltips *tooltips, GtkTooltipsData *data)
{
  gchar *row_end, *text, *row_text, *break_pos;
  gint i, row_width, window_width = 0;
  size_t len;

  if (tooltips->tip_window == NULL)
    gtk_tooltips_create_window (tooltips);

  g_list_foreach (data->row, gtk_tooltips_free_string, 0);
  if (data->row)
    g_list_free (data->row);
  data->row = 0;
  data->font = tooltips->tip_window->style->font;
  data->width = 0;

  text = data->tip_text;
  if (!text)
    return;

  while (*text)
    {
      row_end = strchr (text, '\n');
      if (!row_end)
       row_end = strchr (text, '\0');

      len = row_end - text + 1;
      row_text = g_new(gchar, len);
      memcpy (row_text, text, len - 1);
      row_text[len - 1] = '\0';

      /* now either adjust the window's width or shorten the row until
        it fits in the window */

      while (1)
       {
         row_width = gdk_string_width (data->font, row_text);
         if (!window_width)
           {
             /* make an initial guess at window's width: */

             if (row_width > gdk_screen_width () / 4)
               window_width = gdk_screen_width () / 4;
             else
               window_width = row_width;
           }
         if (row_width <= window_width)
           break;

         if (strchr (row_text, ' '))
           {
             /* the row is currently too wide, but we have blanks in
                the row so we can break it into smaller pieces */

             gint avg_width = row_width / strlen (row_text);

             i = window_width;
             if (avg_width)
               i /= avg_width;
             if ((size_t) i >= len)
               i = len - 1;

             break_pos = strchr (row_text + i, ' ');
             if (!break_pos)
               {
                 break_pos = row_text + i;
                 while (*--break_pos != ' ');
               }
             *break_pos = '\0';
           }
         else
           {
             /* we can't break this row into any smaller pieces, so
                we have no choice but to widen the window: */

             window_width = row_width;
             break;
           }
       }
      if (row_width > data->width)
       data->width = row_width;
      data->row = g_list_append (data->row, row_text);
      text += strlen (row_text);
      if (!*text)
       break;

      if (text[0] == '\n' && text[1])
       /* end of paragraph and there is more text to come */
       data->row = g_list_append (data->row, 0);
      ++text;  /* skip blank or newline */
    }
  data->width += 8;    /* leave some border */
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

      gtk_tooltips_layout_text (tooltips, tooltipsdata);
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

  if (background != NULL)
    tooltips->foreground = foreground;
  if (foreground != NULL)
    tooltips->background = background;
}

static void
gtk_tooltips_draw_tips (GtkTooltips * tooltips)
{
  GtkWidget *widget;
  GtkStyle *style;
  gint gap, x, y, w, h, scr_w, scr_h, baseline_skip;
  GtkTooltipsData *data;
  GList *el;

  if (!tooltips->tip_window)
    gtk_tooltips_create_window (tooltips);
  else if (GTK_WIDGET_VISIBLE (tooltips->tip_window))
    gtk_widget_hide (tooltips->tip_window);

  style = tooltips->tip_window->style;
  
  widget = tooltips->active_tips_data->widget;

  scr_w = gdk_screen_width ();
  scr_h = gdk_screen_height ();

  data = tooltips->active_tips_data;
  if (data->font != style->font)
    gtk_tooltips_layout_text (tooltips, data);

  gap = (style->font->ascent + style->font->descent) / 4;
  if (gap < 2)
    gap = 2;
  baseline_skip = style->font->ascent + style->font->descent + gap;
  w = data->width;
  h = 8 - gap;
  for (el = data->row; el; el = el->next)
    if (el->data)
      h += baseline_skip;
    else
      h += baseline_skip / 2;
  if (h < 8)
    h = 8;

  gdk_window_get_pointer (NULL, &x, NULL, NULL);
  gdk_window_get_origin (widget->window, NULL, &y);

  x -= ((w >> 1) + 4);

  if ((x + w) > scr_w)
    x -= (x + w) - scr_w;
  else if (x < 0)
    x = 0;

  if ((y + h + widget->allocation.height + 4) > scr_h)
    y = y - h - 4;
  else
    y = y + widget->allocation.height + 4;

  gtk_widget_set_usize (tooltips->tip_window, w + 1, h + 1);
  gtk_widget_popup (tooltips->tip_window, x, y);

  if (tooltips->gc == NULL)
    tooltips->gc = gdk_gc_new (tooltips->tip_window->window);

  if (tooltips->background != NULL)
    {
      gdk_gc_set_foreground (tooltips->gc, tooltips->background);
      gdk_gc_set_background (tooltips->gc, tooltips->foreground);
    }
  else
    {
      gdk_gc_set_foreground (tooltips->gc, &style->bg[GTK_STATE_NORMAL]);
      gdk_gc_set_background (tooltips->gc, &style->fg[GTK_STATE_NORMAL]);
    }

  gdk_gc_set_font (tooltips->gc, style->font);
  gdk_draw_rectangle (tooltips->tip_window->window, tooltips->gc, TRUE, 0, 0, w, h);

  if (tooltips->foreground != NULL)
    {
      gdk_gc_set_foreground (tooltips->gc, tooltips->foreground);
      gdk_gc_set_background (tooltips->gc, tooltips->background);
    }
  else
    {
      gdk_gc_set_foreground (tooltips->gc, &style->fg[GTK_STATE_NORMAL]);
      gdk_gc_set_background (tooltips->gc, &style->bg[GTK_STATE_NORMAL]);
    }

  gdk_draw_rectangle (tooltips->tip_window->window, tooltips->gc, FALSE, 0, 0, w, h);
  y = style->font->ascent + 4;

  for (el = data->row; el; el = el->next)
    {
      if (el->data)
       {
         gdk_draw_string (tooltips->tip_window->window, style->font,
			  tooltips->gc, 4, y, el->data);
         y += baseline_skip;
       }
      else
       y += baseline_skip / 2;
    }
}

static gint
gtk_tooltips_timeout (gpointer data)
{
  GtkTooltips *tooltips = (GtkTooltips *) data;

  if (tooltips->active_tips_data != NULL &&
      GTK_WIDGET_DRAWABLE (tooltips->active_tips_data->widget))
    gtk_tooltips_draw_tips (tooltips);

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
