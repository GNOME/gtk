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

#include "gtkstringlist.h"

#include "gtkintl.h"
#include "gtkprivate.h"

/**
 * SECTION:gtkstringlist
 * @title: GtkStringList
 * @short_description: A list model for strings
 * @see_also: #GListModel
 *
 * #GtkStringList is a list model that wraps a %NULL-terminated array of strings.
 *
 * The objects in the model have a "string" property.
 */

struct _GtkStringHolder
{
  GObject parent_instance;
  char *string;
};

enum {
  PROP_STRING = 1,
  PROP_NUM_PROPERTIES
};

G_DEFINE_TYPE (GtkStringHolder, gtk_string_holder, G_TYPE_OBJECT);

static void
gtk_string_holder_init (GtkStringHolder *holder)
{
}

static void
gtk_string_holder_finalize (GObject *object)
{
  GtkStringHolder *holder = GTK_STRING_HOLDER (object);

  g_free (holder->string);

  G_OBJECT_CLASS (gtk_string_holder_parent_class)->finalize (object);
}

static void
gtk_string_holder_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GtkStringHolder *holder = GTK_STRING_HOLDER (object);

  switch (property_id)
    {
    case PROP_STRING:
      g_free (holder->string);
      holder->string = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_string_holder_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GtkStringHolder *holder = GTK_STRING_HOLDER (object);

  switch (property_id)
    {
    case PROP_STRING:
      g_value_set_string (value, holder->string);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}


static void
gtk_string_holder_class_init (GtkStringHolderClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GParamSpec *pspec;

  object_class->finalize = gtk_string_holder_finalize;
  object_class->set_property = gtk_string_holder_set_property;
  object_class->get_property = gtk_string_holder_get_property;

  pspec = g_param_spec_string ("string", "String", "String",
                               NULL,
                               G_PARAM_READWRITE |
                               G_PARAM_CONSTRUCT_ONLY |
                               G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class, PROP_STRING, pspec);

}

static GtkStringHolder *
gtk_string_holder_new (const char *string)
{
  return g_object_new (GTK_TYPE_STRING_HOLDER, "string", string, NULL);
}


struct _GtkStringList
{
  GObject parent_instance;

  GSequence *items;
};

struct _GtkStringListClass
{
  GObjectClass parent_class;
};

static GType
gtk_string_list_get_item_type (GListModel *list)
{
  return G_TYPE_OBJECT;
}

static guint
gtk_string_list_get_n_items (GListModel *list)
{
  GtkStringList *self = GTK_STRING_LIST (list);

  return g_sequence_get_length (self->items);
}

static gpointer
gtk_string_list_get_item (GListModel *list,
                          guint       position)
{
  GtkStringList *self = GTK_STRING_LIST (list);
  GSequenceIter *iter;

  iter = g_sequence_get_iter_at_pos (self->items, position);

  if (g_sequence_iter_is_end (iter))
    return NULL;
  else
    return g_object_ref (g_sequence_get (iter));
}

static void
gtk_string_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_string_list_get_item_type;
  iface->get_n_items = gtk_string_list_get_n_items;
  iface->get_item = gtk_string_list_get_item;
}

G_DEFINE_TYPE_WITH_CODE (GtkStringList, gtk_string_list, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_string_list_model_init))

static void
gtk_string_list_dispose (GObject *object)
{
  GtkStringList *self = GTK_STRING_LIST (object);

  g_clear_pointer (&self->items, g_sequence_free);

  G_OBJECT_CLASS (gtk_string_list_parent_class)->dispose (object);
}

static void
gtk_string_list_class_init (GtkStringListClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->dispose = gtk_string_list_dispose;
}

static void
gtk_string_list_init (GtkStringList *self)
{
  self->items = g_sequence_new (g_object_unref);
}

/**
 * gtk_string_list_new:
 * @strings: (allow-none): The strings to put in the model
 * @length: length @strings, or -1 if @strings i %NULL-terminated
 *
 * Creates a new #GtkStringList with the given @strings.
 *
 * Returns: a new #GtkStringList
 **/
GtkStringList *
gtk_string_list_new (const char * const *strings,
                     int                 length)
{
  GtkStringList *list;

  list = g_object_new (GTK_TYPE_STRING_LIST, NULL);

  if (strings)
    {
      guint i;

      for (i = 0; (length == -1 || i < length) && strings[i]; i++)
        g_sequence_append (list->items, gtk_string_holder_new (strings[i]));
    }

  return list;
}
