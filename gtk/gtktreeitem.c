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
#include "gtklabel.h"
#include "gtktreeitem.h"


static void gtk_tree_item_class_init (GtkTreeItemClass *klass);
static void gtk_tree_item_init       (GtkTreeItem      *tree_item);


guint
gtk_tree_item_get_type ()
{
  static guint tree_item_type = 0;

  if (!tree_item_type)
    {
      GtkTypeInfo tree_item_info =
      {
	"GtkTreeItem",
	sizeof (GtkTreeItem),
	sizeof (GtkTreeItemClass),
	(GtkClassInitFunc) gtk_tree_item_class_init,
	(GtkObjectInitFunc) gtk_tree_item_init,
	(GtkArgFunc) NULL,
      };

      tree_item_type = gtk_type_unique (gtk_item_get_type (), &tree_item_info);
    }

  return tree_item_type;
}

static void
gtk_tree_item_class_init (GtkTreeItemClass *class)
{
}

static void
gtk_tree_item_init (GtkTreeItem *tree_item)
{
}


GtkWidget*
gtk_tree_item_new ()
{
  return GTK_WIDGET (gtk_type_new (gtk_tree_item_get_type ()));
}

GtkWidget*
gtk_tree_item_new_with_label (gchar *label)
{
  GtkWidget *tree_item;
  GtkWidget *label_widget;

  tree_item = gtk_tree_item_new ();
  label_widget = gtk_label_new (label);
  gtk_misc_set_alignment (GTK_MISC (label_widget), 0.0, 0.5);

  gtk_container_add (GTK_CONTAINER (tree_item), label_widget);
  gtk_widget_show (label_widget);

  return tree_item;
}

void
gtk_tree_item_set_subtree (GtkTreeItem *tree_item,
			   GtkWidget   *subtree)
{
  g_return_if_fail (tree_item != NULL);
  g_return_if_fail (GTK_IS_TREE_ITEM (tree_item));
}

void
gtk_tree_item_select (GtkTreeItem *tree_item)
{
}

void
gtk_tree_item_deselect (GtkTreeItem *tree_item)
{
}

void
gtk_tree_item_expand (GtkTreeItem *tree_item)
{
}

void
gtk_tree_item_collapse (GtkTreeItem *tree_item)
{
}
