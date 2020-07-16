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

#include "gtkmultisorter.h"

#include "gtkbuildable.h"
#include "gtkintl.h"
#include "gtktypebuiltins.h"

#define GDK_ARRAY_TYPE_NAME GtkSorters
#define GDK_ARRAY_NAME gtk_sorters
#define GDK_ARRAY_ELEMENT_TYPE GtkSorter *
#define GDK_ARRAY_FREE_FUNC g_object_unref

#include "gdk/gdkarrayimpl.c"

/**
 * SECTION:gtkmultisorter
 * @Title: GtkMultiSorter
 * @Short_description: Combining multiple sorters
 *
 * GtkMultiSorter combines multiple sorters by trying them
 * in turn. If the first sorter compares two items as equal,
 * the second is tried next, and so on.
 */
struct _GtkMultiSorter
{
  GtkSorter parent_instance;

  GtkSorters sorters;
};

static GType
gtk_multi_sorter_get_item_type (GListModel *list)
{
  return GTK_TYPE_SORTER;
}

static guint
gtk_multi_sorter_get_n_items (GListModel *list)
{
  GtkMultiSorter *self = GTK_MULTI_SORTER (list);

  return gtk_sorters_get_size (&self->sorters);
}

static gpointer
gtk_multi_sorter_get_item (GListModel *list,
                           guint       position)
{
  GtkMultiSorter *self = GTK_MULTI_SORTER (list);

  if (position < gtk_sorters_get_size (&self->sorters))
    return g_object_ref (gtk_sorters_get (&self->sorters, position));
  else
    return NULL;
}

static void
gtk_multi_sorter_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_multi_sorter_get_item_type;
  iface->get_n_items = gtk_multi_sorter_get_n_items;
  iface->get_item = gtk_multi_sorter_get_item;
}

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_multi_sorter_buildable_add_child (GtkBuildable *buildable,
                                      GtkBuilder   *builder,
                                      GObject      *child,
                                      const gchar  *type)
{
  if (GTK_IS_SORTER (child))
    gtk_multi_sorter_append (GTK_MULTI_SORTER (buildable), g_object_ref (GTK_SORTER (child)));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_multi_sorter_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gtk_multi_sorter_buildable_add_child;
}

G_DEFINE_TYPE_WITH_CODE (GtkMultiSorter, gtk_multi_sorter, GTK_TYPE_SORTER,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_multi_sorter_list_model_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, gtk_multi_sorter_buildable_init))

static GtkOrdering
gtk_multi_sorter_compare (GtkSorter *sorter,
                          gpointer   item1,
                          gpointer   item2)
{
  GtkMultiSorter *self = GTK_MULTI_SORTER (sorter);
  GtkOrdering result = GTK_ORDERING_EQUAL;
  guint i;

  for (i = 0; i < gtk_sorters_get_size (&self->sorters); i++)
    {
      GtkSorter *child = gtk_sorters_get (&self->sorters, i);

      result = gtk_sorter_compare (child, item1, item2);
      if (result != GTK_ORDERING_EQUAL)
        break;
    }

  return result;
}

static GtkSorterOrder
gtk_multi_sorter_get_order (GtkSorter *sorter)
{
  GtkMultiSorter *self = GTK_MULTI_SORTER (sorter);
  GtkSorterOrder result = GTK_SORTER_ORDER_NONE;
  guint i;

  for (i = 0; i < gtk_sorters_get_size (&self->sorters); i++)
    {
      GtkSorter *child = gtk_sorters_get (&self->sorters, i);
      GtkSorterOrder child_order;

      child_order = gtk_sorter_get_order (child);
      switch (child_order)
        {
          case GTK_SORTER_ORDER_PARTIAL:
            result = GTK_SORTER_ORDER_PARTIAL;
            break;
          case GTK_SORTER_ORDER_NONE:
            break;
          case GTK_SORTER_ORDER_TOTAL:
            return GTK_SORTER_ORDER_TOTAL;
          default:
            g_assert_not_reached ();
            break;
        }
    }

  return result;
}

