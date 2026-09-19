/* gtkemojiitem.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gtkemojiitemprivate.h"

#include "gtkemojidataprivate.h"

#define MAX_RECENT 21

typedef struct
{
  guint offset;
  guint size;
} GtkEmojiGroupRange;

/* All model access and final unrefs must occur on the construction thread.
 * The queue and embedded links are deliberately not synchronized. */
struct _GtkEmojiDatabase
{
  GObject parent_instance;
  GVariant *records;
  GtkEmojiItem **live_items; /* Non-owning indexed lookup. */
  GQueue linked_items;       /* Intrusive links owned by the items. */
  GtkEmojiGroupRange groups[11];
  GThread *owner;
  guint n_items;
  guint get_item_calls;
  guint disposed : 1;
};

struct _GtkEmojiItem
{
  GObject parent_instance;
  GtkEmojiDatabase *database; /* Strong reference for metadata lifetime. */
  GtkEmojiDatabase *registry; /* Cleared when the database is disposed. */
  GVariant *record; /* Owned only by standalone items (recents and variations). */
  gunichar modifier;
  GList link;
  guint id;
};

static void gtk_emoji_database_list_model_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkEmojiDatabase, gtk_emoji_database, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_emoji_database_list_model_init))
G_DEFINE_TYPE (GtkEmojiItem, gtk_emoji_item, G_TYPE_OBJECT)

enum { PROP_0, PROP_ITEM_TYPE, PROP_N_ITEMS, N_PROPS };

static GType
gtk_emoji_database_get_item_type (GListModel *model)
{
  return GTK_TYPE_EMOJI_ITEM;
}

static guint
gtk_emoji_database_get_n_items (GListModel *model)
{
  return GTK_EMOJI_DATABASE (model)->n_items;
}

static gpointer
gtk_emoji_database_get_item (GListModel *model,
                             guint       position)
{
  GtkEmojiDatabase *self = GTK_EMOJI_DATABASE (model);
  GtkEmojiItem *item;

  g_assert (g_thread_self () == self->owner);
  self->get_item_calls++;
  if (self->disposed || position >= self->n_items)
    return NULL;

  if (self->live_items == NULL)
    self->live_items = g_new0 (GtkEmojiItem *, self->n_items);

  item = self->live_items[position];
  if (item != NULL)
    return g_object_ref (item);

  item = g_object_new (GTK_TYPE_EMOJI_ITEM, NULL);
  item->database = g_object_ref (self);
  item->registry = self;
  item->id = position;
  g_queue_push_tail_link (&self->linked_items, &item->link);
  self->live_items[position] = item;

  return item;
}

static void
gtk_emoji_database_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_emoji_database_get_item_type;
  iface->get_n_items = gtk_emoji_database_get_n_items;
  iface->get_item = gtk_emoji_database_get_item;
}

