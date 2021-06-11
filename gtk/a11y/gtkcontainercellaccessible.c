/* GTK+ - accessibility implementations
 * Copyright 2001 Sun Microsystems Inc.
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

#include "config.h"

#include <gtk/gtk.h>
#include "gtkcontainercellaccessible.h"
#include "gtkcellaccessibleprivate.h"

struct _GtkContainerCellAccessiblePrivate
{
  GList *children;
  gint n_children;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkContainerCellAccessible, gtk_container_cell_accessible, GTK_TYPE_CELL_ACCESSIBLE)


static void
unset_child (gpointer child)
{
  atk_object_set_parent (ATK_OBJECT (child), NULL);
  g_object_unref (child);
}

static void
gtk_container_cell_accessible_finalize (GObject *obj)
{
  GtkContainerCellAccessible *container = GTK_CONTAINER_CELL_ACCESSIBLE (obj);

  g_list_free_full (container->priv->children, unset_child);

  G_OBJECT_CLASS (gtk_container_cell_accessible_parent_class)->finalize (obj);
}


static gint
gtk_container_cell_accessible_get_n_children (AtkObject *obj)
{
  GtkContainerCellAccessible *cell = GTK_CONTAINER_CELL_ACCESSIBLE (obj);

  return cell->priv->n_children;
}

static AtkObject *
gtk_container_cell_accessible_ref_child (AtkObject *obj,
                                         gint       child)
{
  GtkContainerCellAccessible *cell = GTK_CONTAINER_CELL_ACCESSIBLE (obj);
  GList *l;

  l = g_list_nth (cell->priv->children, child);
  if (l == NULL)
    return NULL;

  return g_object_ref (ATK_OBJECT (l->data));
}

static void
gtk_container_cell_accessible_update_cache (GtkCellAccessible *cell,
                                            gboolean           emit_signal)
{
  GtkContainerCellAccessible *container = GTK_CONTAINER_CELL_ACCESSIBLE (cell);
  GList *l;

  for (l = container->priv->children; l; l = l->next)
    _gtk_cell_accessible_update_cache (l->data, emit_signal);
}

static void
gtk_container_cell_widget_set (GtkAccessible *accessible)
{
  GtkContainerCellAccessible *container = GTK_CONTAINER_CELL_ACCESSIBLE (accessible);
  GList *l;

  for (l = container->priv->children; l; l = l->next)
    gtk_accessible_set_widget (l->data, gtk_accessible_get_widget (accessible));

  GTK_ACCESSIBLE_CLASS (gtk_container_cell_accessible_parent_class)->widget_set (accessible);
}

static void
gtk_container_cell_widget_unset (GtkAccessible *accessible)
{
  GtkContainerCellAccessible *container = GTK_CONTAINER_CELL_ACCESSIBLE (accessible);
  GList *l;

  for (l = container->priv->children; l; l = l->next)
    gtk_accessible_set_widget (l->data, NULL);

  GTK_ACCESSIBLE_CLASS (gtk_container_cell_accessible_parent_class)->widget_unset (accessible);
}

static void
gtk_container_cell_accessible_class_init (GtkContainerCellAccessibleClass *klass)
{
  GtkCellAccessibleClass *cell_class = GTK_CELL_ACCESSIBLE_CLASS (klass);
  GtkAccessibleClass *accessible_class = GTK_ACCESSIBLE_CLASS (klass);
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
  GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

  g_object_class->finalize = gtk_container_cell_accessible_finalize;

  class->get_n_children = gtk_container_cell_accessible_get_n_children;
  class->ref_child = gtk_container_cell_accessible_ref_child;

  accessible_class->widget_set = gtk_container_cell_widget_set;
  accessible_class->widget_unset = gtk_container_cell_widget_unset;

  cell_class->update_cache = gtk_container_cell_accessible_update_cache;
}

static void
gtk_container_cell_accessible_init (GtkContainerCellAccessible *cell)
{
  cell->priv = gtk_container_cell_accessible_get_instance_private (cell);
}

GtkContainerCellAccessible *
gtk_container_cell_accessible_new (void)
{
  GObject *object;

  object = g_object_new (GTK_TYPE_CONTAINER_CELL_ACCESSIBLE, NULL);

  ATK_OBJECT (object)->role = ATK_ROLE_TABLE_CELL;

  return GTK_CONTAINER_CELL_ACCESSIBLE (object);
}

void
gtk_container_cell_accessible_add_child (GtkContainerCellAccessible *container,
                                          GtkCellAccessible          *child)
{
  g_return_if_fail (GTK_IS_CONTAINER_CELL_ACCESSIBLE (container));
  g_return_if_fail (GTK_IS_CELL_ACCESSIBLE (child));

  g_object_ref (child);

  container->priv->n_children++;
  container->priv->children = g_list_append (container->priv->children, child);
  atk_object_set_parent (ATK_OBJECT (child), ATK_OBJECT (container));
}

void
gtk_container_cell_accessible_remove_child (GtkContainerCellAccessible *container,
                                             GtkCellAccessible          *child)
{
  g_return_if_fail (GTK_IS_CONTAINER_CELL_ACCESSIBLE (container));
  g_return_if_fail (GTK_IS_CELL_ACCESSIBLE (child));
  g_return_if_fail (container->priv->n_children > 0);

  atk_object_set_parent (ATK_OBJECT (child), NULL);
  container->priv->children = g_list_remove (container->priv->children, child);
  container->priv->n_children--;

  g_object_unref (child);
}

/**
 * gtk_container_cell_accessible_get_children:
 * @container: the container
 *
 * Get a list of children.
 *
 * Returns: (transfer none) (element-type Gtk.CellAccessible)
 */
GList *
gtk_container_cell_accessible_get_children (GtkContainerCellAccessible *container)
{
  g_return_val_if_fail (GTK_IS_CONTAINER_CELL_ACCESSIBLE (container), NULL);

  return container->priv->children;
}
