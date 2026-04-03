/*
 * Copyright (C) 2018-2020 Purism SPC
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gtkenumlist.h"

#include <gio/gio.h>
#include <gtk/gtk.h>

/**
 * GtkEnumList:
 *
 * A [iface@Gio.ListModel] representing values of a given enum.
 *
 * `GtkEnumList` contains objects of type [class@Gtk.EnumListItem].
 *
 * A simple way to use a `GtkEnumList` is to populate a [class@Gtk.DropDown]
 * widget using the short name (or "nick") of the values of an
 * enumeration type:
 *
 * ```c
 * choices = gtk_drop_down_new (G_LIST_MODEL (gtk_enum_list_new (type)),
 *                              gtk_property_expression_new (GTK_TYPE_ENUM_LIST_ITEM,
 *                                                           NULL,
 *                                                           "nick"));
 * ```
 *
 * Since: 4.24
 */

struct _GtkEnumList
{
  GObject parent_instance;

  GType enum_type;
  GEnumClass *enum_class;

  GtkEnumListItem **objects;
};

enum {
  PROP_0,
  PROP_ENUM_TYPE,
  PROP_ITEM_TYPE,
  PROP_N_ITEMS,
  LAST_PROP,
};

static GParamSpec *props[LAST_PROP];

static void gtk_enum_list_iface_init (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GtkEnumList, gtk_enum_list, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_enum_list_iface_init))

/**
 * GtkEnumListItem:
 *
 * `GtkEnumListItem` is the type of items in a [class@Gtk.EnumList].
 *
 * Since: 4.24
 */

struct _GtkEnumListItem
{
  GObject parent_instance;

  GEnumValue enum_value;
};

enum {
  VALUE_PROP_0,
  VALUE_PROP_VALUE,
  VALUE_PROP_NAME,
  VALUE_PROP_NICK,
  LAST_VALUE_PROP,
};

static GParamSpec *value_props[LAST_VALUE_PROP];

G_DEFINE_FINAL_TYPE (GtkEnumListItem, gtk_enum_list_item, G_TYPE_OBJECT)

