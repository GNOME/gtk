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

#include "gtkhseparator.h"


static void gtk_hseparator_class_init (GtkHSeparatorClass *klass);
static void gtk_hseparator_init       (GtkHSeparator      *hseparator);
static gint gtk_hseparator_expose     (GtkWidget          *widget,
				       GdkEventExpose     *event);


GtkType
gtk_hseparator_get_type (void)
{
  static GtkType hseparator_type = 0;

  if (!hseparator_type)
    {
      static const GtkTypeInfo hseparator_info =
      {
	"GtkHSeparator",
	sizeof (GtkHSeparator),
	sizeof (GtkHSeparatorClass),
	(GtkClassInitFunc) gtk_hseparator_class_init,
	(GtkObjectInitFunc) gtk_hseparator_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      hseparator_type = gtk_type_unique (GTK_TYPE_SEPARATOR, &hseparator_info);
    }

  return hseparator_type;
}

static void
gtk_hseparator_class_init (GtkHSeparatorClass *class)
{
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) class;

  widget_class->expose_event = gtk_hseparator_expose;
}

static void
gtk_hseparator_init (GtkHSeparator *hseparator)
{
  GTK_WIDGET (hseparator)->requisition.width = 1;
  GTK_WIDGET (hseparator)->requisition.height = GTK_WIDGET (hseparator)->style->klass->ythickness;
}

GtkWidget*
gtk_hseparator_new (void)
{
  return GTK_WIDGET (gtk_type_new (GTK_TYPE_HSEPARATOR));
}


static gint
gtk_hseparator_expose (GtkWidget      *widget,
		       GdkEventExpose *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_HSEPARATOR (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    gtk_paint_hline (widget->style, widget->window, GTK_STATE_NORMAL,
		     &event->area, widget, "hseparator",
		     widget->allocation.x,
		     widget->allocation.x + widget->allocation.width,
		     widget->allocation.y + (widget->allocation.height -
					     widget->style->klass->ythickness) / 2);

  return FALSE;
}
