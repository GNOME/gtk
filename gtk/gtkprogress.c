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

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "gtkprogress.h" 
#include "gtksignal.h"

#define EPSILON  1e-5

enum {
  ARG_0,
  ARG_ACTIVITY_MODE,
  ARG_SHOW_TEXT,
  ARG_TEXT_XALIGN,
  ARG_TEXT_YALIGN
};


static void gtk_progress_class_init      (GtkProgressClass *klass);
static void gtk_progress_init            (GtkProgress      *progress);
static void gtk_progress_set_arg	 (GtkObject        *object,
					  GtkArg           *arg,
					  guint             arg_id);
static void gtk_progress_get_arg	 (GtkObject        *object,
					  GtkArg           *arg,
					  guint             arg_id);
static void gtk_progress_destroy         (GtkObject        *object);
static void gtk_progress_finalize        (GtkObject        *object);
static void gtk_progress_realize         (GtkWidget        *widget);
static gint gtk_progress_expose          (GtkWidget        *widget,
				 	  GdkEventExpose   *event);
static void gtk_progress_size_allocate   (GtkWidget        *widget,
				 	  GtkAllocation    *allocation);
static void gtk_progress_create_pixmap   (GtkProgress      *progress);


static GtkWidgetClass *parent_class = NULL;


GtkType
gtk_progress_get_type (void)
{
  static GtkType progress_type = 0;

  if (!progress_type)
    {
      static const GtkTypeInfo progress_info =
      {
	"GtkProgress",
	sizeof (GtkProgress),
	sizeof (GtkProgressClass),
	(GtkClassInitFunc) gtk_progress_class_init,
	(GtkObjectInitFunc) gtk_progress_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL
      };

      progress_type = gtk_type_unique (GTK_TYPE_WIDGET, &progress_info);
    }

  return progress_type;
}

static void
gtk_progress_class_init (GtkProgressClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass *) class;
  widget_class = (GtkWidgetClass *) class;
  parent_class = gtk_type_class (GTK_TYPE_WIDGET);

  gtk_object_add_arg_type ("GtkProgress::activity_mode",
			   GTK_TYPE_BOOL,
			   GTK_ARG_READWRITE,
			   ARG_ACTIVITY_MODE);
  gtk_object_add_arg_type ("GtkProgress::show_text",
			   GTK_TYPE_BOOL,
			   GTK_ARG_READWRITE,
			   ARG_SHOW_TEXT);
  gtk_object_add_arg_type ("GtkProgress::text_xalign",
			   GTK_TYPE_FLOAT,
			   GTK_ARG_READWRITE,
			   ARG_TEXT_XALIGN);
  gtk_object_add_arg_type ("GtkProgress::text_yalign",
			   GTK_TYPE_FLOAT,
			   GTK_ARG_READWRITE,
			   ARG_TEXT_YALIGN);

  object_class->set_arg = gtk_progress_set_arg;
  object_class->get_arg = gtk_progress_get_arg;
  object_class->destroy = gtk_progress_destroy;
  object_class->finalize = gtk_progress_finalize;

  widget_class->realize = gtk_progress_realize;
  widget_class->expose_event = gtk_progress_expose;
  widget_class->size_allocate = gtk_progress_size_allocate;

  /* to be overridden */
  class->paint = NULL;
  class->update = NULL;
  class->act_mode_enter = NULL;
}

static void
gtk_progress_set_arg (GtkObject      *object,
		      GtkArg         *arg,
		      guint           arg_id)
{
  GtkProgress *progress;
  
  progress = GTK_PROGRESS (object);

  switch (arg_id)
    {
    case ARG_ACTIVITY_MODE:
      gtk_progress_set_activity_mode (progress, GTK_VALUE_BOOL (*arg));
      break;
    case ARG_SHOW_TEXT:
      gtk_progress_set_show_text (progress, GTK_VALUE_BOOL (*arg));
      break;
    case ARG_TEXT_XALIGN:
      gtk_progress_set_text_alignment (progress, GTK_VALUE_FLOAT (*arg), progress->y_align);
      break;
    case ARG_TEXT_YALIGN:
      gtk_progress_set_text_alignment (progress, progress->x_align, GTK_VALUE_FLOAT (*arg));
      break;
    default:
      break;
    }
}

