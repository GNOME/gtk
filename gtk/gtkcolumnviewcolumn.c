/*
 * Copyright © 2019 Benjamin Otte
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

#include "gtkcolumnviewcolumnprivate.h"

#include "gtkintl.h"
#include "gtklistbaseprivate.h"
#include "gtklistitemmanagerprivate.h"
#include "gtkmain.h"
#include "gtkprivate.h"
#include "gtkrbtreeprivate.h"
#include "gtkstylecontext.h"
#include "gtkwidgetprivate.h"

/**
 * SECTION:gtkcolumnviewcolumn
 * @title: GtkColumnViewColumn
 * @short_description: The column added to GtkColumnView
 * @see_also: #GtkColumnView
 *
 * GtkColumnViewColumn represents the columns being added to #GtkColumnView.
 */

struct _GtkColumnViewColumn
{
  GObject parent_instance;

  GtkListItemFactory *factory;
  char *title;

  /* data for the view */
  GtkColumnView *view;
};

struct _GtkColumnViewColumnClass
{
  GObjectClass parent_class;
};

enum
{
  PROP_0,
  PROP_COLUMN_VIEW,
  PROP_FACTORY,
  PROP_TITLE,

  N_PROPS
};

G_DEFINE_TYPE (GtkColumnViewColumn, gtk_column_view_column, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gtk_column_view_column_dispose (GObject *object)
{
  GtkColumnViewColumn *self = GTK_COLUMN_VIEW_COLUMN (object);

  g_assert (self->view == NULL); /* would hold a ref otherwise */
  g_clear_object (&self->factory);
  g_clear_pointer (&self->title, g_free);

  G_OBJECT_CLASS (gtk_column_view_column_parent_class)->dispose (object);
}

static void
gtk_column_view_column_get_property (GObject    *object,
                                     guint       property_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GtkColumnViewColumn *self = GTK_COLUMN_VIEW_COLUMN (object);

  switch (property_id)
    {
    case PROP_COLUMN_VIEW:
      g_value_set_object (value, self->view);
      break;

    case PROP_FACTORY:
      g_value_set_object (value, self->factory);
      break;

    case PROP_TITLE:
      g_value_set_string (value, self->title);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_column_view_column_set_property (GObject      *object,
                                     guint         property_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkColumnViewColumn *self = GTK_COLUMN_VIEW_COLUMN (object);

  switch (property_id)
    {
    case PROP_FACTORY:
      gtk_column_view_column_set_factory (self, g_value_get_object (value));
      break;

    case PROP_TITLE:
      gtk_column_view_column_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_column_view_column_class_init (GtkColumnViewColumnClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gtk_column_view_column_dispose;
  gobject_class->get_property = gtk_column_view_column_get_property;
  gobject_class->set_property = gtk_column_view_column_set_property;

  /**
   * GtkColumnViewColumn:column-view:
   *
   * #GtkColumnView this column is a part of
   */
  properties[PROP_COLUMN_VIEW] =
    g_param_spec_object ("column-view",
                         P_("Column view"),
                         P_("Column view this column is a part of"),
                         GTK_TYPE_COLUMN_VIEW,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkColumnViewColumn:factory:
   *
   * Factory for populating list items
   */
  properties[PROP_FACTORY] =
    g_param_spec_object ("factory",
                         P_("Factory"),
                         P_("Factory for populating list items"),
                         GTK_TYPE_LIST_ITEM_FACTORY,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkColumnViewColumn:title:
   *
   * Title displayed in the header
   */
  properties[PROP_TITLE] =
    g_param_spec_string ("title",
                          P_("Title"),
                          P_("Title displayed in the header"),
                          NULL,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gtk_column_view_column_init (GtkColumnViewColumn *self)
{
}

/**
 * gtk_column_view_column_new:
 * @title: (nullable): Title to use for this column
 *
 * Creates a new #GtkColumnViewColumn.
 *
 * You most likely want to call gtk_column_add_column() next.
 *
 * Returns: a new #GtkColumnViewColumn
 **/
GtkColumnViewColumn *
gtk_column_view_column_new (const char *title)
{
  return g_object_new (GTK_TYPE_COLUMN_VIEW_COLUMN,
                       "title", title,
                       NULL);
}

/**
 * gtk_column_view_column_new_with_factory:
 * @title: (nullable): Title to use for this column
 * @factory: (transfer full): The factory to populate items with
 *
 * Creates a new #GtkColumnViewColumn that uses the given @factory for
 * mapping items to widgets.
 *
 * You most likely want to call gtk_column_add_column() next.
 *
 * The function takes ownership of the
 * argument, so you can write code like
 * ```
 *   column = gtk_column_view_column_new_with_factory (_("Name"),
 *     gtk_builder_list_item_factory_new_from_resource ("/name.ui"));
 * ```
 *
 * Returns: a new #GtkColumnViewColumn using the given @factory
 **/
GtkColumnViewColumn *
gtk_column_view_column_new_with_factory (const char         *title,
                                         GtkListItemFactory *factory)
{
  GtkColumnViewColumn *result;

  g_return_val_if_fail (GTK_IS_LIST_ITEM_FACTORY (factory), NULL);

  result = g_object_new (GTK_TYPE_COLUMN_VIEW_COLUMN,
                         "factory", factory,
                         "title", title,
                         NULL);

  g_object_unref (factory);

  return result;
}

/**
 * gtk_column_view_column_get_column_view:
 * @self: a #GtkColumnViewColumn
 *
 * Gets the column view that's currently displaying this column.
 *
 * If @self has not been added to a column view yet, %NULL is returned.
 *
 * Returns: (nullable) (transfer none): The column view displaying @self.
 **/
GtkColumnView *
gtk_column_view_column_get_column_view (GtkColumnViewColumn *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (self), NULL);

  return self->view;
}

void
gtk_column_view_column_set_column_view (GtkColumnViewColumn *self,
                                        GtkColumnView       *view)
{
  if (self->view == view)
    return;

  self->view = view;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COLUMN_VIEW]);
}

/**
 * gtk_column_view_column_get_factory:
 * @self: a #GtkColumnViewColumn
 *
 * Gets the factory that's currently used to populate list items for
 * this column.
 *
 * Returns: (nullable) (transfer none): The factory in use
 **/
GtkListItemFactory *
gtk_column_view_column_get_factory (GtkColumnViewColumn *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (self), NULL);

  return self->factory;
}

/**
 * gtk_column_view_column_set_factory:
 * @self: a #GtkColumnViewColumn
 * @factory: (allow-none) (transfer none): the factory to use or %NULL for none
 *
 * Sets the #GtkListItemFactory to use for populating list items for this
 * column.
 **/
void
gtk_column_view_column_set_factory (GtkColumnViewColumn *self,
                                    GtkListItemFactory  *factory)
{
  g_return_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (self));
  g_return_if_fail (factory == NULL || GTK_LIST_ITEM_FACTORY (factory));

  if (!g_set_object (&self->factory, factory))
    return;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FACTORY]);
}

/**
 * gtk_column_view_column_set_title:
 * @self: a #GtkColumnViewColumn
 * @title: (nullable): Title to use for this column
 *
 * Sets the title of this column. The title is displayed in the header of a
 * #GtkColumnView for this column and is therefor user-facing text that should
 * be translated.
 */
void
gtk_column_view_column_set_title (GtkColumnViewColumn *self,
                                  const char          *title)
{
  g_return_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (self));

  if (g_strcmp0 (self->title, title) == 0)
    return;

  g_free (self->title);
  self->title = g_strdup (title);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);
}

/**
 * gtk_column_view_column_get_title:
 * @self: a #GtkColumnViewColumn
 *
 * Returns the title set with gtk_column_view_column_set_title().
 *
 * Returns: (nullable) The column's title
 */
const char *
gtk_column_view_column_get_title (GtkColumnViewColumn *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (self), FALSE);

  return self->title;
}