static void
gtk_emoji_database_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GtkEmojiDatabase *self = GTK_EMOJI_DATABASE (object);

  switch (prop_id)
    {
    case PROP_ITEM_TYPE:
      g_value_set_gtype (value, GTK_TYPE_EMOJI_ITEM);
      break;
    case PROP_N_ITEMS:
      g_value_set_uint (value, self->n_items);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_emoji_database_dispose (GObject *object)
{
  GtkEmojiDatabase *self = GTK_EMOJI_DATABASE (object);
  GList *link;

  g_assert (g_thread_self () == self->owner);
  self->disposed = TRUE;

  while ((link = g_queue_peek_head_link (&self->linked_items)))
    {
      GtkEmojiItem *item = link->data;

      g_queue_unlink (&self->linked_items, link);
      item->registry = NULL;
    }

  g_clear_pointer (&self->live_items, g_free);

  G_OBJECT_CLASS (gtk_emoji_database_parent_class)->dispose (object);
}

static void
gtk_emoji_database_finalize (GObject *object)
{
  GtkEmojiDatabase *self = GTK_EMOJI_DATABASE (object);

  g_clear_pointer (&self->records, g_variant_unref);
  G_OBJECT_CLASS (gtk_emoji_database_parent_class)->finalize (object);
}

static void
gtk_emoji_database_class_init (GtkEmojiDatabaseClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gtk_emoji_database_get_property;
  object_class->dispose = gtk_emoji_database_dispose;
  object_class->finalize = gtk_emoji_database_finalize;

  g_object_class_install_property (object_class, PROP_ITEM_TYPE,
                                   g_param_spec_gtype ("item-type", NULL, NULL,
                                                       G_TYPE_OBJECT,
                                                       (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_N_ITEMS,
                                   g_param_spec_uint ("n-items", NULL, NULL,
                                                      0, G_MAXUINT, 0,
                                                      (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));
}

static void
gtk_emoji_database_init (GtkEmojiDatabase *self)
{
  self->owner = g_thread_self ();
  g_queue_init (&self->linked_items);
}

GtkEmojiDatabase *
gtk_emoji_database_new (void)
{
  GtkEmojiDatabase *self;
  GBytes *bytes;

  self = g_object_new (GTK_TYPE_EMOJI_DATABASE, NULL);
  bytes = gtk_emoji_data_load ();
  self->records = g_variant_ref_sink (g_variant_new_from_bytes (G_VARIANT_TYPE ("a(aussasasu)"),
                                                               bytes, TRUE));
  g_bytes_unref (bytes);
  self->n_items = g_variant_n_children (self->records);

  /* Emoji data is ordered by group, so each group can back a slice model.
   * Group 2 contains components that the chooser does not show. Group 10
   * collects any future or otherwise unknown group. */
  for (guint id = 0; id < self->n_items; id++)
    {
      GVariant *record = g_variant_get_child_value (self->records, id);
      guint group;

      g_variant_get_child (record, 5, "u", &group);
      g_variant_unref (record);
      group = MIN (group, G_N_ELEMENTS (self->groups) - 1);
      if (self->groups[group].size == 0)
        self->groups[group].offset = id;
      else if (self->groups[group].offset + self->groups[group].size != id)
        g_error ("Emoji group %u is not contiguous", group);
      self->groups[group].size++;
    }

  return self;
}

guint
gtk_emoji_database_get_item_calls (GtkEmojiDatabase *self)
{
  g_return_val_if_fail (GTK_IS_EMOJI_DATABASE (self), 0);
  g_return_val_if_fail (g_thread_self () == self->owner, 0);

  return self->get_item_calls;
}

gboolean
gtk_emoji_database_is_owner (GtkEmojiDatabase *self)
{
  g_return_val_if_fail (GTK_IS_EMOJI_DATABASE (self), FALSE);

  return g_thread_self () == self->owner;
}

guint
gtk_emoji_database_get_n_live_items (GtkEmojiDatabase *self)
{
  g_return_val_if_fail (GTK_IS_EMOJI_DATABASE (self), 0);
  g_return_val_if_fail (g_thread_self () == self->owner, 0);

  return self->linked_items.length;
}

GVariant *
gtk_emoji_database_dup_record (GtkEmojiDatabase *self,
                               guint             id)
{
  g_return_val_if_fail (GTK_IS_EMOJI_DATABASE (self), NULL);
  g_return_val_if_fail (g_thread_self () == self->owner, NULL);

  if (id >= self->n_items)
    return NULL;

  return g_variant_get_child_value (self->records, id);
}

void
gtk_emoji_database_get_group_range (GtkEmojiDatabase *self,
                                    guint             group,
                                    guint            *out_offset,
                                    guint            *out_size)
{
  g_return_if_fail (GTK_IS_EMOJI_DATABASE (self));
  g_return_if_fail (g_thread_self () == self->owner);
  g_return_if_fail (out_offset != NULL);
  g_return_if_fail (out_size != NULL);

  if (group >= G_N_ELEMENTS (self->groups))
    {
      *out_offset = self->n_items;
      *out_size = 0;
      return;
    }

  *out_offset = self->groups[group].offset;
  *out_size = self->groups[group].size;
}

char *
gtk_emoji_database_dup_text (GtkEmojiDatabase *self,
                             guint             id,
                             gunichar          modifier)
{
  g_autoptr(GVariant) record = NULL;

  g_return_val_if_fail (GTK_IS_EMOJI_DATABASE (self), NULL);

  if (!(record = gtk_emoji_database_dup_record (self, id)))
    return NULL;

  return gtk_emoji_data_dup_text (record, modifier);
}

/* Items hold metadata alive, but the database never owns item references.
 * Disposing either side unregisters the intrusive link exactly once. */
static void
gtk_emoji_item_dispose (GObject *object)
{
  GtkEmojiItem *self = GTK_EMOJI_ITEM (object);

  g_clear_pointer (&self->record, g_variant_unref);

  if (self->database == NULL)
    {
      G_OBJECT_CLASS (gtk_emoji_item_parent_class)->dispose (object);
      return;
    }

  g_assert (g_thread_self () == self->database->owner);
  if (self->registry != NULL)
    {
      g_assert (self->registry->live_items[self->id] == self);
      self->registry->live_items[self->id] = NULL;
      g_queue_unlink (&self->registry->linked_items, &self->link);
      self->registry = NULL;
    }

  g_clear_object (&self->database);

  G_OBJECT_CLASS (gtk_emoji_item_parent_class)->dispose (object);
}

static void
gtk_emoji_item_class_init (GtkEmojiItemClass *klass)
{
  G_OBJECT_CLASS (klass)->dispose = gtk_emoji_item_dispose;
}

static void
gtk_emoji_item_init (GtkEmojiItem *self)
{
  self->link.data = self;
  self->id = G_MAXUINT;
}

guint
gtk_emoji_item_get_id (GtkEmojiItem *self)
{
  g_return_val_if_fail (GTK_IS_EMOJI_ITEM (self), G_MAXUINT);

  return self->id;
}

GtkEmojiDatabase *
gtk_emoji_item_get_database (GtkEmojiItem *self)
{
  g_return_val_if_fail (GTK_IS_EMOJI_ITEM (self), NULL);

  return self->database;
}

/* Standalone records deliberately do not participate in the database registry.
 * Their identity includes the modifier, and their record survives locale changes. */
GtkEmojiItem *
gtk_emoji_item_new (GVariant *record,
                    gunichar  modifier)
{
  GtkEmojiItem *self;

  g_return_val_if_fail (record != NULL, NULL);
  g_return_val_if_fail (g_variant_is_of_type (record, G_VARIANT_TYPE ("(aussasasu)")), NULL);

  self = g_object_new (GTK_TYPE_EMOJI_ITEM, NULL);
  self->record = g_variant_ref_sink (record);
  self->modifier = modifier;
  return self;
}

GVariant *
gtk_emoji_item_dup_record (GtkEmojiItem *self)
{
  g_return_val_if_fail (GTK_IS_EMOJI_ITEM (self), NULL);

  if (self->record != NULL)
    return g_variant_ref (self->record);
  if (self->database != NULL)
    return gtk_emoji_database_dup_record (self->database, self->id);
  return NULL;
}

char *
gtk_emoji_item_dup_text (GtkEmojiItem *self)
{
  g_autoptr(GVariant) record = NULL;

  g_return_val_if_fail (GTK_IS_EMOJI_ITEM (self), NULL);

  record = gtk_emoji_item_dup_record (self);
  return record ? gtk_emoji_data_dup_text (record, self->modifier) : NULL;
}

gunichar
gtk_emoji_item_get_modifier (GtkEmojiItem *self)
{
  g_return_val_if_fail (GTK_IS_EMOJI_ITEM (self), 0);

  return self->modifier;
}

/* Recents retain complete records, including records missing from today's
 * locale database. Only this bounded, mutable store is serialized. */
void
gtk_emoji_recent_add (GListStore   *store,
                      GtkEmojiItem *item)
{
  g_autoptr(GVariant) record = NULL;
  g_autoptr(GtkEmojiItem) recent = NULL;
  gunichar modifier;

  g_return_if_fail (G_IS_LIST_STORE (store));
  g_return_if_fail (GTK_IS_EMOJI_ITEM (item));

  record = gtk_emoji_item_dup_record (item);
  modifier = gtk_emoji_item_get_modifier (item);
  for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (store));)
    {
      g_autoptr(GtkEmojiItem) other = g_list_model_get_item (G_LIST_MODEL (store), i);
      g_autoptr(GVariant) other_record = gtk_emoji_item_dup_record (other);

      if (modifier == gtk_emoji_item_get_modifier (other) && g_variant_equal (record, other_record))
        g_list_store_remove (store, i);
      else
        i++;
    }
  recent = gtk_emoji_item_new (record, modifier);
  g_list_store_insert (store, 0, recent);
  while (g_list_model_get_n_items (G_LIST_MODEL (store)) > MAX_RECENT)
    g_list_store_remove (store, MAX_RECENT);
}

void
gtk_emoji_recent_load (GListStore *store,
                       GVariant   *saved)
{
  g_return_if_fail (G_IS_LIST_STORE (store));
  g_return_if_fail (g_variant_is_of_type (saved, G_VARIANT_TYPE ("a((aussasasu)u)")));

  g_list_store_remove_all (store);
  /* Reverse insertion preserves the saved most-recent-first order. */
  for (gsize i = g_variant_n_children (saved); i > 0; i--)
    {
      g_autoptr(GVariant) entry = g_variant_get_child_value (saved, i - 1);
      g_autoptr(GVariant) record = g_variant_get_child_value (entry, 0);
      g_autoptr(GtkEmojiItem) item = NULL;
      gunichar modifier;

      g_variant_get_child (entry, 1, "u", &modifier);
      item = gtk_emoji_item_new (record, modifier);
      gtk_emoji_recent_add (store, item);
    }
}

GVariant *
gtk_emoji_recent_serialize (GListStore *store)
{
  GVariantBuilder builder;

  g_return_val_if_fail (G_IS_LIST_STORE (store), NULL);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a((aussasasu)u)"));
  for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (store)); i++)
    {
      g_autoptr(GtkEmojiItem) item = g_list_model_get_item (G_LIST_MODEL (store), i);
      g_autoptr(GVariant) record = gtk_emoji_item_dup_record (item);

      g_variant_builder_add (&builder, "(@(aussasasu)u)", record, gtk_emoji_item_get_modifier (item));
    }
  return g_variant_builder_end (&builder);
}