static void
gtk_progress_get_arg (GtkObject      *object,
		      GtkArg         *arg,
		      guint           arg_id)
{
  GtkProgress *progress;
  
  progress = GTK_PROGRESS (object);

  switch (arg_id)
    {
    case ARG_ACTIVITY_MODE:
      GTK_VALUE_BOOL (*arg) = (progress->activity_mode != 0);
      break;
    case ARG_SHOW_TEXT:
      GTK_VALUE_BOOL (*arg) = (progress->show_text != 0);
      break;
    case ARG_TEXT_XALIGN:
      GTK_VALUE_FLOAT (*arg) = progress->x_align;
      break;
    case ARG_TEXT_YALIGN:
      GTK_VALUE_FLOAT (*arg) = progress->y_align;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

static void
gtk_progress_init (GtkProgress *progress)
{
  progress->adjustment = NULL;
  progress->offscreen_pixmap = NULL;
  progress->format = g_strdup ("%P %%");
  progress->x_align = 0.5;
  progress->y_align = 0.5;
  progress->show_text = FALSE;
  progress->activity_mode = FALSE;
}

static void
gtk_progress_realize (GtkWidget *widget)
{
  GtkProgress *progress;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_PROGRESS (widget));

  progress = GTK_PROGRESS (widget);
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= GDK_EXPOSURE_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
				   &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, progress);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_ACTIVE);

  gtk_progress_create_pixmap (progress);
}

static void
gtk_progress_destroy (GtkObject *object)
{
  GtkProgress *progress;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_PROGRESS (object));

  progress = GTK_PROGRESS (object);

  if (progress->adjustment)
    gtk_signal_disconnect_by_data (GTK_OBJECT (progress->adjustment),
				   progress);

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
gtk_progress_finalize (GtkObject *object)
{
  GtkProgress *progress;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_PROGRESS (object));

  progress = GTK_PROGRESS (object);

  if (progress->adjustment)
    gtk_object_unref (GTK_OBJECT (GTK_PROGRESS (object)->adjustment));
  
  if (progress->offscreen_pixmap)
    gdk_pixmap_unref (progress->offscreen_pixmap);

  if (progress->format)
    g_free (progress->format);

  GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

static gint
gtk_progress_expose (GtkWidget      *widget,
		     GdkEventExpose *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_PROGRESS (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    gdk_draw_pixmap (widget->window,
		     widget->style->black_gc,
		     GTK_PROGRESS (widget)->offscreen_pixmap,
		     event->area.x, event->area.y,
		     event->area.x, event->area.y,
		     event->area.width,
		     event->area.height);

  return FALSE;
}

static void
gtk_progress_size_allocate (GtkWidget     *widget,
			    GtkAllocation *allocation)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_PROGRESS (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);

      gtk_progress_create_pixmap (GTK_PROGRESS (widget));
    }
}

static void
gtk_progress_create_pixmap (GtkProgress *progress)
{
  GtkWidget *widget;

  g_return_if_fail (progress != NULL);
  g_return_if_fail (GTK_IS_PROGRESS (progress));

  if (GTK_WIDGET_REALIZED (progress))
    {
      widget = GTK_WIDGET (progress);

      if (progress->offscreen_pixmap)
	gdk_pixmap_unref (progress->offscreen_pixmap);

      progress->offscreen_pixmap = gdk_pixmap_new (widget->window,
						   widget->allocation.width,
						   widget->allocation.height,
						   -1);
      GTK_PROGRESS_CLASS (GTK_OBJECT (progress)->klass)->paint (progress);
    }
}

static void
gtk_progress_value_changed (GtkAdjustment *adjustment,
			    GtkProgress   *progress)
{
  GTK_PROGRESS_CLASS (GTK_OBJECT (progress)->klass)->update (progress);
}

