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

#include "gtkbuildable.h"
#include "gtkbuilderprivate.h"
#include "gtkintl.h"
#include "gtkprivate.h"

/**
 * SECTION:gtkstringlist
 * @title: GtkStringList
 * @short_description: A list model for strings
 * @see_also: #GListModel
 *
 * #GtkStringList is a list model that wraps an array of strings.
 *
 * The objects in the model have a "string" property.
 *
 * # GtkStringList as GtkBuildable
 *
 * The GtkStringList implementation of the GtkBuildable interface
 * supports adding items directly using the <items> element and
 * specifying <item> elements for each item. Each <item> element
 * supports the regular translation attributes “translatable”,
 * “context” and “comments”.
 *
 * Here is a UI definition fragment specifying a GtkStringList
 * |[
 * <object class="GtkStringList">
 *   <items>
 *     <item translatable="yes">Factory</item>
 *     <item translatable="yes">Home</item>
 *     <item translatable="yes">Subway</item>
 *   </items>
 * </object>
 * ]|

 */

struct _GtkStringObject
{
  GObject parent_instance;
  char *string;
};

enum {
  PROP_STRING = 1,
  PROP_NUM_PROPERTIES
};

G_DEFINE_TYPE (GtkStringObject, gtk_string_object, G_TYPE_OBJECT);

static void
gtk_string_object_init (GtkStringObject *object)
{
}

static void
gtk_string_object_finalize (GObject *object)
{
  GtkStringObject *self = GTK_STRING_OBJECT (object);

  g_free (self->string);

  G_OBJECT_CLASS (gtk_string_object_parent_class)->finalize (object);
}

