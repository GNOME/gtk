/*
 * Copyright © 2019 Matthias Clasen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtktreelistrowsorter.h"

#include "gtktreelistmodel.h"

#include "gtkintl.h"
#include "gtktypebuiltins.h"

/**
 * SECTION:gtktreelistrowsorter
 * @title: GtkTreeListRowSorter
 * @Short_description: Sort trees by levels
 * @See_also: #GtkTreeListModel
 *
 * #GtkTreeListRowSorter is a special-purpose sorter that will apply a given sorter
 * to the levels in a tree, while respecting the tree structure.
 */

struct _GtkTreeListRowSorter
{
  GtkSorter parent_instance;

  GtkSorter *sorter;
};

enum {
  PROP_0,
  PROP_SORTER,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (GtkTreeListRowSorter, gtk_tree_list_row_sorter, GTK_TYPE_SORTER)

static GtkOrdering
gtk_tree_list_row_sorter_compare (GtkSorter *sorter,
                                  gpointer   item1,
                                  gpointer   item2)
{
  GtkTreeListRowSorter *self = GTK_TREE_LIST_ROW_SORTER (sorter);
  GtkTreeListRow *r1, *r2;
  GtkTreeListRow *p1, *p2;
  guint d1, d2;
  GtkOrdering result = GTK_ORDERING_EQUAL;

  /* break ties here so we really are a total order */
  if (!GTK_IS_TREE_LIST_ROW (item1))
    return GTK_IS_TREE_LIST_ROW (item2) ? GTK_ORDERING_LARGER : (item1 < item2 ? GTK_ORDERING_SMALLER : GTK_ORDERING_LARGER);
  else if (!GTK_IS_TREE_LIST_ROW (item2))
    return GTK_ORDERING_SMALLER;

  r1 = GTK_TREE_LIST_ROW (item1);
  r2 = GTK_TREE_LIST_ROW (item2);

  g_object_ref (r1);
  g_object_ref (r2);

  d1 = gtk_tree_list_row_get_depth (r1);
  d2 = gtk_tree_list_row_get_depth (r2);

  /* First, get to the same depth */
  while (d1 > d2)
    {
      p1 = gtk_tree_list_row_get_parent (r1);
      g_object_unref (r1);
      r1 = p1;
      d1--;
      result = GTK_ORDERING_LARGER;
    }
  while (d2 > d1)
    {
      p2 = gtk_tree_list_row_get_parent (r2);
      g_object_unref (r2);
      r2 = p2;
      d2--;
      result = GTK_ORDERING_SMALLER;
    }

  /* Now walk up until we find a common parent */
  if (r1 != r2)
    {
      while (TRUE)
        {
          p1 = gtk_tree_list_row_get_parent (r1);
          p2 = gtk_tree_list_row_get_parent (r2);
          if (p1 == p2)
            {
              gpointer obj1 = gtk_tree_list_row_get_item (r1);
              gpointer obj2 = gtk_tree_list_row_get_item (r2);
          
              if (self->sorter == NULL)
                result = GTK_ORDERING_EQUAL;
              else
                result = gtk_sorter_compare (self->sorter, obj1, obj2);

              /* We must break ties here because if r1 ever gets a child,
               * it would need to go right inbetween r1 and r2. */
              if (result == GTK_ORDERING_EQUAL)
                {
                  if (gtk_tree_list_row_get_position (r1) < gtk_tree_list_row_get_position (r2))
                    result = GTK_ORDERING_SMALLER;
                  else
                    result = GTK_ORDERING_LARGER;
                }

              g_object_unref (obj1);
              g_object_unref (obj2);

              break;
            }
          else
            {
              g_object_unref (r1);
              r1 = p1;
              g_object_unref (r2);
              r2 = p2;
            }
        }
    }

  g_object_unref (r1);
  g_object_unref (r2);

  return result;
}

static GtkSorterOrder
gtk_tree_list_row_sorter_get_order (GtkSorter *sorter)
{
  /* Must be a total order, because we need an exact position where new items go */
  return GTK_SORTER_ORDER_TOTAL;
}

static void
propagate_changed (GtkSorter *sorter, GtkSorterChange change, gpointer data)
{
  gtk_sorter_changed (GTK_SORTER (data), change);
}

static void
gtk_tree_list_row_sorter_dispose (GObject *object)
{
  GtkTreeListRowSorter *self = GTK_TREE_LIST_ROW_SORTER (object);

  if (self->sorter)
    g_signal_handlers_disconnect_by_func (self->sorter, propagate_changed, self);
  g_clear_object (&self->sorter);

  G_OBJECT_CLASS (gtk_tree_list_row_sorter_parent_class)->dispose (object);
}

static void
gtk_tree_list_row_sorter_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GtkTreeListRowSorter *self = GTK_TREE_LIST_ROW_SORTER (object);