static gchar *
gtk_progress_build_string (GtkProgress *progress,
			   gfloat       value,
			   gfloat       percentage)
{
  gchar buf[256] = { 0 };
  gchar tmp[256] = { 0 };
  gchar *src;
  gchar *dest;
  gchar fmt[10];

  src = progress->format;
  dest = buf;
 
  while (src && *src)
    {
      if (*src != '%')
	{
	  *dest = *src;
	  dest++;
	}
      else
	{
	  gchar c;
	  gint digits;

	  c = *(src + sizeof(gchar));
	  digits = 0;

	  if (c >= '0' && c <= '2')
	    {
	      digits = (gint) (c - '0');
	      src++;
	      c = *(src + sizeof(gchar));
	    }

	  switch (c)
	    {
	    case '%':
	      *dest = '%';
	      src++;
	      dest++;
	      break;
	    case 'p':
	    case 'P':
	      if (digits)
		{
		  sprintf (fmt, "%%.%df", digits);
		  sprintf (tmp, fmt, 100 * percentage);
		}
	      else
		sprintf (tmp, "%.0f", 100 * percentage);
	      strcat (buf, tmp);
	      dest = &(buf[strlen (buf)]);
	      src++;
	      break;
	    case 'v':
	    case 'V':
	      if (digits)
		{
		  sprintf (fmt, "%%.%df", digits);
		  sprintf (tmp, fmt, value);
		}
	      else
		sprintf (tmp, "%.0f", value);
	      strcat (buf, tmp);
	      dest = &(buf[strlen (buf)]);
	      src++;
	      break;
	    case 'l':
	    case 'L':
	      if (digits)
		{
		  sprintf (fmt, "%%.%df", digits);
		  sprintf (tmp, fmt, progress->adjustment->lower);
		}
	      else
		sprintf (tmp, "%.0f", progress->adjustment->lower);
	      strcat (buf, tmp);
	      dest = &(buf[strlen (buf)]);
	      src++;
	      break;
	    case 'u':
	    case 'U':
	      if (digits)
		{
		  sprintf (fmt, "%%.%df", digits);
		  sprintf (tmp, fmt, progress->adjustment->upper);
		}
	      else
		sprintf (tmp, "%.0f", progress->adjustment->upper);
	      strcat (buf, tmp);
	      dest = &(buf[strlen (buf)]);
	      src++;
	      break;
	    default:
	      break;
	    }
	}
      src++;
    }

  return g_strdup (buf);
}

/***************************************************************/

void
gtk_progress_set_adjustment (GtkProgress   *progress,
			     GtkAdjustment *adjustment)
{
  g_return_if_fail (progress != NULL);
  g_return_if_fail (GTK_IS_PROGRESS (progress));
  if (adjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
  else
    adjustment = (GtkAdjustment*) gtk_adjustment_new (0, 0, 100, 0, 0, 0);

  if (progress->adjustment != adjustment)
    {
      if (progress->adjustment)
        {
          gtk_signal_disconnect_by_data (GTK_OBJECT (progress->adjustment),
                                         (gpointer) progress);
          gtk_object_unref (GTK_OBJECT (progress->adjustment));
        }
      progress->adjustment = adjustment;
      if (adjustment)
        {
          gtk_object_ref (GTK_OBJECT (adjustment));
	  gtk_object_sink (GTK_OBJECT (adjustment));
          gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
			      (GtkSignalFunc) gtk_progress_value_changed,
			      (gpointer) progress);
        }
    }
}

void
gtk_progress_configure (GtkProgress *progress,
			gfloat value,
			gfloat min,
			gfloat max)
{
  GtkAdjustment *adj;
  gboolean changed = FALSE;

  g_return_if_fail (progress != NULL);
  g_return_if_fail (GTK_IS_PROGRESS (progress));
  g_return_if_fail (min <= max);
  g_return_if_fail (value >= min && value <= max);

  adj = progress->adjustment;

  if (fabs (adj->lower - min) > EPSILON || fabs (adj->upper - max) > EPSILON)
    changed = TRUE;

  adj->value = value;
  adj->lower = min;
  adj->upper = max;

  gtk_signal_emit_by_name (GTK_OBJECT (adj), "value_changed");
  if (changed)
    gtk_signal_emit_by_name (GTK_OBJECT (progress->adjustment), "changed");
}

void
gtk_progress_set_percentage (GtkProgress *progress,
			     gfloat       percentage)
{
  g_return_if_fail (progress != NULL);
  g_return_if_fail (GTK_IS_PROGRESS (progress));
  g_return_if_fail (percentage >= 0 && percentage <= 1.0);

  gtk_progress_set_value (progress, progress->adjustment->lower + percentage * 
		 (progress->adjustment->upper - progress->adjustment->lower));
}

