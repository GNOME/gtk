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
#include "gtkscrollbar.h"

static void gtk_scrollbar_class_init (GtkScrollbarClass *klass);
static void gtk_scrollbar_init       (GtkScrollbar      *scrollbar);

guint
gtk_scrollbar_get_type (void)
{
  static guint scrollbar_type = 0;

  if (!scrollbar_type)
    {
      GtkTypeInfo scrollbar_info =
      {
	"GtkScrollbar",
	sizeof (GtkScrollbar),
	sizeof (GtkScrollbarClass),
	(GtkClassInitFunc) gtk_scrollbar_class_init,
	(GtkObjectInitFunc) gtk_scrollbar_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      scrollbar_type = gtk_type_unique (gtk_range_get_type (), &scrollbar_info);
    }

  return scrollbar_type;
}

static void
gtk_scrollbar_class_init (GtkScrollbarClass *class)
{
}

static void
gtk_scrollbar_init (GtkScrollbar *scrollbar)
{
}
