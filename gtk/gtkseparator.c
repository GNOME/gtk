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
#include "gtkseparator.h"


static void gtk_separator_class_init (GtkSeparatorClass *klass);
static void gtk_separator_init       (GtkSeparator      *separator);


guint
gtk_separator_get_type ()
{
  static guint separator_type = 0;

  if (!separator_type)
    {
      GtkTypeInfo separator_info =
      {
	"GtkSeparator",
	sizeof (GtkSeparator),
	sizeof (GtkSeparatorClass),
	(GtkClassInitFunc) gtk_separator_class_init,
	(GtkObjectInitFunc) gtk_separator_init,
	(GtkArgSetFunc) NULL,
        (GtkArgGetFunc) NULL,
      };

      separator_type = gtk_type_unique (gtk_widget_get_type (), &separator_info);
    }

  return separator_type;
}

static void
gtk_separator_class_init (GtkSeparatorClass *class)
{
}

static void
gtk_separator_init (GtkSeparator *separator)
{
  GTK_WIDGET_SET_FLAGS (separator, GTK_NO_WINDOW | GTK_BASIC);
}
