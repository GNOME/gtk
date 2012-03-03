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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtkseparatormenuitem.h"

#include "gtkstylecontext.h"

/**
 * SECTION:gtkseparatormenuitem
 * @Short_description: A separator used in menus
 * @Title: GtkSeparatorMenuItem
 *
 * The #GtkSeparatorMenuItem is a separator used to group
 * items within a menu. It displays a horizontal line with a shadow to
 * make it appear sunken into the interface.
 */

G_DEFINE_TYPE (GtkSeparatorMenuItem, gtk_separator_menu_item, GTK_TYPE_MENU_ITEM)

static void
gtk_separator_menu_item_class_init (GtkSeparatorMenuItemClass *class)
{
  GTK_CONTAINER_CLASS (class)->child_type = NULL;

  gtk_widget_class_set_accessible_role (GTK_WIDGET_CLASS (class), ATK_ROLE_SEPARATOR);
}

static void 
gtk_separator_menu_item_init (GtkSeparatorMenuItem *item)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (GTK_WIDGET (item));
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_SEPARATOR);
}

/**
 * gtk_separator_menu_item_new:
 *
 * Creates a new #GtkSeparatorMenuItem.
 *
 * Returns: a new #GtkSeparatorMenuItem.
 */
GtkWidget *
gtk_separator_menu_item_new (void)
{
  return g_object_new (GTK_TYPE_SEPARATOR_MENU_ITEM, NULL);
}
