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

#include <config.h>
#include "gtkhseparator.h"
#include "gtkalias.h"


static void gtk_hseparator_class_init (GtkHSeparatorClass *klass);
static void gtk_hseparator_init       (GtkHSeparator      *hseparator);
static gint gtk_hseparator_expose     (GtkWidget          *widget,
				       GdkEventExpose     *event);


GType
gtk_hseparator_get_type (void)
{
  static GType hseparator_type = 0;

  if (!hseparator_type)
    {
      static const GTypeInfo hseparator_info =
      {
	sizeof (GtkHSeparatorClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_hseparator_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_init */
	sizeof (GtkHSeparator),
	0,		/* n_preallocs */
	(GInstanceInitFunc) gtk_hseparator_init,
      };

      hseparator_type =
	g_type_register_static (GTK_TYPE_SEPARATOR, "GtkHSeparator",
				&hseparator_info, 0);
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
  GTK_WIDGET (hseparator)->requisition.height = GTK_WIDGET (hseparator)->style->ythickness;
}

GtkWidget*
gtk_hseparator_new (void)
{
  return g_object_new (GTK_TYPE_HSEPARATOR, NULL);
}


static gint
gtk_hseparator_expose (GtkWidget      *widget,
		       GdkEventExpose *event)
{
  if (GTK_WIDGET_DRAWABLE (widget))
    gtk_paint_hline (widget->style, widget->window, GTK_WIDGET_STATE (widget),
		     &event->area, widget, "hseparator",
		     widget->allocation.x,
		     widget->allocation.x + widget->allocation.width - 1,
		     widget->allocation.y + (widget->allocation.height -
					     widget->style->ythickness) / 2);

  return FALSE;
}

#define __GTK_HSEPARATOR_C__
#include "gtkaliasdef.c"
