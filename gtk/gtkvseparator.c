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
#include "gtkvseparator.h"


static void gtk_vseparator_class_init (GtkVSeparatorClass *klass);
static void gtk_vseparator_init       (GtkVSeparator      *vseparator);
static gint gtk_vseparator_expose     (GtkWidget          *widget,
				       GdkEventExpose     *event);


guint
gtk_vseparator_get_type (void)
{
  static guint vseparator_type = 0;

  if (!vseparator_type)
    {
      GtkTypeInfo vseparator_info =
      {
	"GtkVSeparator",
	sizeof (GtkVSeparator),
	sizeof (GtkVSeparatorClass),
	(GtkClassInitFunc) gtk_vseparator_class_init,
	(GtkObjectInitFunc) gtk_vseparator_init,
	/* reversed_1 */ NULL,
        /* reversed_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      vseparator_type = gtk_type_unique (gtk_separator_get_type (), &vseparator_info);
    }

  return vseparator_type;
}

static void
gtk_vseparator_class_init (GtkVSeparatorClass *klass)
{
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) klass;

  widget_class->expose_event = gtk_vseparator_expose;
}

static void
gtk_vseparator_init (GtkVSeparator *vseparator)
{
  GTK_WIDGET (vseparator)->requisition.width = GTK_WIDGET (vseparator)->style->klass->xthickness;
  GTK_WIDGET (vseparator)->requisition.height = 1;
}

GtkWidget*
gtk_vseparator_new (void)
{
  return GTK_WIDGET (gtk_type_new (gtk_vseparator_get_type ()));
}


static gint
gtk_vseparator_expose (GtkWidget      *widget,
		       GdkEventExpose *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_VSEPARATOR (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    gtk_draw_vline (widget->style, widget->window, GTK_STATE_NORMAL,
		    widget->allocation.y,
		    widget->allocation.y + widget->allocation.height,
		    widget->allocation.x + (widget->allocation.width -
					    widget->style->klass->xthickness) / 2);

  return FALSE;
}
