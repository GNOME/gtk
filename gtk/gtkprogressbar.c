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
#include "gtkprogressbar.h"


#define MIN_WIDTH   200
#define MIN_HEIGHT  20


static void gtk_progress_bar_class_init    (GtkProgressBarClass *klass);
static void gtk_progress_bar_init          (GtkProgressBar      *pbar);
static void gtk_progress_bar_realize       (GtkWidget           *widget);
static void gtk_progress_bar_size_allocate (GtkWidget           *widget,
					    GtkAllocation       *allocation);
static gint gtk_progress_bar_expose        (GtkWidget           *widget,
					    GdkEventExpose      *event);
static void gtk_progress_bar_make_pixmap   (GtkProgressBar      *pbar);
static void gtk_progress_bar_paint         (GtkProgressBar      *pbar);


guint
gtk_progress_bar_get_type ()
{
  static guint progress_bar_type = 0;

  if (!progress_bar_type)
    {
      GtkTypeInfo progress_bar_info =
      {
	"GtkProgressBar",
	sizeof (GtkProgressBar),
	sizeof (GtkProgressBarClass),
	(GtkClassInitFunc) gtk_progress_bar_class_init,
	(GtkObjectInitFunc) gtk_progress_bar_init,
	(GtkArgSetFunc) NULL,
        (GtkArgGetFunc) NULL,
      };

      progress_bar_type = gtk_type_unique (gtk_widget_get_type (), &progress_bar_info);
    }

  return progress_bar_type;
}

static void
gtk_progress_bar_class_init (GtkProgressBarClass *class)
{
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) class;

  widget_class->realize = gtk_progress_bar_realize;
  widget_class->size_allocate = gtk_progress_bar_size_allocate;
  widget_class->expose_event = gtk_progress_bar_expose;
}

static void
gtk_progress_bar_init (GtkProgressBar *pbar)
{
  GTK_WIDGET_SET_FLAGS (pbar, GTK_BASIC);

  GTK_WIDGET (pbar)->requisition.width = MIN_WIDTH;
  GTK_WIDGET (pbar)->requisition.height = MIN_HEIGHT;
  pbar->offscreen_pixmap = NULL;
  pbar->percentage = 0;
}


GtkWidget*
gtk_progress_bar_new ()
{
  return GTK_WIDGET (gtk_type_new (gtk_progress_bar_get_type ()));
}

void
gtk_progress_bar_update (GtkProgressBar *pbar,
			 gfloat          percentage)
{
  g_return_if_fail (pbar != NULL);
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  if (percentage < 0.0)
    percentage = 0.0;
  else if (percentage > 1.0)
    percentage = 1.0;

  if (pbar->percentage != percentage)
    {
      pbar->percentage = percentage;
      gtk_progress_bar_paint (pbar);
      gtk_widget_queue_draw (GTK_WIDGET (pbar));
    }
}

static void
gtk_progress_bar_realize (GtkWidget *widget)
{
  GtkProgressBar *pbar;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_PROGRESS_BAR (widget));

  pbar = GTK_PROGRESS_BAR (widget);
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

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, pbar);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_ACTIVE);

  gtk_progress_bar_make_pixmap (pbar);
}

static void
gtk_progress_bar_size_allocate (GtkWidget     *widget,
				GtkAllocation *allocation)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_PROGRESS_BAR (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);

      gtk_progress_bar_make_pixmap (GTK_PROGRESS_BAR (widget));
    }
}

static gint
gtk_progress_bar_expose (GtkWidget      *widget,
			 GdkEventExpose *event)
{
  GtkProgressBar *pbar;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      pbar = GTK_PROGRESS_BAR (widget);

      gdk_draw_pixmap (widget->window,
		       widget->style->black_gc,
		       pbar->offscreen_pixmap,
		       0, 0, 0, 0,
		       widget->allocation.width,
		       widget->allocation.height);
    }

  return FALSE;
}

static void
gtk_progress_bar_make_pixmap (GtkProgressBar *pbar)
{
  GtkWidget *widget;

  g_return_if_fail (pbar != NULL);
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  if (GTK_WIDGET_REALIZED (pbar))
    {
      widget = GTK_WIDGET (pbar);

      if (pbar->offscreen_pixmap)
	gdk_pixmap_unref (pbar->offscreen_pixmap);

      pbar->offscreen_pixmap = gdk_pixmap_new (widget->window,
					       widget->allocation.width,
					       widget->allocation.height,
					       -1);

      gtk_progress_bar_paint (pbar);
    }
}

static void
gtk_progress_bar_paint (GtkProgressBar *pbar)
{
  GtkWidget *widget;
  int amount;

  g_return_if_fail (pbar != NULL);
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  if (pbar->offscreen_pixmap)
    {
      widget = GTK_WIDGET (pbar);

      gtk_draw_shadow (widget->style,
		       pbar->offscreen_pixmap,
		       GTK_STATE_NORMAL, GTK_SHADOW_IN, 0, 0,
		       widget->allocation.width,
		       widget->allocation.height);

      gdk_draw_rectangle (pbar->offscreen_pixmap,
			  widget->style->bg_gc[GTK_STATE_ACTIVE], TRUE,
			  widget->style->klass->xthickness,
			  widget->style->klass->ythickness,
			  widget->allocation.width - widget->style->klass->xthickness * 2,
			  widget->allocation.height - widget->style->klass->ythickness * 2);


      amount = pbar->percentage * (widget->allocation.width - widget->style->klass->xthickness * 2);
      if (amount > 0)
	{
	  gdk_draw_rectangle (pbar->offscreen_pixmap,
			      widget->style->bg_gc[GTK_STATE_PRELIGHT], TRUE,
			      widget->style->klass->xthickness,
			      widget->style->klass->ythickness,
			      amount,
			      widget->allocation.height - widget->style->klass->ythickness * 2);

	  gtk_draw_shadow (widget->style,
			   pbar->offscreen_pixmap,
			   GTK_STATE_PRELIGHT, GTK_SHADOW_OUT,
			   widget->style->klass->xthickness,
			   widget->style->klass->ythickness,
			   amount,
			   widget->allocation.height - widget->style->klass->ythickness * 2);
	}
    }
}