static void
gtk_enum_list_item_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GtkEnumListItem *self = GTK_ENUM_LIST_ITEM (object);

  switch (prop_id)
    {
    case VALUE_PROP_VALUE:
      g_value_set_int (value, gtk_enum_list_item_get_value (self));
      break;
    case VALUE_PROP_NAME:
      g_value_set_string (value, gtk_enum_list_item_get_name (self));
      break;
    case VALUE_PROP_NICK:
      g_value_set_string (value, gtk_enum_list_item_get_nick (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_enum_list_item_class_init (GtkEnumListItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gtk_enum_list_item_get_property;

  /**
   * GtkEnumListItem:value:
   *
   * The enum value.
   *
   * Since: 4.24
   */
  value_props[VALUE_PROP_VALUE] =
    g_param_spec_int ("value", NULL, NULL,
                      G_MININT, G_MAXINT, 0,
                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GtkEnumListItem:name:
   *
   * The enum value name.
   *
   * Since: 4.24
   */
  value_props[VALUE_PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GtkEnumListItem:nick:
   *
   * The enum value nick.
   *
   * Since: 4.24
   */
  value_props[VALUE_PROP_NICK] =
    g_param_spec_string ("nick", NULL, NULL,
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_VALUE_PROP, value_props);
}

static void
gtk_enum_list_item_init (GtkEnumListItem *self)
{
}

static GtkEnumListItem *
gtk_enum_list_item_new (GEnumValue *enum_value)
{
  GtkEnumListItem *self = g_object_new (GTK_TYPE_ENUM_LIST_ITEM, NULL);

  self->enum_value = *enum_value;

  return self;
}

/**
 * gtk_enum_list_item_get_value:
 *
 * Gets the enum value.
 *
 * Returns: the enum value
 */
int
gtk_enum_list_item_get_value (GtkEnumListItem *self)
{
  g_return_val_if_fail (GTK_IS_ENUM_LIST_ITEM (self), 0);

  return self->enum_value.value;
}

/**
 * gtk_enum_list_item_get_name:
 *
 * Gets the enum value name.
 *
 * Returns: the enum value name
 */
const char *
gtk_enum_list_item_get_name (GtkEnumListItem *self)
{
  g_return_val_if_fail (GTK_IS_ENUM_LIST_ITEM (self), NULL);

  return self->enum_value.value_name;
}

/**
 * gtk_enum_list_item_get_nick:
 *
 * Gets the enum value nick.
 *
 * Returns: the enum value nick
 */
const char *
gtk_enum_list_item_get_nick (GtkEnumListItem *self)
{
  g_return_val_if_fail (GTK_IS_ENUM_LIST_ITEM (self), NULL);

  return self->enum_value.value_nick;
}

static void
gtk_enum_list_constructed (GObject *object)
{
  GtkEnumList *self = GTK_ENUM_LIST (object);
  guint i;

  self->enum_class = g_type_class_get (self->enum_type);

  self->objects = g_new (GtkEnumListItem *, self->enum_class->n_values);

  for (i = 0; i < self->enum_class->n_values; i++)
    self->objects[i] = gtk_enum_list_item_new (&self->enum_class->values[i]);

  G_OBJECT_CLASS (gtk_enum_list_parent_class)->constructed (object);
}

static void
gtk_enum_list_finalize (GObject *object)
{
  GtkEnumList *self = GTK_ENUM_LIST (object);

  for (unsigned int i = 0; i < self->enum_class->n_values; i++)
    g_object_unref (self->objects[i]);

  g_clear_pointer (&self->objects, g_free);

  G_OBJECT_CLASS (gtk_enum_list_parent_class)->finalize (object);
}

static void
gtk_enum_list_get_property (GObject      *object,
                            unsigned int  prop_id,
                            GValue       *value,
                            GParamSpec   *pspec)
{
  GtkEnumList *self = GTK_ENUM_LIST (object);

  switch (prop_id)
    {
    case PROP_ENUM_TYPE:
      g_value_set_gtype (value, self->enum_type);
      break;
    case PROP_ITEM_TYPE:
      g_value_set_gtype (value, GTK_TYPE_ENUM_LIST_ITEM);
      break;
    case PROP_N_ITEMS:
      g_value_set_uint (value, self->enum_class->n_values);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_enum_list_set_property (GObject      *object,
                            unsigned int  prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkEnumList *self = GTK_ENUM_LIST (object);

  switch (prop_id)
    {
    case PROP_ENUM_TYPE:
      self->enum_type = g_value_get_gtype (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_enum_list_class_init (GtkEnumListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gtk_enum_list_constructed;
  object_class->finalize = gtk_enum_list_finalize;
  object_class->get_property = gtk_enum_list_get_property;
  object_class->set_property = gtk_enum_list_set_property;

  /**
   * GtkEnumList:enum-type:
   *
   * The type of the enum represented by the model.
   *
   * Since: 4.24
   */
  props[PROP_ENUM_TYPE] =
    g_param_spec_gtype ("enum-type", NULL, NULL,
                        G_TYPE_ENUM,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkEnumList:item-type:
   *
   * The type of the items. See [method@Gio.ListModel.get_item_type].
   *
   * Since: 4.24
   */
  props[PROP_ITEM_TYPE] =
    g_param_spec_gtype ("item-type", NULL, NULL,
                        GTK_TYPE_ENUM_LIST_ITEM,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GtkEnumList:n-items:
   *
   * The number of items. See [method@Gio.ListModel.get_n_items].
   *
   * Since: 4.24
   */
  props[PROP_N_ITEMS] =
    g_param_spec_uint ("n-items", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
gtk_enum_list_init (GtkEnumList *self)
{
}

static GType
gtk_enum_list_get_item_type (GListModel *list)
{
  return GTK_TYPE_ENUM_LIST_ITEM;
}

static guint
gtk_enum_list_get_n_items (GListModel *list)
{
  GtkEnumList *self = GTK_ENUM_LIST (list);

  return self->enum_class->n_values;
}

static gpointer
gtk_enum_list_get_item (GListModel *list,
                        guint       position)
{
  GtkEnumList *self = GTK_ENUM_LIST (list);

  if (position < self->enum_class->n_values)
    return g_object_ref (self->objects[position]);

  return NULL;
}

static void
gtk_enum_list_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_enum_list_get_item_type;
  iface->get_n_items = gtk_enum_list_get_n_items;
  iface->get_item = gtk_enum_list_get_item;
}

/**
 * gtk_enum_list_new:
 * @enum_type: the type of the enum to construct the model from
 *
 * Creates a new `GtkEnumList` for @enum_type.
 *
 * Returns: the newly created `GtkEnumList`
 *
 * Since: 4.24
 */
GtkEnumList *
gtk_enum_list_new (GType enum_type)
{
  g_return_val_if_fail (G_TYPE_IS_ENUM (enum_type), NULL);

  return g_object_new (GTK_TYPE_ENUM_LIST,
                       "enum-type", enum_type,
                       NULL);
}

/**
 * gtk_enum_list_get_enum_type:
 *
 * Gets the type of the enum represented by @self.
 *
 * Returns: the enum type
 *
 * Since: 4.24
 */
GType
gtk_enum_list_get_enum_type (GtkEnumList *self)
{
  g_return_val_if_fail (GTK_IS_ENUM_LIST (self), G_TYPE_INVALID);

  return self->enum_type;
}

/**
 * gtk_enum_list_find:
 * @value: an enum value
 *
 * Finds the position of a given enum value in @self.
 *
 * If the value is not found, [const@Gtk.INVALID_LIST_POSITION] is returned.
 *
 * Returns: the position of the value
 *
 * Since: 4.24
 */
unsigned int
gtk_enum_list_find (GtkEnumList *self,
                    int          value)
{
  g_return_val_if_fail (GTK_IS_ENUM_LIST (self), GTK_INVALID_LIST_POSITION);

  for (unsigned int i = 0; i < self->enum_class->n_values; i++)
    {
      if (self->enum_class->values[i].value == value)
        return i;
    }

  return GTK_INVALID_LIST_POSITION;
}
