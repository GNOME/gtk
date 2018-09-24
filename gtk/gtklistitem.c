/*
 * Copyright © 2018 Benjamin Otte
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtklistitemprivate.h"

#include "gtkintl.h"

/**
 * SECTION:gtklistitem
 * @title: GtkListItem
 * @short_description: Widget used to represent items of a ListModel
 * @see_also: #GtkListView, #GListModel
 *
 * #GtkListItem is the widget that GTK list-handling containers such
 * as #GtkListView create to represent items in a #GListModel.  
 * They are managed by the container and cannot be created by application
 * code.
 *
 * #GtkListIems are container widgets that need to be populated by
 * application code. The container provides functions to do that.
 *
 * #GtkListItems exist in 2 stages:
 *
 * 1. The unbound stage where the listitem is not currently connected to
 *    an item in the list. In that case, the GtkListItem:item property is
 *    set to %NULL.
 *
 * 2. The bound stage where the listitem references an item from the list.
 *    The GtkListItem:item property is not %NULL.
 */

struct _GtkListItem
{
  GtkBin parent_instance;

  GObject *item;
  guint position;
};

enum
{
  PROP_0,
  PROP_ITEM,
  PROP_POSITION,

  N_PROPS
};

G_DEFINE_TYPE (GtkListItem, gtk_list_item, GTK_TYPE_BIN)

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gtk_list_item_dispose (GObject *object)
{
  GtkListItem *self = GTK_LIST_ITEM (object);

  g_assert (self->item == NULL);

  G_OBJECT_CLASS (gtk_list_item_parent_class)->dispose (object);
}

static void
gtk_list_item_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkListItem *self = GTK_LIST_ITEM (object);

  switch (property_id)
    {
    case PROP_ITEM:
      g_value_set_object (value, self->item);
      break;

    case PROP_POSITION:
      g_value_set_uint (value, self->position);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_list_item_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  //GtkListItem *self = GTK_LIST_ITEM (object);

  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_list_item_class_init (GtkListItemClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gtk_list_item_dispose;
  gobject_class->get_property = gtk_list_item_get_property;
  gobject_class->set_property = gtk_list_item_set_property;

  /**
   * GtkListItem:item:
   *
   * Displayed item
   */
  properties[PROP_ITEM] =
    g_param_spec_object ("item",
                         P_("Item"),
                         P_("Displayed item"),
                         G_TYPE_OBJECT,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkListItem:position:
   *
   * Position in the item
   */
  properties[PROP_POSITION] =
    g_param_spec_uint ("position",
                       P_("Position"),
                       P_("Position of the item"),
                       0, G_MAXUINT, 0,
                       G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);

  /* This gets overwritten by gtk_list_item_new() but better safe than sorry */
  gtk_widget_class_set_css_name (widget_class, I_("row"));
}

static void
gtk_list_item_init (GtkListItem *self)
{
}

GtkWidget *
gtk_list_item_new (const char *css_name)
{
  g_return_val_if_fail (css_name != NULL, NULL);

  return g_object_new (GTK_TYPE_LIST_ITEM,
                       "css-name", css_name,
                       NULL);
}

/**
 * gtk_list_item_get_item:
 * @self: a #GtkListItem
 *
 * Gets the item that is currently displayed in model that @self is
 * currently bound to or %NULL if @self is unbound.
 *
 * Returns: (nullable) (transfer none) (type GObject): The item displayed
 **/
gpointer
gtk_list_item_get_item (GtkListItem *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM (self), NULL);

  return self->item;
}

/**
 * gtk_list_item_get_position:
 * @self: a #GtkListItem
 *
 * Gets the position in the model that @self currently displays.
 * If @self is unbound, 0 is returned.
 *
 * Returns: The position of this item
 **/
guint
gtk_list_item_get_position (GtkListItem *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM (self), 0);

  return self->position;
}

void
gtk_list_item_set_item (GtkListItem *self,
                        gpointer     item)
{
  g_return_if_fail (GTK_IS_LIST_ITEM (self));
  g_return_if_fail (item == NULL || G_IS_OBJECT (item));

  if (self->item == item)
    return;

  g_clear_object (&self->item);
  if (item)
    self->item = g_object_ref (item);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ITEM]);
}

void
gtk_list_item_set_position (GtkListItem *self,
                            guint        position)
{
  g_return_if_fail (GTK_IS_LIST_ITEM (self));

  if (self->position == position)
    return;

  self->position = position;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_POSITION]);
}

