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
#include "gtkhseparator.h"


static void gtk_hseparator_class_init (GtkHSeparatorClass *klass);
static void gtk_hseparator_init       (GtkHSeparator      *hseparator);
static gint gtk_hseparator_expose     (GtkWidget          *widget,
				       GdkEventExpose     *event);


guint
gtk_hseparator_get_type ()
{
  static guint hseparator_type = 0;

  if (!hseparator_type)
    {
      GtkTypeInfo hseparator_info =
      {
	"GtkHSeparator",
	sizeof (GtkHSeparator),
	sizeof (GtkHSeparatorClass),
	(GtkClassInitFunc) gtk_hseparator_class_init,
	(GtkObjectInitFunc) gtk_hseparator_init,
	(GtkArgFunc) NULL,
      };

      hseparator_type = gtk_type_unique (gtk_separator_get_type (), &hseparator_info);
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
gtk_hseparator_new ()
{
  return GTK_WIDGET (gtk_type_new (gtk_hseparator_get_type ()));
}


static gint
gtk_hseparator_expose (GtkWidget      *widget,
		       GdkEventExpose *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_HSEPARATOR (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    gtk_draw_hline (widget->style, widget->window, GTK_STATE_NORMAL,
		    widget->allocation.x,
		    widget->allocation.x + widget->allocation.width,
		    widget->allocation.y + (widget->allocation.height -
					    widget->style->klass->ythickness) / 2);

  return FALSE;
}
