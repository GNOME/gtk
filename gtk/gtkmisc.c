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
#include "gtkcontainer.h"
#include "gtkmisc.h"


static void gtk_misc_class_init (GtkMiscClass *klass);
static void gtk_misc_init       (GtkMisc      *misc);
static void gtk_misc_realize    (GtkWidget    *widget);


guint
gtk_misc_get_type ()
{
  static guint misc_type = 0;

  if (!misc_type)
    {
      GtkTypeInfo misc_info =
      {
	"GtkMisc",
	sizeof (GtkMisc),
	sizeof (GtkMiscClass),
	(GtkClassInitFunc) gtk_misc_class_init,
	(GtkObjectInitFunc) gtk_misc_init,
	(GtkArgSetFunc) NULL,
        (GtkArgGetFunc) NULL,
      };

      misc_type = gtk_type_unique (gtk_widget_get_type (), &misc_info);
    }

  return misc_type;
}

static void
gtk_misc_class_init (GtkMiscClass *class)
{
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) class;

  widget_class->realize = gtk_misc_realize;
}

static void
gtk_misc_init (GtkMisc *misc)
{
  GTK_WIDGET_SET_FLAGS (misc, GTK_BASIC);

  misc->xalign = 0.5;
  misc->yalign = 0.5;
  misc->xpad = 0;
  misc->ypad = 0;
}

void
gtk_misc_set_alignment (GtkMisc *misc,
			gfloat   xalign,
			gfloat   yalign)
{
  g_return_if_fail (misc != NULL);
  g_return_if_fail (GTK_IS_MISC (misc));

  if (xalign < 0.0)
    xalign = 0.0;
  else if (xalign > 1.0)
    xalign = 1.0;

  if (yalign < 0.0)
    yalign = 0.0;
  else if (yalign > 1.0)
    yalign = 1.0;

  if ((xalign != misc->xalign) || (yalign != misc->yalign))
    {
      misc->xalign = xalign;
      misc->yalign = yalign;

      /* clear the area that was allocated before the change
      */
      if (GTK_WIDGET_VISIBLE (misc))
        {
          GtkWidget *widget;

          widget = GTK_WIDGET (misc);
          gdk_window_clear_area (widget->window,
                                 widget->allocation.x,
                                 widget->allocation.y,
                                 widget->allocation.width,
                                 widget->allocation.height);
        }

      gtk_widget_queue_draw (GTK_WIDGET (misc));
    }
}

void
gtk_misc_set_padding (GtkMisc *misc,
		      gint     xpad,
		      gint     ypad)
{
  GtkRequisition *requisition;

  g_return_if_fail (misc != NULL);
  g_return_if_fail (GTK_IS_MISC (misc));

  if (xpad < 0)
    xpad = 0;
  if (ypad < 0)
    ypad = 0;

  if ((xpad != misc->xpad) || (ypad != misc->ypad))
    {
      requisition = &(GTK_WIDGET (misc)->requisition);
      requisition->width -= misc->xpad * 2;
      requisition->height -= misc->ypad * 2;

      misc->xpad = xpad;
      misc->ypad = ypad;

      requisition->width += misc->xpad * 2;
      requisition->height += misc->ypad * 2;

      if (GTK_WIDGET (misc)->parent && GTK_WIDGET_VISIBLE (misc))
	gtk_widget_queue_resize (GTK_WIDGET (misc));
    }
}

static void
gtk_misc_realize (GtkWidget *widget)
{
  GtkMisc *misc;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MISC (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  misc = GTK_MISC (widget);

  if (GTK_WIDGET_NO_WINDOW (widget))
    {
      widget->window = gtk_widget_get_parent_window (widget);
      gdk_window_ref (widget->window);
      widget->style = gtk_style_attach (widget->style, widget->window);
    }
  else
    {
      attributes.window_type = GDK_WINDOW_CHILD;
      attributes.x = widget->allocation.x;
      attributes.y = widget->allocation.y;
      attributes.width = widget->allocation.width;
      attributes.height = widget->allocation.height;
      attributes.wclass = GDK_INPUT_OUTPUT;
      attributes.visual = gtk_widget_get_visual (widget);
      attributes.colormap = gtk_widget_get_colormap (widget);
      attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;
      attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

      widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
      gdk_window_set_user_data (widget->window, widget);

      widget->style = gtk_style_attach (widget->style, widget->window);
      gdk_window_set_back_pixmap (widget->window, NULL, TRUE);
    }
}
