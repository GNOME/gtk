/*
 * Copyright © 2022 Red Hat, Inc.
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

#include "gtkemojilist.h"

#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"
#include "gtksectionmodel.h"

#define GDK_ARRAY_ELEMENT_TYPE GtkEmojiObject *
#define GDK_ARRAY_NAME objects
#define GDK_ARRAY_TYPE_NAME Objects
#define GDK_ARRAY_FREE_FUNC g_object_unref
#include "gdk/gdkarrayimpl.c"

struct _GtkEmojiObject
{
  GObject parent_instance;
  GVariant *data;
  gboolean is_recent;
  gunichar modifier;
};

enum {
  PROP_NAME = 1,
  PROP_GROUP,
  PROP_NUM_PROPERTIES
};

G_DEFINE_TYPE (GtkEmojiObject, gtk_emoji_object, G_TYPE_OBJECT);

static void
gtk_emoji_object_init (GtkEmojiObject *object)
{
}

static void
gtk_emoji_object_finalize (GObject *object)
{
  GtkEmojiObject *self = GTK_EMOJI_OBJECT (object);

  g_variant_unref (self->data);

  G_OBJECT_CLASS (gtk_emoji_object_parent_class)->finalize (object);
}

static void
gtk_emoji_object_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GtkEmojiObject *self = GTK_EMOJI_OBJECT (object);

  switch (property_id)
    {
    case PROP_NAME:
      g_value_set_string (value, gtk_emoji_object_get_name (self));
      break;

    case PROP_GROUP:
      g_value_set_enum (value, gtk_emoji_object_get_group (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_emoji_object_class_init (GtkEmojiObjectClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GParamSpec *pspec;

  object_class->finalize = gtk_emoji_object_finalize;
  object_class->get_property = gtk_emoji_object_get_property;

  /**
   * GtkEmojiObject:name: (attributes org.gtk.Property.get=gtk_emoji_object_get_name)
   *
   * The name.
   */
  pspec = g_param_spec_string ("name", "Name", "Name",
                               NULL,
                               G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class, PROP_NAME, pspec);

  pspec = g_param_spec_enum ("group", "Group", "Group",
                             GTK_TYPE_EMOJI_GROUP, GTK_EMOJI_GROUP_SMILEYS,
                             G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class, PROP_GROUP, pspec);

}

static GtkEmojiObject *
gtk_emoji_object_new (GVariant *data,
                      gboolean  recent,
                      gunichar  modifier)
{
  GtkEmojiObject *obj;

  obj = g_object_new (GTK_TYPE_EMOJI_OBJECT, NULL);
  obj->data = g_variant_ref (data);
  obj->is_recent = recent;
  obj->modifier = modifier;

  return obj;
}

/**
 * gtk_emoji_object_get_emoji: (attributes org.gtk.Method.get_property=emoji)
 * @self: a `GtkEmojiObject`
 *
 * Returns the emoji contained in a `GtkEmojiObject`.
 *
 * Returns: the emoji of @self
 */
void
gtk_emoji_object_get_text (GtkEmojiObject *self,
                           char           *buffer,
                           int             length,
                           gunichar        modifier)
{
  g_return_if_fail (GTK_IS_EMOJI_OBJECT (self));
  GVariant *codes;
  char *p = buffer;
  gunichar mod;

  if (self->is_recent)
    mod = self->modifier;
  else
    mod = modifier;

  codes = g_variant_get_child_value (self->data, 0);
  for (int i = 0; i < g_variant_n_children (codes); i++)
    {
      gunichar code;

      g_variant_get_child (codes, i, "u", &code);
      if (code == 0)
        code = mod;
      if (code != 0)
        p += g_unichar_to_utf8 (code, p);
    }
  g_variant_unref (codes);
  p += g_unichar_to_utf8 (0xFE0F, p); /* U+FE0F is the Emoji variation selector */
  p[0] = 0;
}

GtkEmojiGroup
gtk_emoji_object_get_group (GtkEmojiObject *self)
{
  g_return_val_if_fail (GTK_IS_EMOJI_OBJECT (self), GTK_EMOJI_GROUP_SMILEYS);

  if (self->is_recent)
    {
      return GTK_EMOJI_GROUP_RECENT;
    }
  else
    {
      GtkEmojiGroup group;

      g_variant_get_child (self->data, 3, "u", &group);

      return group + 1;
    }
}

const char *
gtk_emoji_object_get_name (GtkEmojiObject *self)
{
  const char *name;

  g_return_val_if_fail (GTK_IS_EMOJI_OBJECT (self), NULL);

  g_variant_get_child (self->data, 1, "s", &name);

  return name;
}

const char **
gtk_emoji_object_get_keywords (GtkEmojiObject *self)
{
  const char **keywords;

  g_return_val_if_fail (GTK_IS_EMOJI_OBJECT (self), NULL);

  g_variant_get_child (self->data, 2, "^a&s", &keywords);

  return keywords;
}

struct _GtkEmojiList
{
  GObject parent_instance;

  GVariant *data;
  Objects items;

  int section[GTK_EMOJI_GROUP_FLAGS + 1];
};

struct _GtkEmojiListClass
{
  GObjectClass parent_class;
};

static GType
gtk_emoji_list_get_item_type (GListModel *list)
{
  return G_TYPE_OBJECT;
}

static guint
gtk_emoji_list_get_n_items (GListModel *list)
{
  GtkEmojiList *self = GTK_EMOJI_LIST (list);

  return objects_get_size (&self->items);
}