static void
gtk_string_object_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GtkStringObject *self = GTK_STRING_OBJECT (object);

  switch (property_id)
    {
    case PROP_STRING:
      g_free (self->string);
      self->string = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_string_object_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GtkStringObject *self = GTK_STRING_OBJECT (object);

  switch (property_id)
    {
    case PROP_STRING:
      g_value_set_string (value, self->string);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_string_object_class_init (GtkStringObjectClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GParamSpec *pspec;

  object_class->finalize = gtk_string_object_finalize;
  object_class->set_property = gtk_string_object_set_property;
  object_class->get_property = gtk_string_object_get_property;

  pspec = g_param_spec_string ("string", "String", "String",
                               NULL,
                               G_PARAM_READABLE |
                               G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class, PROP_STRING, pspec);

}

static GtkStringObject *
gtk_string_object_new_take (char *string)
{
  GtkStringObject *obj;

  obj = g_object_new (GTK_TYPE_STRING_OBJECT, NULL);
  obj->string = string;

  return obj;
}

/**
 * gtk_string_object_new:
 * @string: (non-nullable): The string to wrap
 *
 * Wraps a string in an object for use with #GListModel
 *
 * Returns: a new #GtkStringObject
 **/
GtkStringObject *
gtk_string_object_new (const char *string)
{
  return gtk_string_object_new_take (g_strdup (string));
}

/**
 * gtk_string_object_get_string:
 * @self: a #GtkStringObject
 *
 * Returns the string contained in a #GtkStringObject.
 *
 * Returns: the string of @self
 */
const char *
gtk_string_object_get_string (GtkStringObject *self)
{
  g_return_val_if_fail (GTK_IS_STRING_OBJECT (self), NULL);

  return self->string;
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

typedef struct
{
  GtkBuilder    *builder;
  GtkStringList *list;
  GString       *string;
  const gchar   *domain;
  gchar         *context;

  guint          translatable : 1;
  guint          is_text      : 1;
} ItemParserData;

static void
item_start_element (GtkBuildableParseContext  *context,
                    const gchar               *element_name,
                    const gchar              **names,
                    const gchar              **values,
                    gpointer                   user_data,
                    GError                   **error)
{
  ItemParserData *data = (ItemParserData*)user_data;

  if (strcmp (element_name, "items") == 0)
    {
      if (!_gtk_builder_check_parent (data->builder, context, "object", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_INVALID, NULL, NULL,
                                        G_MARKUP_COLLECT_INVALID))
        _gtk_builder_prefix_error (data->builder, context, error);
    }
  else if (strcmp (element_name, "item") == 0)
    {
      gboolean translatable = FALSE;
      const gchar *msg_context = NULL;

      if (!_gtk_builder_check_parent (data->builder, context, "items", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_BOOLEAN|G_MARKUP_COLLECT_OPTIONAL, "translatable", &translatable,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "comments", NULL,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "context", &msg_context,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      data->is_text = TRUE;
      data->translatable = translatable;
      data->context = g_strdup (msg_context);
    }
  else
    {
      _gtk_builder_error_unhandled_tag (data->builder, context,
                                        "GtkStringList", element_name,
                                        error);
    }
}

static void
item_text (GtkBuildableParseContext  *context,
           const gchar               *text,
           gsize                      text_len,
           gpointer                   user_data,
           GError                   **error)
{
  ItemParserData *data = (ItemParserData*)user_data;

  if (data->is_text)
    g_string_append_len (data->string, text, text_len);
}

static void
item_end_element (GtkBuildableParseContext  *context,
                  const gchar               *element_name,
                  gpointer                   user_data,
                  GError                   **error)
{
  ItemParserData *data = (ItemParserData*)user_data;

  /* Append the translated strings */
  if (data->string->len)
    {
      if (data->translatable)
        {
          const char *translated;

          translated = _gtk_builder_parser_translate (data->domain,
                                                      data->context,
                                                      data->string->str);
          g_string_assign (data->string, translated);
        }

      g_sequence_append (data->list->items, gtk_string_object_new (data->string->str));
    }

  data->translatable = FALSE;
  g_string_set_size (data->string, 0);
  g_clear_pointer (&data->context, g_free);
  data->is_text = FALSE;
}

static const GtkBuildableParser item_parser =
{
  item_start_element,
  item_end_element,
  item_text
};

static gboolean
gtk_string_list_buildable_custom_tag_start (GtkBuildable       *buildable,
                                            GtkBuilder         *builder,
                                            GObject            *child,
                                            const gchar        *tagname,
                                            GtkBuildableParser *parser,
                                            gpointer           *parser_data)
{
  if (strcmp (tagname, "items") == 0)
    {
      ItemParserData *data;

      data = g_slice_new0 (ItemParserData);
      data->builder = g_object_ref (builder);
      data->list = g_object_ref (GTK_STRING_LIST (buildable));
      data->domain = gtk_builder_get_translation_domain (builder);
      data->string = g_string_new ("");

      *parser = item_parser;
      *parser_data = data;

      return TRUE;
    }

  return FALSE;
}

static void
gtk_string_list_buildable_custom_finished (GtkBuildable *buildable,
                                           GtkBuilder   *builder,
                                           GObject      *child,
                                           const gchar  *tagname,
                                           gpointer      user_data)
{
  if (strcmp (tagname, "items") == 0)
    {
      ItemParserData *data;

      data = (ItemParserData*)user_data;
      g_object_unref (data->list);
      g_object_unref (data->builder);
      g_string_free (data->string, TRUE);
      g_slice_free (ItemParserData, data);
    }
}

static void
gtk_string_list_buildable_init (GtkBuildableIface *iface)
{
  iface->custom_tag_start = gtk_string_list_buildable_custom_tag_start;
  iface->custom_finished = gtk_string_list_buildable_custom_finished;
}

G_DEFINE_TYPE_WITH_CODE (GtkStringList, gtk_string_list, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_string_list_buildable_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                                gtk_string_list_model_init))

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
 * @strings: (array zero-terminated=1) (nullable): The strings to put in the model
 *
 * Creates a new #GtkStringList with the given @strings.
 *
 * Returns: a new #GtkStringList
 */
GtkStringList *
gtk_string_list_new (const char * const *strings)
{
  GtkStringList *self;

  self = g_object_new (GTK_TYPE_STRING_LIST, NULL);

  gtk_string_list_splice (self, 0, 0, strings);

  return self;
}

/**
 * gtk_string_list_splice:
 * @self: a #GtkStringList
 * @position: the position at which to make the change
 * @n_removals: the number of strings to remove
 * @additions: (array zero-terminated=1) (nullable): The strings to add
 *
 * Changes @self by removing @n_removals strings and adding @additions
 * to it.
 *
 * This function is more efficient than gtk_string_list_insert() and
 * gtk_string_list_remove(), because it only emits
 * #GListModel::items-changed once for the change.
 *
 * This function takes a ref on each item in @additions.
 *
 * The parameters @position and @n_removals must be correct (ie:
 * @position + @n_removals must be less than or equal to the length
 * of the list at the time this function is called).
 */
void
gtk_string_list_splice (GtkStringList      *self,
                        guint               position,
                        guint               n_removals,
                        const char * const *additions)
{
  GSequenceIter *it;
  guint add, n_items;

  g_return_if_fail (GTK_IS_STRING_LIST (self));
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
          g_sequence_insert_before (it, gtk_string_object_new (additions[add]));
        }
    }
  else
    add = 0;

  if (n_removals || add)
    g_list_model_items_changed (G_LIST_MODEL (self), position, n_removals, add);
}

/**
 * gtk_string_list_append:
 * @self: a #GtkStringList
 * @string: the string to insert
 *
 * Appends @string to @self.
 *
 * The @string will be copied. See gtk_string_list_take()
 * for a way to avoid that.
 */
void
gtk_string_list_append (GtkStringList *self,
                        const char    *string)
{
  guint n_items;

  g_return_if_fail (GTK_IS_STRING_LIST (self));

  n_items = g_sequence_get_length (self->items);
  g_sequence_append (self->items, gtk_string_object_new (string));

  g_list_model_items_changed (G_LIST_MODEL (self), n_items, 0, 1);
}

/**
 * gtk_string_list_take:
 * @self: a #GtkStringList
 * @string: (transfer full): the string to insert
 *
 * Adds @string to self at the end, and takes
 * ownership of it.
 *
 * This variant of gtk_string_list_append() is
 * convenient for formatting strings:
 *
 * |[
 * gtk_string_list_take (self, g_strdup_print ("%d dollars", lots));
 * ]|
 */
void
gtk_string_list_take (GtkStringList *self,
                      char          *string)
{
  guint n_items;

  g_return_if_fail (GTK_IS_STRING_LIST (self));

  n_items = g_sequence_get_length (self->items);
  g_sequence_append (self->items, gtk_string_object_new_take (string));

  g_list_model_items_changed (G_LIST_MODEL (self), n_items, 0, 1);
}

/**
 * gtk_string_list_remove:
 * @self: a #GtkStringList
 * @position: the position of the string that is to be removed
 *
 * Removes the string at @position from @self. @position must
 * be smaller than the current length of the list.
 */
void
gtk_string_list_remove (GtkStringList *self,
                        guint          position)
{
  GSequenceIter *iter;

  g_return_if_fail (GTK_IS_STRING_LIST (self));

  iter = g_sequence_get_iter_at_pos (self->items, position);
  g_return_if_fail (!g_sequence_iter_is_end (iter));

  g_sequence_remove (iter);

  g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);
}

/**
 * gtk_string_list_get_string:
 * @self: a #GtkStringList
 * @position: the position to get the string for
 *
 * Gets the string that is at @position in @self. If @self
 * does not contain @position items, %NULL is returned.
 *
 * This function returns the const char *. To get the
 * object wrapping it, use g_list_model_get_item().
 *
 * Returns: the string at the given position
 */
const char *
gtk_string_list_get_string (GtkStringList *self,
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

      return obj->string;
    }
}
