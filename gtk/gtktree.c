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
#include "gtktree.h"


static void gtk_tree_class_init (GtkTreeClass *klass);
static void gtk_tree_init       (GtkTree      *tree);


guint
gtk_tree_get_type ()
{
  static guint tree_type = 0;

  if (!tree_type)
    {
      GtkTypeInfo tree_info =
      {
	"GtkTree",
	sizeof (GtkTree),
	sizeof (GtkTreeClass),
	(GtkClassInitFunc) gtk_tree_class_init,
	(GtkObjectInitFunc) gtk_tree_init,
	(GtkArgFunc) NULL,
      };

      tree_type = gtk_type_unique (gtk_container_get_type (), &tree_info);
    }

  return tree_type;
}

static void
gtk_tree_class_init (GtkTreeClass *class)
{
}

static void
gtk_tree_init (GtkTree *tree)
{
}

GtkWidget*
gtk_tree_new ()
{
  return GTK_WIDGET (gtk_type_new (gtk_tree_get_type ()));
}

void
gtk_tree_append (GtkTree   *tree,
		 GtkWidget *child)
{
}

void
gtk_tree_prepend (GtkTree   *tree,
		  GtkWidget *child)
{
}

void
gtk_tree_insert (GtkTree   *tree,
		 GtkWidget *child,
		 gint       position)
{
}
