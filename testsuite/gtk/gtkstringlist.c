/*
 * Copyright © 2020 Red Hat, Inc.
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

#include <gtk.h>
#include "gtkstringlist.h"

struct _GtkStringList2
{
  GObject parent_instance;

  GSequence *items;
};

struct _GtkStringList2Class
{
  GObjectClass parent_class;
};

static GType
gtk_string_list2_get_item_type (GListModel *list)
{
  return G_TYPE_OBJECT;
}

static guint
gtk_string_list2_get_n_items (GListModel *list)
{
  GtkStringList2 *self = GTK_STRING_LIST2 (list);

  return g_sequence_get_length (self->items);
}

static gpointer
gtk_string_list2_get_item (GListModel *list,
                          guint       position)
{
  GtkStringList2 *self = GTK_STRING_LIST2 (list);
  GSequenceIter *iter;

  iter = g_sequence_get_iter_at_pos (self->items, position);

  if (g_sequence_iter_is_end (iter))
     return NULL;
  else
    return g_object_ref (g_sequence_get (iter));
}

static void
gtk_string_list2_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_string_list2_get_item_type;
  iface->get_n_items = gtk_string_list2_get_n_items;
  iface->get_item = gtk_string_list2_get_item;
}

G_DEFINE_TYPE_WITH_CODE (GtkStringList2, gtk_string_list2, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                                gtk_string_list2_model_init))

static void
gtk_string_list2_dispose (GObject *object)
{
  GtkStringList2 *self = GTK_STRING_LIST2 (object);

  g_clear_pointer (&self->items, g_sequence_free);

  G_OBJECT_CLASS (gtk_string_list2_parent_class)->dispose (object);
}

static void
gtk_string_list2_class_init (GtkStringList2Class *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->dispose = gtk_string_list2_dispose;
}

static void
gtk_string_list2_init (GtkStringList2 *self)
{
  self->items = g_sequence_new (g_object_unref);
}

GtkStringList2 *
gtk_string_list2_new (const char * const *strings)
{
  GtkStringList2 *self;

  self = g_object_new (GTK_TYPE_STRING_LIST2, NULL);

  gtk_string_list2_splice (self, 0, 0, strings);

  return self;
}

typedef struct {
  GObject obj;
  char *str;
} StrObj;

static GtkStringObject *
string_object_new (const char *string)
{
  GtkStringObject *s;

  s = g_object_new (GTK_TYPE_STRING_OBJECT, NULL);
  ((StrObj*)s)->str = g_strdup (string);

  return s;
}

void
gtk_string_list2_splice (GtkStringList2      *self,
                        guint               position,
                        guint               n_removals,
                        const char * const *additions)
{
  guint n_items;
  guint add;
  GSequenceIter *it;

  g_return_if_fail (GTK_IS_STRING_LIST2 (self));
  g_return_if_fail (position + n_removals >= position); /* overflow */

  n_items = g_sequence_get_length (self->items);
  g_return_if_fail (position + n_removals <= n_items);

  it = g_sequence_get_iter_at_pos (self->items, position);

  if (n_removals)
    {
      GSequenceIter *end;

      end = g_sequence_iter_move (it, n_removals);
      g_sequence_remove_range (it, end);

      it = end;
    }

  if (additions)
    {
      for (add = 0; additions[add]; add++)
        {
          g_sequence_insert_before (it, string_object_new (additions[add]));
        }
    }
  else
    add = 0;

  if (n_removals || add)
    g_list_model_items_changed (G_LIST_MODEL (self), position, n_removals, add);
}

void
gtk_string_list2_append (GtkStringList2 *self,
                        const char    *string)
{
  guint n_items;

  g_return_if_fail (GTK_IS_STRING_LIST2 (self));

  n_items = g_sequence_get_length (self->items);
  g_sequence_append (self->items, string_object_new (string));

  g_list_model_items_changed (G_LIST_MODEL (self), n_items, 0, 1);
}

void
gtk_string_list2_remove (GtkStringList2 *self,
                        guint          position)
{
  GSequenceIter *iter;

  g_return_if_fail (GTK_IS_STRING_LIST2 (self));

  iter = g_sequence_get_iter_at_pos (self->items, position);
  g_return_if_fail (!g_sequence_iter_is_end (iter));

  g_sequence_remove (iter);

  g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);
}

const char *
gtk_string_list2_get_string (GtkStringList2 *self,
                            guint          position)
{
  GSequenceIter *iter;

   g_return_val_if_fail (GTK_IS_STRING_LIST (self), NULL);

  iter = g_sequence_get_iter_at_pos (self->items, position);

  if (g_sequence_iter_is_end (iter))
    {
      return NULL;
    }
   else
    {
      GtkStringObject *obj = g_sequence_get (iter);

      return gtk_string_object_get_string (obj);
    }
}