static gpointer
gtk_emoji_list_get_item (GListModel *list,
                          guint       position)
{
  GtkEmojiList *self = GTK_EMOJI_LIST (list);

  if (position >= objects_get_size (&self->items))
    return NULL;

  return g_object_ref (objects_get (&self->items, position));
}

static void
gtk_emoji_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_emoji_list_get_item_type;
  iface->get_n_items = gtk_emoji_list_get_n_items;
  iface->get_item = gtk_emoji_list_get_item;
}

static void
gtk_emoji_list_get_section (GtkSectionModel *model,
                            guint            position,
                            guint           *out_start,
                            guint           *out_end)
{
  GtkEmojiList *self = GTK_EMOJI_LIST (model);
  GtkEmojiObject *obj;
  GtkEmojiGroup group;

  if (objects_get_size (&self->items) <= position)
    {
      *out_start = objects_get_size (&self->items);
      *out_end = G_MAXUINT;
      return;
    }

  obj = objects_get (&self->items, position);
  group = gtk_emoji_object_get_group (obj);

  *out_end = self->section[group];
  if (group > 0)
    *out_start = self->section[group - 1];
  else
    *out_start = 0;

  g_assert (*out_start <= position);
  g_assert (position <= *out_end);
}

static void
gtk_emoji_list_section_model_init (GtkSectionModelInterface *iface)
{
  iface->get_section = gtk_emoji_list_get_section;
}

G_DEFINE_TYPE_WITH_CODE (GtkEmojiList, gtk_emoji_list, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                                gtk_emoji_list_model_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SECTION_MODEL,
                                                gtk_emoji_list_section_model_init))


static void
gtk_emoji_list_dispose (GObject *object)
{
  GtkEmojiList *self = GTK_EMOJI_LIST (object);

  objects_clear (&self->items);
  g_variant_unref (self->data);

  G_OBJECT_CLASS (gtk_emoji_list_parent_class)->dispose (object);
}

static void
gtk_emoji_list_class_init (GtkEmojiListClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->dispose = gtk_emoji_list_dispose;
}

static void
gtk_emoji_list_init (GtkEmojiList *self)
{
  objects_init (&self->items);
}

static gboolean
has_emoji_coverage (GtkEmojiObject *emoji)
{
  PangoContext *context;
  PangoLayout *layout;
  char buffer[64];
  gboolean ret;

  context = pango_font_map_create_context (pango_cairo_font_map_get_default ());
  layout = pango_layout_new (context);

  gtk_emoji_object_get_text (emoji, buffer, sizeof (buffer), 0);
  pango_layout_set_text (layout, buffer, -1);
  ret = pango_layout_get_unknown_glyphs_count (layout) == 0;

  g_object_unref (layout);
  g_object_unref (context);

  return ret;
}

static void
gtk_emoji_list_populate_recent (GtkEmojiList *self)
{
  GSettings *settings;
  GVariant *variant;
  GVariant *item;
  GVariantIter iter;
  int pos;

  settings = g_settings_new ("org.gtk.gtk4.Settings.EmojiChooser");
  variant = g_settings_get_value (settings, "recent-emoji");

  pos = objects_get_size (&self->items);

  g_variant_iter_init (&iter, variant);
  while ((item = g_variant_iter_next_value (&iter)))
    {
      GVariant *emoji_data;
      gunichar modifier;
      GtkEmojiObject *emoji;

      emoji_data = g_variant_get_child_value (item, 0);
      g_variant_get_child (item, 1, "u", &modifier);

      emoji = gtk_emoji_object_new (emoji_data, TRUE, modifier);

      if (has_emoji_coverage (emoji))
        {
          GtkEmojiGroup group;

          pos++;

          group = gtk_emoji_object_get_group (emoji);
          self->section[group] = MAX (self->section[group], pos);

          objects_append (&self->items, emoji);
        }
      else
        g_object_unref (emoji);

      g_variant_unref (emoji_data);
      g_variant_unref (item);
    }

  g_variant_unref (variant);
  g_object_unref (settings);
}

static void
gtk_emoji_list_populate_data (GtkEmojiList *self)
{
  GBytes *bytes;
  GVariantIter *iter;
  GVariant *item;
  int pos;

  bytes = get_emoji_data ();
  self->data = g_variant_ref_sink (g_variant_new_from_bytes (G_VARIANT_TYPE ("a(ausasu)"), bytes, TRUE));
  g_bytes_unref (bytes);

  pos = objects_get_size (&self->items);

  iter = g_variant_iter_new (self->data);
  while ((item = g_variant_iter_next_value (iter)))
    {
      GtkEmojiObject *emoji = gtk_emoji_object_new (item, FALSE, 0);

      if (has_emoji_coverage (emoji))
        {
          GtkEmojiGroup group;

          pos++;

          group = gtk_emoji_object_get_group (emoji);
          self->section[group] = MAX (self->section[group], pos);

          objects_append (&self->items, emoji);
        }
      else
        g_object_unref (emoji);

      g_variant_unref (item);
    }
}

/**
 * gtk_emoji_list_new:
 *
 * Creates a new `GtkEmojiList` with the given @emojis.
 *
 * Returns: a new `GtkEmojiList`
 */
GtkEmojiList *
gtk_emoji_list_new (void)
{
  GtkEmojiList *self;

  self = g_object_new (GTK_TYPE_EMOJI_LIST, NULL);

  gtk_emoji_list_populate_recent (self);
  gtk_emoji_list_populate_data (self);

  return self;
}
