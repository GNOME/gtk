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

#include "gtksignal.h"
#include "gtkinvisible.h"

static void gtk_invisible_class_init               (GtkInvisibleClass *klass);
static void gtk_invisible_init                     (GtkInvisible      *invisible);
static void gtk_invisible_realize                  (GtkWidget        *widget);
static void gtk_invisible_show                     (GtkWidget        *widget);
static void gtk_invisible_size_allocate            (GtkWidget        *widget,
						    GtkAllocation    *allocation);

GtkType
gtk_invisible_get_type (void)
{
  static GtkType invisible_type = 0;

  if (!invisible_type)
    {
      static const GtkTypeInfo invisible_info =
      {
	"GtkInvisible",
	sizeof (GtkInvisible),
	sizeof (GtkInvisibleClass),
	(GtkClassInitFunc) gtk_invisible_class_init,
	(GtkObjectInitFunc) gtk_invisible_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      invisible_type = gtk_type_unique (GTK_TYPE_WIDGET, &invisible_info);
    }

  return invisible_type;
}

static void
gtk_invisible_class_init (GtkInvisibleClass *class)
{
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) class;

  widget_class->realize = gtk_invisible_realize;
  widget_class->show = gtk_invisible_show;
  widget_class->size_allocate = gtk_invisible_size_allocate;
}

static void
gtk_invisible_init (GtkInvisible *invisible)
{
  GTK_WIDGET_UNSET_FLAGS (invisible, GTK_NO_WINDOW);

  gtk_widget_ref (GTK_WIDGET (invisible));
  gtk_object_sink (GTK_OBJECT (invisible));
}

GtkWidget*
gtk_invisible_new (void)
{
  return GTK_WIDGET (gtk_type_new (GTK_TYPE_INVISIBLE));
}

static void
gtk_invisible_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_INVISIBLE (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  attributes.x = -100;
  attributes.y = -100;
  attributes.width = 10;
  attributes.height = 10;
  attributes.window_type = GDK_WINDOW_TEMP;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.override_redirect = TRUE;
  attributes.event_mask = gtk_widget_get_events (widget);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_NOREDIR;

  widget->window = gdk_window_new (NULL, &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);
  
  widget->style = gtk_style_attach (widget->style, widget->window);
}

static void
gtk_invisible_show (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);

  GTK_WIDGET_SET_FLAGS (widget, GTK_VISIBLE);
  gtk_widget_map (widget);
}

static void
gtk_invisible_size_allocate (GtkWidget     *widget,
			    GtkAllocation *allocation)
{
  widget->allocation = *allocation;
}