  switch (prop_id)
    {
    case PROP_SORTER:
      gtk_tree_list_row_sorter_set_sorter (self, GTK_SORTER (g_value_get_object (value)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_tree_list_row_sorter_get_property (GObject     *object,
                                       guint        prop_id,
                                       GValue      *value,
                                       GParamSpec  *pspec)
{
  GtkTreeListRowSorter *self = GTK_TREE_LIST_ROW_SORTER (object);

  switch (prop_id)
    {
    case PROP_SORTER:
      g_value_set_object (value, self->sorter);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_tree_list_row_sorter_class_init (GtkTreeListRowSorterClass *class)
{
  GtkSorterClass *sorter_class = GTK_SORTER_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  sorter_class->compare = gtk_tree_list_row_sorter_compare;
  sorter_class->get_order = gtk_tree_list_row_sorter_get_order;

  object_class->dispose = gtk_tree_list_row_sorter_dispose;
  object_class->set_property = gtk_tree_list_row_sorter_set_property;
  object_class->get_property = gtk_tree_list_row_sorter_get_property;

  /**
   * GtkTreeListRowSorter:sorter:
   *
   * The underlying sorter
   */
  properties[PROP_SORTER] =
      g_param_spec_object ("sorter",
                          P_("Sorter"),
                          P_("The underlyinh sorter"),
                          GTK_TYPE_SORTER,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);
}

static void
gtk_tree_list_row_sorter_init (GtkTreeListRowSorter *self)
{
}

/**
 * gtk_tree_list_row_sorter_new:
 * @sorter: (nullable) (transfer full): a #GtkSorter
 *
 * Create a special-purpose sorter that applies the sorting
 * of @sorter to the levels of a #GtkTreeListModel.
 *
 * Note that this sorter relies on #GtkTreeListModel:passthrough
 * being %FALSE as it can only sort #GtkTreeListRows.
 *
 * Returns: a new #GtkSorter
 */
GtkSorter *
gtk_tree_list_row_sorter_new (GtkSorter *sorter)
{
  GtkSorter *result;

  g_return_val_if_fail (sorter == NULL || GTK_IS_SORTER (sorter), NULL);

  result = g_object_new (GTK_TYPE_TREE_LIST_ROW_SORTER,
                         "sorter", sorter,
                         NULL);

  g_clear_object (&sorter);

  return result;
}

/**
 * gtk_tree_list_row_sorter_set_sorter:
 * @self: a #GtkTreeListRowSorter
 * @sorter: (nullable) (transfer none): The sorter to use or %NULL
 *     for none
 *
 * Sets the sorter to use for items with the same parent. This
 * sorter will be passed the GtkTreeListRow:item of the tree list rows
 * passed to @self.
 **/
void
gtk_tree_list_row_sorter_set_sorter (GtkTreeListRowSorter *self,
                                     GtkSorter            *sorter)
{
  g_return_if_fail (GTK_IS_TREE_LIST_ROW_SORTER (self));
  g_return_if_fail (sorter == NULL || GTK_IS_SORTER (sorter));

  if (self->sorter == sorter)
    return;

  if (self->sorter)
    g_signal_handlers_disconnect_by_func (self->sorter, propagate_changed, self);
  g_set_object (&self->sorter, sorter);
  if (self->sorter)
    g_signal_connect (sorter, "changed", G_CALLBACK (propagate_changed), self);

  gtk_sorter_changed (GTK_SORTER (self), GTK_SORTER_CHANGE_DIFFERENT);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SORTER]); 
}

/**
 * gtk_tree_list_row_sorter_get_sorter:
 * @self: a #GtkTreeListRowSorter
 *
 * Returns the sorter used by @self.
 *
 * Returns: (nullable) the sorter used
 **/
GtkSorter *
gtk_tree_list_row_sorter_get_sorter (GtkTreeListRowSorter *self)
{
  g_return_val_if_fail (GTK_IS_TREE_LIST_ROW_SORTER (self), NULL);

  return self->sorter;
}
