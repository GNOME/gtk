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

#include "gtkseparatormenuitem.h"

static void gtk_separator_menu_item_class_init (GtkSeparatorMenuItemClass *class);

GType
gtk_separator_menu_item_get_type (void)
{
  static GType separator_menu_item_type = 0;

  if (!separator_menu_item_type)
    {
      static const GTypeInfo separator_menu_item_info =
      {
        sizeof (GtkSeparatorMenuItemClass),
        NULL, /* base_init */
        NULL, /* base_finalize */
        (GClassInitFunc) gtk_separator_menu_item_class_init,
        NULL, /* class_finalize */
        NULL, /* class_data */
        sizeof (GtkSeparatorMenuItem),
        0,    /* n_preallocs */
        NULL, /* instance_init */
      };

      separator_menu_item_type =
	g_type_register_static (GTK_TYPE_MENU_ITEM, "GtkSeparatorMenuItem",
				&separator_menu_item_info, 0);
    }

  return separator_menu_item_type;
}

static void
gtk_separator_menu_item_class_init (GtkSeparatorMenuItemClass *class)
{
  GTK_CONTAINER_CLASS (class)->child_type = NULL;
}

GtkWidget *
gtk_separator_menu_item_new (void)
{
  return g_object_new (GTK_TYPE_SEPARATOR_MENU_ITEM, NULL);
}
