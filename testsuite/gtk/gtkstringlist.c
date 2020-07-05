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

#include <Judy.h>

struct _GtkStringList2
{
  GObject parent_instance;

  gpointer items;
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
  guint64 count;

  GtkStringList2 *self = GTK_STRING_LIST2 (list);

  JLC (count, self->items, 0, -1);

  return count;
}

static gpointer
gtk_string_list2_get_item (GListModel *list,
                          guint       position)
{
  GtkStringList2 *self = GTK_STRING_LIST2 (list);
  guint64 index = position;
  GObject **item;

  JLG (item, self->items, index);

  if (!item)
    return NULL;

  return g_object_ref (*item);
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
  guint64 count;

  JLFA (count, self->items);

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
  guint64 n_items;
  guint64 index = position;
  guint add;
  guint i;
  int retval;

  g_return_if_fail (GTK_IS_STRING_LIST2 (self));
  g_return_if_fail (position + n_removals >= position); /* overflow */

  JLC (n_items, self->items, 0, -1);

  g_return_if_fail (position + n_removals <= n_items);

  if (n_removals)
    {
      for (i = 0; i < n_removals; i++)
        JLD (retval, self->items, index);
    }

  if (additions)
    {
      for (add = 0; additions[add]; add++)
        {
          gpointer *item;

          JLI (item, self->items, index + add);

          *item = string_object_new (additions[add]);
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
  guint64 n_items;
  gpointer *item;

  g_return_if_fail (GTK_IS_STRING_LIST2 (self));

  JLC (n_items, self->items, 0, -1);

  JLI (item, self->items, n_items);

  *item = string_object_new (string);

  g_list_model_items_changed (G_LIST_MODEL (self), (guint)n_items, 0, 1);
}

void
gtk_string_list2_remove (GtkStringList2 *self,
                        guint          position)
{
  guint64 index = position;
  int retval;

  g_return_if_fail (GTK_IS_STRING_LIST2 (self));

  JLD (retval, self->items, index);

  g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);
}

const char *
gtk_string_list2_get_string (GtkStringList2 *self,
                            guint          position)
{
  guint64 index = position;
  GtkStringObject **item;

  g_return_val_if_fail (GTK_IS_STRING_LIST (self), NULL);

  JLG (item, self->items, index);
  if (!item)
    return NULL;
   else
    return gtk_string_object_get_string (*item);
}

guint64
gtk_string_list2_get_size (GtkStringList2 *self)
{
  guint size;

  JLMU (size, self->items);

  return sizeof (GtkStringList2) + size;
}