static void
gtk_multi_sorter_changed_cb (GtkSorter       *sorter,
                             GtkSorterChange  change,
                             GtkMultiSorter  *self)
{
  /* Using an enum on purpose, so gcc complains about this case if
   * new values are added to the enum
   */
  switch (change)
  {
    case GTK_SORTER_CHANGE_INVERTED:
      /* This could do a lot better with change handling, in particular in
       * cases where self->n_sorters == 1 or if sorter == self->sorters[0]
       */
      change = GTK_SORTER_CHANGE_DIFFERENT;
      break;

    case GTK_SORTER_CHANGE_DIFFERENT:
    case GTK_SORTER_CHANGE_LESS_STRICT:
    case GTK_SORTER_CHANGE_MORE_STRICT:
      break;

    default:
      g_assert_not_reached ();
      change = GTK_SORTER_CHANGE_DIFFERENT;
  }
  gtk_sorter_changed (GTK_SORTER (self), change);
}

static void
gtk_multi_sorter_dispose (GObject *object)
{
  GtkMultiSorter *self = GTK_MULTI_SORTER (object);
  guint i;

  for (i = 0; i < gtk_sorters_get_size (&self->sorters); i++)
    {
      GtkSorter *sorter = gtk_sorters_get (&self->sorters, i);
      g_signal_handlers_disconnect_by_func (sorter, gtk_multi_sorter_changed_cb, self);
    }
  gtk_sorters_clear (&self->sorters);

  G_OBJECT_CLASS (gtk_multi_sorter_parent_class)->dispose (object);
}

static void
gtk_multi_sorter_class_init (GtkMultiSorterClass *class)
{
  GtkSorterClass *sorter_class = GTK_SORTER_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  sorter_class->compare = gtk_multi_sorter_compare;
  sorter_class->get_order = gtk_multi_sorter_get_order;

  object_class->dispose = gtk_multi_sorter_dispose;
}

static void
gtk_multi_sorter_init (GtkMultiSorter *self)
{
  gtk_sorters_init (&self->sorters);
}

/**
 * gtk_multi_sorter_new:
 *
 * Creates a new multi sorter.
 *
 * This sorter compares items by trying each of the sorters
 * in turn, until one returns non-zero. In particular, if
 * no sorter has been added to it, it will always compare
 * items as equal.
 *
 * Returns: a new #GtkSorter
 */
GtkSorter *
gtk_multi_sorter_new (void)
{
  return g_object_new (GTK_TYPE_MULTI_SORTER, NULL);
}

/**
 * gtk_multi_sorter_append:
 * @self: a #GtkMultiSorter
 * @sorter: (transfer full): a sorter to add
 *
 * Add @sorter to @self to use for sorting at the end. @self
 * will consult all existing sorters before it will sort with
 * the given @sorter.
 */
void
gtk_multi_sorter_append (GtkMultiSorter *self,
                         GtkSorter      *sorter)
{
  g_return_if_fail (GTK_IS_MULTI_SORTER (self));
  g_return_if_fail (GTK_IS_SORTER (sorter));

  g_signal_connect (sorter, "changed", G_CALLBACK (gtk_multi_sorter_changed_cb), self);
  gtk_sorters_append (&self->sorters, sorter);

  gtk_sorter_changed (GTK_SORTER (self), GTK_SORTER_CHANGE_MORE_STRICT);
}

/**
 * gtk_multi_sorter_remove:
 * @self: a #GtkMultiSorter
 * @position: position of sorter to remove
 *
 * Removes the sorter at the given @position from the list of sorter
 * used by @self.
 *
 * If @position is larger than the number of sorters, nothing happens.
 */
void
gtk_multi_sorter_remove (GtkMultiSorter *self,
                         guint           position)
{
  guint length;
  GtkSorter *sorter;

  g_return_if_fail (GTK_IS_MULTI_SORTER (self));

  length = gtk_sorters_get_size (&self->sorters);
  if (position >= length)
    return;

  sorter = gtk_sorters_get (&self->sorters, position);
  g_signal_handlers_disconnect_by_func (sorter, gtk_multi_sorter_changed_cb, self);
  gtk_sorters_splice (&self->sorters, position, 1, NULL, 0);

  gtk_sorter_changed (GTK_SORTER (self), GTK_SORTER_CHANGE_LESS_STRICT);
}