gfloat
gtk_progress_get_current_percentage (GtkProgress *progress)
{
  g_return_val_if_fail (progress != NULL, 0);
  g_return_val_if_fail (GTK_IS_PROGRESS (progress), 0);

  return (progress->adjustment->value - progress->adjustment->lower) /
    (progress->adjustment->upper - progress->adjustment->lower);
}

gfloat
gtk_progress_get_percentage_from_value (GtkProgress *progress,
					gfloat       value)
{
  g_return_val_if_fail (progress != NULL, 0);
  g_return_val_if_fail (GTK_IS_PROGRESS (progress), 0);

  if (value >= progress->adjustment->lower &&
      value <= progress->adjustment->upper)
    return (value - progress->adjustment->lower) /
      (progress->adjustment->upper - progress->adjustment->lower);
  else
    return 0.0;
}

void
gtk_progress_set_value (GtkProgress *progress,
			gfloat       value)
{
  g_return_if_fail (progress != NULL);
  g_return_if_fail (GTK_IS_PROGRESS (progress));

  if (fabs (progress->adjustment->value - value) > EPSILON)
    gtk_adjustment_set_value (progress->adjustment, value);
}

gfloat
gtk_progress_get_value (GtkProgress *progress)
{
  g_return_val_if_fail (progress != NULL, 0);
  g_return_val_if_fail (GTK_IS_PROGRESS (progress), 0);

  return progress->adjustment->value;
}

void
gtk_progress_set_show_text (GtkProgress *progress,
			    gint        show_text)
{
  g_return_if_fail (progress != NULL);
  g_return_if_fail (GTK_IS_PROGRESS (progress));

  if (progress->show_text != show_text)
    {
      progress->show_text = show_text;

      if (GTK_WIDGET_DRAWABLE (GTK_WIDGET (progress)))
	gtk_widget_queue_resize (GTK_WIDGET (progress));
    }
}

void
gtk_progress_set_text_alignment (GtkProgress *progress,
				 gfloat       x_align,
				 gfloat       y_align)
{
  g_return_if_fail (progress != NULL);
  g_return_if_fail (GTK_IS_PROGRESS (progress));
  g_return_if_fail (x_align >= 0.0 && x_align <= 1.0);
  g_return_if_fail (y_align >= 0.0 && y_align <= 1.0);

  if (progress->x_align != x_align || progress->y_align != y_align)
    {
      progress->x_align = x_align;
      progress->y_align = y_align;

      if (GTK_WIDGET_DRAWABLE (GTK_WIDGET (progress)))
	gtk_widget_queue_resize (GTK_WIDGET (progress));
    }
}

void
gtk_progress_set_format_string (GtkProgress *progress,
				const gchar *format)
{
  g_return_if_fail (progress != NULL);
  g_return_if_fail (GTK_IS_PROGRESS (progress));

  if (format)
    {
      if (progress->format)
	g_free (progress->format);
      progress->format = g_strdup (format);

      if (GTK_WIDGET_DRAWABLE (GTK_WIDGET (progress)))
	gtk_widget_queue_resize (GTK_WIDGET (progress));
    }
}

gchar *
gtk_progress_get_current_text (GtkProgress *progress)
{
  g_return_val_if_fail (progress != NULL, 0);
  g_return_val_if_fail (GTK_IS_PROGRESS (progress), 0);

  return gtk_progress_build_string (progress, progress->adjustment->value,
		    gtk_progress_get_current_percentage (progress));
}

gchar *
gtk_progress_get_text_from_value (GtkProgress *progress,
				  gfloat       value)
{
  g_return_val_if_fail (progress != NULL, 0);
  g_return_val_if_fail (GTK_IS_PROGRESS (progress), 0);

  return gtk_progress_build_string (progress, value,
		    gtk_progress_get_percentage_from_value (progress, value));
}

void
gtk_progress_set_activity_mode (GtkProgress *progress,
				guint        activity_mode)
{
  g_return_if_fail (progress != NULL);
  g_return_if_fail (GTK_IS_PROGRESS (progress));

  if (progress->activity_mode != (activity_mode != 0))
    {
      progress->activity_mode = (activity_mode != 0);

      if (progress->activity_mode)
	GTK_PROGRESS_CLASS 
	  (GTK_OBJECT (progress)->klass)->act_mode_enter (progress);

      if (GTK_WIDGET_DRAWABLE (GTK_WIDGET (progress)))
	gtk_widget_queue_resize (GTK_WIDGET (progress));
    }
}
